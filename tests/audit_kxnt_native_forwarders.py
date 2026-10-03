"""Inventory unresolved native forwarders against explicit guest DLL snapshots.

This checks static link targets, not whether applications call or resolve them.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def exports(data):
    def u16(p): return struct.unpack_from('<H', data, p)[0]
    def u32(p): return struct.unpack_from('<I', data, p)[0]
    pe = u32(60)
    if data[:2] != b'MZ' or data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Not a PE image')
    optional = pe + 24
    magic = u16(optional)
    if magic not in (0x10b, 0x20b):
        raise ValueError('Unsupported optional header')
    section = optional + u16(pe + 20)
    sections = [(u32(section+i*40+12), u32(section+i*40+16),
                 u32(section+i*40+20)) for i in range(u16(pe+6))]

    def offset(rva, length=1):
        for address, raw_size, raw in sections:
            if address <= rva and rva-address <= raw_size-length:
                result = raw+rva-address
                if result <= len(data)-length:
                    return result
        raise ValueError('RVA outside file-backed section')

    def string(rva):
        p = offset(rva)
        end = data.index(b'\0', p)
        offset(rva, end-p+1)
        return data[p:end].decode('ascii')

    directory = optional + (112 if magic == 0x20b else 96)
    rva, size = u32(directory), u32(directory+4)
    table = offset(rva, 40)
    base, function_count, name_count = [u32(table+x) for x in (16, 20, 24)]
    functions = offset(u32(table+28), function_count*4)
    names = offset(u32(table+32), name_count*4)
    ordinals = offset(u32(table+36), name_count*2)
    result = {}
    for i in range(function_count):
        target = u32(functions+4*i)
        if target:
            result['#'+str(base+i)] = string(target) if rva <= target < rva+size else None
    for i in range(name_count):
        index = u16(ordinals+2*i)
        if index >= function_count:
            raise ValueError('Export ordinal out of range')
        result[string(u32(names+4*i))] = result['#'+str(base+index)]
    return u16(pe+4), result


def missing(table, native):
    return {name: dest for name, dest in table.items()
            if not name.startswith('#') and dest
            and dest.split('.', 1)[0].lower() == 'ntdll'
            and dest.split('.', 1)[1] not in native}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-x86', required=True, type=Path)
    parser.add_argument('--native-x64', required=True, type=Path)
    parser.add_argument('--base', default='24a03ae')
    parser.add_argument('--output', type=Path, default=Path('audit/KxNtParity/native-forwarders.json'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    result = {'BaseCommit': args.base, 'Scope': 'Static named native-forwarder resolution only', 'Architectures': []}
    for arch, native_path, package, expected in (
        ('x86', args.native_x86, 'Installer/Kex32/KxNt.dll', 0x14c),
        ('x64', args.native_x64, 'Installer/KxNt.dll', 0x8664)):
        native_bytes = native_path.read_bytes()
        native_machine, native = exports(native_bytes)
        package_bytes = (root/package).read_bytes()
        machine, current = exports(package_bytes)
        baseline_bytes = subprocess.check_output(['git', 'show', args.base+':'+package], cwd=root)
        baseline_machine, baseline = exports(baseline_bytes)
        if any(m != expected for m in (native_machine, machine, baseline_machine)):
            raise ValueError('Architecture mismatch: '+arch)
        old_missing, new_missing = missing(baseline, native), missing(current, native)
        removed = sorted(set(baseline)-set(current))
        if removed:
            raise ValueError('Baseline export names removed: '+str(removed))
        row = {'Architecture': arch, 'PackagePath': package,
               'NativePath': str(native_path.resolve()),
               'NativeSHA256': hashlib.sha256(native_bytes).hexdigest(),
               'PackageSHA256': hashlib.sha256(package_bytes).hexdigest(),
               'BaselineNamedExports': sum(not n.startswith('#') for n in baseline),
               'CurrentNamedExports': sum(not n.startswith('#') for n in current),
               'BaselineMissingCount': len(old_missing), 'CurrentMissingCount': len(new_missing),
               'NoLongerMissing': {n: {'OldTarget': old_missing[n], 'CurrentTarget': current[n]}
                                  for n in sorted(set(old_missing)-set(new_missing))},
               'NewMissing': {n: new_missing[n] for n in sorted(set(new_missing)-set(old_missing))},
               'UnresolvedNativeForwarders': new_missing}
        result['Architectures'].append(row)
        print(f'{arch}: missing {len(old_missing)} -> {len(new_missing)}, named {row["CurrentNamedExports"]}')
    output = root/args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
