#!/usr/bin/env python3
"""Parse PE headers correctly according to PE/COFF specification."""
import struct
filepath = '/mnt/c/Users/YamaR/Desktop/AI_Datas/VxKex_Vista/SystemFiles/vista/kernel32.dll'
with open(filepath, 'rb') as f:
    data = f.read(1024)

pe_offset = struct.unpack_from('<I', data, 0x3C)[0]
print(f'PE offset: {pe_offset} (0x{pe_offset:X})')

# COFF Header starts at PE+0x04 (after "PE\0\0")
# SizeOfCOFFHeader = 20 bytes
print(f'\n--- COFF Header (20 bytes) ---')
machine = struct.unpack_from('<H', data, pe_offset + 4)[0]
optional_header_size = struct.unpack_from('<H', data, pe_offset + 20)[0]
characteristics = struct.unpack_from('<H', data, pe_offset + 22)[0]
num_sections = struct.unpack_from('<H', data, pe_offset + 6)[0]
print(f'Machine: 0x{machine:X} (0x8664=x64)')
print(f'NumberOfSections: {num_sections}')
print(f'SizeOfOptionalHeader: {optional_header_size}')
print(f'Characteristics: 0x{characteristics:X}')

# Optional Header starts at PE+0x18
opt_header_start = pe_offset + 0x18
print(f'\n--- Optional Header (starts at PE+0x18 = 0x{opt_header_start:X}) ---')

# Magic is at offset 0x00 from Optional Header start
magic = struct.unpack_from('<H', data, opt_header_start + 0)[0]
print(f'Magic at opt+0x00: 0x{magic:X} (0x10b=PE32, 0x20b=PE32+)')

# Windows-Specific Fields (PE32+)
image_base = struct.unpack_from('<Q', data, opt_header_start + 24)[0]
print(f'ImageBase (PE32+ at opt+0x18): 0x{image_base:X}')

# Data Directories
# According to PE spec for PE32+:
# Data directories start at opt+0x60
# NumberOfDirectoryEntries is at opt+0x70
num_data_dirs = struct.unpack_from('<H', data, opt_header_start + 0x70)[0]
data_dirs_start = opt_header_start + 0x60
print(f'\n--- Data Directories ---')
print(f'NumberOfDirectoryEntries at opt+0x70: {num_data_dirs}')
print(f'Data directories at opt+0x60 = PE+0x{data_dirs_start - pe_offset:X}')

# Export Directory (Directory Entry 0)
exp_rva = struct.unpack_from('<I', data, data_dirs_start)[0]
exp_size = struct.unpack_from('<I', data, data_dirs_start + 4)[0]
print(f'Export Directory: RVA=0x{exp_rva:X}, Size={exp_size}')

# If export directory is not found, let's dump all data directory entries
if exp_rva == 0:
    print('\n--- All Data Directory Entries ---')
    for i in range(min(16, num_data_dirs)):
        entry_rva = struct.unpack_from('<I', data, data_dirs_start + i * 8)[0]
        entry_size = struct.unpack_from('<I', data, data_dirs_start + i * 8 + 4)[0]
        if entry_rva > 0:
            names = ['Export', 'Import', 'Resource', 'Exception', 'Security',
                     'BaseRelocation', 'Debug', 'Architecture', 'GlobalPtr', 'TLSP',
                     'LoadConfig', 'BoundImport', 'ImportAddress', 'DelayImport', 'COMDescriptor']
            name = names[i] if i < len(names) else f'Unknown{i}'
            print(f'  [{i:2d}] {name}: RVA=0x{entry_rva:X}, Size={entry_size}')
