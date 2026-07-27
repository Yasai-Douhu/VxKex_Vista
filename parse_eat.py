#!/usr/bin/env python3
"""Parse PE export address table and identify forwarded exports."""

import struct
import sys

def parse_pe_exports(filepath):
    """Parse PE file and return exported functions with forwarder information."""
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Check DOS header
    if data[0:2] != b'MZ':
        print(f"  ERROR: Not a PE file (no MZ signature)")
        return [], []
    
    pe_offset = struct.unpack_from('<I', data, 0x3C)[0]
    
    # Check PE signature
    if data[pe_offset:pe_offset+4] != b'PE\x00\x00':
        print(f"  ERROR: Invalid PE signature at offset {pe_offset}")
        return [], []
    
    machine = struct.unpack_from('<H', data, pe_offset + 4)[0]
    num_sections = struct.unpack_from('<H', data, pe_offset + 6)[0]
    time_stamp = struct.unpack_from('<I', data, pe_offset + 8)[0]
    ptr_symbol_table = struct.unpack_from('<I', data, pe_offset + 12)[0]
    num_symbols = struct.unpack_from('<I', data, pe_offset + 16)[0]
    optional_header_size = struct.unpack_from('<H', data, pe_offset + 20)[0]
    image_base = struct.unpack_from('<Q', data, pe_offset + 24)[0]
    characteristics = struct.unpack_from('<H', data, pe_offset + 0x16)[0]
    
    # Check magic for PE32 vs PE32+
    magic_offset = pe_offset + 20
    magic = struct.unpack_from('<H', data, magic_offset)[0]
    
    if magic == 0x10b:  # PE32 (32-bit)
        is_64 = False
        image_base = struct.unpack_from('<I', data, magic_offset + 28)[0]
        print(f"  Architecture: x86 (PE32)")
    elif magic == 0x20b:  # PE32+ (64-bit)
        is_64 = True
        print(f"  Architecture: x64 (PE32+)")
    else:
        print(f"  ERROR: Unknown magic {hex(magic)}")
        return [], []
    
    # Data directory starts at different offsets based on PE type
    # PE32: Data directories start at offset 0x60 from optional header start
    # PE32+: Data directories start at offset 0x70 from optional header start
    # Number of data directories is at offset 0x74 (PE32) or 0x84 (PE32+)
    
    if is_64:
        data_dirs_start = magic_offset + 0x70
        num_dirs_offset = magic_offset + 0x84
    else:
        data_dirs_start = magic_offset + 0x60
        num_dirs_offset = magic_offset + 0x74
    
    num_data_dirs = struct.unpack_from('<H', data, num_dirs_offset)[0]
    print(f"  Optional Header Size: {optional_header_size}, Num Data Dirs: {num_data_dirs}")
    print(f"  Data dirs at: {data_dirs_start}, Image Base: {hex(image_base)}")
    
    if num_data_dirs < 10:
        print(f"  ERROR: Not enough data directories ({num_data_dirs})")
        return [], []
    
    # Export directory is entry #0 (index 0)
    exp_rva = struct.unpack_from('<I', data, data_dirs_start)[0]
    exp_size = struct.unpack_from('<I', data, data_dirs_start + 4)[0]
    
    print(f"  Export Directory: RVA={hex(exp_rva)}, Size={exp_size}")
    
    if exp_rva == 0 or exp_size == 0:
        print(f"  ERROR: No export directory")
        return [], []
    
    # Find section headers (after optional header)
    section_header_offset = pe_offset + 20 + optional_header_size
    
    # Build section info
    sections = []
    for i in range(num_sections):
        sec_offset = section_header_offset + i * 40
        sec_name = data[sec_offset:sec_offset+8].strip(b'\x00').decode('ascii', errors='replace')
        sec_vaddr = struct.unpack_from('<I', data, sec_offset + 0x0C)[0]
        sec_size = struct.unpack_from('<I', data, sec_offset + 0x08)[0]
        sec_rawsize = struct.unpack_from('<I', data, sec_offset + 0x08)[0]
        sec_rawoffset = struct.unpack_from('<I', data, sec_offset + 0x08)[0] if i == 0 else struct.unpack_from('<I', data, sec_offset + 0x14)[0]
        sec_rawoffset = struct.unpack_from('<I', data, sec_offset + 0x14)[0]
        sections.append({
            'name': sec_name,
            'vaddr': sec_vaddr,
            'size': sec_size,
            'rawoffset': sec_rawoffset
        })
    
    def rva_to_offset(rva):
        for sec in sections:
            if sec['vaddr'] <= rva < sec['vaddr'] + sec['size']:
                return rva - sec['vaddr'] + sec['rawoffset']
        return -1
    
    # Parse export directory
    exp_file_offset = rva_to_offset(exp_rva)
    if exp_file_offset < 0:
        print(f"  ERROR: Could not find export directory in sections")
        return [], []
    
    exp_data = data[exp_file_offset:exp_file_offset+40]
    if len(exp_data) < 40:
        print(f"  ERROR: Export directory data truncated")
        return [], []
    
    NumberOfFunctions = struct.unpack_from('<I', exp_data, 20)[0]
    NumberOfNames = struct.unpack_from('<I', exp_data, 24)[0]
    AddressTableRVA = struct.unpack_from('<I', exp_data, 28)[0]
    NamePtrRVA = struct.unpack_from('<I', exp_data, 32)[0]
    OrdinalTableRVA = struct.unpack_from('<I', exp_data, 36)[0]
    
    print(f"  NumberOfFunctions: {NumberOfFunctions}, NumberOfNames: {NumberOfNames}")
    print(f"  AddressTableRVA: {hex(AddressTableRVA)}, NamePtrRVA: {hex(NamePtrRVA)}")
    print(f"  OrdinalTableRVA: {hex(OrdinalTableRVA)}")
    
    # Convert RVAs to file offsets
    address_table_offset = rva_to_offset(AddressTableRVA)
    name_ptr_offset = rva_to_offset(NamePtrRVA)
    ordinal_table_offset = rva_to_offset(OrdinalTableRVA)
    
    if address_table_offset < 0 or name_ptr_offset < 0 or ordinal_table_offset < 0:
        print(f"  ERROR: Could not convert function table RVAs to offsets")
        return [], []
    
    exports = []
    forwarders = []
    
    for i in range(NumberOfNames):
        # Get export name
        name_rva = struct.unpack_from('<I', data, name_ptr_offset + i * 4)[0]
        name_offset = rva_to_offset(name_rva)
        if name_offset < 0 or name_offset >= len(data):
            continue
        name = data[name_offset:].split(b'\x00')[0].decode('utf-8', errors='replace')
        
        # Get ordinal
        ordinal = struct.unpack_from('<H', data, ordinal_table_offset + i * 2)[0]
        
        # Get function RVA (using ordinal as index into address table)
        if ordinal * 4 + 4 > len(data) - address_table_offset:
            continue
        func_rva = struct.unpack_from('<I', data, address_table_offset + ordinal * 4)[0]
        
        # Check if it's a forward by reading the target as string
        func_offset = rva_to_offset(func_rva)
        if 0 <= func_offset < len(data) - 10:
            func_text = data[func_offset:func_offset+200].split(b'\x00')[0].decode('utf-8', errors='replace')
            
            # Check if it looks like a forwarder (contains DLL.function pattern)
            if '.' in func_text and len(func_text) > 10:
                # Verify it's actually a string by checking for common DLL names
                if any(dll in func_text for dll in ['kernel32.dll', 'kernelbase.dll', 'advapi32.dll', 
                                                      'user32.dll', 'gdi32.dll', 'ntdll.dll', 
                                                      'msvcrt.dll', 'api-ms-win']):
                    exports.append({
                        'name': name, 
                        'rva': hex(func_rva), 
                        'forward': func_text, 
                        'type': 'forward'
                    })
                    forwarders.append({'name': name, 'forward': func_text})
                    continue
        
        exports.append({
            'name': name, 
            'rva': hex(func_rva), 
            'type': 'code'
        })
    
    return exports, forwarders

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <dll_file> [dll_file2 ...]")
        sys.exit(1)
    
    for dll_path in sys.argv[1:]:
        print(f"\n{'='*80}")
        print(f"Analyzing: {dll_path}")
        print(f"{'='*80}")
        
        exports, forwarders = parse_pe_exports(dll_path)
        
        print(f"\nTotal exports: {len(exports)}")
        print(f"Forwarded exports: {len(forwarders)}")
        
        if forwarders:
            print(f"\n--- Forwarded Exports (sorted by target DLL) ---")
            for exp in sorted(forwarders, key=lambda x: x.get('forward', '')):
                print(f"  {exp['name']} -> {exp.get('forward', 'N/A')}")

if __name__ == '__main__':
    main()
