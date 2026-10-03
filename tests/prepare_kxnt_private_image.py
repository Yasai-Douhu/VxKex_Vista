"""Retarget ntdll imports in an owned diagnostic EXE copy, never the original."""
import argparse
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.destination.resolve():
        raise ValueError('Diagnostic copy must have a separate path')
    data = bytearray(args.source.read_bytes())
    u16 = lambda p: struct.unpack_from('<H', data, p)[0]
    u32 = lambda p: struct.unpack_from('<I', data, p)[0]
    pe = u32(60)
    if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE image')
    opt = pe + 24
    magic = u16(opt)
    if magic not in (0x10b, 0x20b):
        raise ValueError('Unsupported image format')
    table = opt + u16(pe + 20)

    def offset(rva):
        for i in range(u16(pe + 6)):
            row = table + i * 40
            size, address, raw_size, raw = struct.unpack_from('<IIII', data, row + 8)
            if address <= rva < address + min(size, raw_size):
                return raw + rva - address
        raise ValueError('Import RVA outside file-backed sections')

    directory = opt + (112 if magic == 0x20b else 96)
    imports, size = struct.unpack_from('<II', data, directory + 8)
    replaced = 0
    for rva in range(imports, imports + size, 20):
        row = offset(rva)
        name_rva = u32(row + 12)
        if not name_rva:
            break
        name = offset(name_rva)
        end = data.index(0, name)
        if data[name:end].lower() == b'ntdll.dll':
            data[name:end + 1] = b'KxNt.dll\0\0'
            replaced += 1
    if replaced != 1:
        raise ValueError(f'Expected exactly one ntdll descriptor, found {replaced}')
    args.destination.write_bytes(data)
    print('Private copy prepared: one ntdll import descriptor redirected')


if __name__ == '__main__':
    main()
