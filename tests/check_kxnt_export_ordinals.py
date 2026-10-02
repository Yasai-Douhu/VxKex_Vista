"""Verify that adding KxNt/KexDll exports preserves released export ordinals."""
import argparse
import json
from pathlib import Path
import struct
import subprocess


def exports(data):
    u16 = lambda p: struct.unpack_from('<H', data, p)[0]
    u32 = lambda p: struct.unpack_from('<I', data, p)[0]
    pe = u32(60)
    if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Invalid PE image')
    opt = pe + 24
    section = opt + u16(pe + 20)
    sections = [(u32(section + i * 40 + 12),
                 max(u32(section + i * 40 + 8), u32(section + i * 40 + 16)),
                 u32(section + i * 40 + 20)) for i in range(u16(pe + 6))]

    def offset(rva):
        for address, size, raw in sections:
            if address <= rva < address + size:
                return raw + rva - address
        raise ValueError('Export RVA outside sections')

    magic = u16(opt)
    if magic not in (0x10b, 0x20b):
        raise ValueError('Unknown optional header')
    directory = opt + (112 if magic == 0x20b else 96)
    eat = offset(u32(directory))
    base, count = u32(eat + 16), u32(eat + 24)
    names, ordinals = offset(u32(eat + 32)), offset(u32(eat + 36))
    result = {}
    for i in range(count):
        start = offset(u32(names + 4 * i))
        name = data[start:data.index(b'\0', start)].decode('ascii')
        result[name] = base + u16(ordinals + 2 * i)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', default='24a03ae')
    parser.add_argument('--output', default='audit/KxNtParity/export-ordinals.json')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    results = []
    for relative in ('Installer/KxNt.dll', 'Installer/Kex32/KxNt.dll',
                     'Installer/KexDll.dll', 'Installer/Kex32/KexDll.dll'):
        baseline = exports(subprocess.check_output(
            ['git', 'show', args.base + ':' + relative], cwd=root))
        current = exports((root / relative).read_bytes())
        changed = {name: [old, current.get(name)] for name, old in baseline.items()
                   if current.get(name) != old}
        results.append({'Path': relative, 'BaselineNames': len(baseline),
                        'OrdinalChanges': changed,
                        'NewExports': {name: ordinal for name, ordinal in current.items()
                                       if name not in baseline}})
        print(relative, 'ordinal changes:', len(changed))
    output = root / args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({'BaseCommit': args.base, 'Results': results}, indent=2),
                      encoding='utf-8')
    return int(any(row['OrdinalChanges'] for row in results))


if __name__ == '__main__':
    raise SystemExit(main())
