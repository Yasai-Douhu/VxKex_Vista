"""Prioritize unresolved KxNt native forwarders using actual PE imports.

Static evidence only: dynamic GetProcAddress calls and runtime class selection
cannot be inferred from this report. No guest or executable is modified.
"""
import argparse
import hashlib
import json
from pathlib import Path


class PE:
    def __init__(self, path):
        self.path = str(Path(path).resolve())
        self.data = Path(path).read_bytes()
        self.sha256 = hashlib.sha256(self.data).hexdigest().upper()
        if self.data[:2] != b'MZ':
            raise ValueError('Missing MZ header')
        pe = self.num(60, 4)
        if self.data[pe:pe + 4] != b'PE\0\0':
            raise ValueError('Missing PE signature')
        self.machine = self.num(pe + 4, 2)
        if self.machine not in (0x14c, 0x8664):
            raise ValueError('Only x86 and AMD64 are audited')
        opt = pe + 24
        magic = self.num(opt, 2)
        if magic not in (0x10b, 0x20b):
            raise ValueError('Unsupported optional header')
        self.bits = 64 if magic == 0x20b else 32
        self.width = self.bits // 8
        directories = opt + (112 if self.bits == 64 else 96)
        count = self.num(directories - 4, 4)
        self.directories = [(self.num(directories + i * 8, 4), self.num(directories + i * 8 + 4, 4)) for i in range(min(count, 16))]
        section = opt + self.num(pe + 20, 2)
        self.sections = [(self.num(section + i * 40 + 12, 4), self.num(section + i * 40 + 16, 4), self.num(section + i * 40 + 20, 4)) for i in range(self.num(pe + 6, 2))]
        self.headers = self.num(opt + 60, 4)

    def num(self, pos, width):
        if pos < 0 or pos + width > len(self.data):
            raise ValueError('PE read outside file')
        return int.from_bytes(self.data[pos:pos + width], 'little')

    def offset(self, rva, length=1):
        if rva < self.headers and rva + length <= min(self.headers, len(self.data)):
            return rva
        for va, raw_size, raw in self.sections:
            if va <= rva and rva + length <= va + raw_size and raw + rva - va + length <= len(self.data):
                return raw + rva - va
        raise ValueError('RVA outside file-backed image')

    def string(self, rva):
        pos = self.offset(rva)
        end = self.data.find(b'\0', pos, min(pos + 4096, len(self.data)))
        if end < 0:
            raise ValueError('Unterminated PE name')
        self.offset(rva, end - pos + 1)
        return self.data[pos:end].decode('ascii')

    def directory(self, index):
        return self.directories[index] if index < len(self.directories) else (0, 0)

    def exports(self):
        rva, size = self.directory(0)
        if not rva:
            return {}
        pos = self.offset(rva, 40)
        functions, count = self.num(pos + 20, 4), self.num(pos + 24, 4)
        fa, na, oa = [self.num(pos + p, 4) for p in (28, 32, 36)]
        result = {}
        for i in range(count):
            name = self.string(self.num(self.offset(na + i * 4, 4), 4))
            ordinal = self.num(self.offset(oa + i * 2, 2), 2)
            if ordinal >= functions:
                raise ValueError('Export ordinal outside EAT')
            function = self.num(self.offset(fa + ordinal * 4, 4), 4)
            result[name] = self.string(function) if rva <= function < rva + size else None
        return result

    def imports(self):
        result = []
        for index, width, mode in ((1, 20, 'normal'), (13, 32, 'delay')):
            rva, size = self.directory(index)
            if not rva:
                continue
            terminated = False
            for i in range(size // width):
                pos = self.offset(rva + i * width, width)
                words = [self.num(pos + j * 4, 4) for j in range(width // 4)]
                if not any(words):
                    terminated = True
                    break
                if mode == 'delay':
                    if words[0] != 1:
                        raise ValueError('Unsupported VA-based delay descriptor')
                    name, thunk = words[1], words[4]
                else:
                    name, thunk = words[3], words[0] or words[4]
                dll = self.string(name)
                for j in range(len(self.data) // self.width):
                    value = self.num(self.offset(thunk + j * self.width, self.width), self.width)
                    if not value:
                        break
                    api = '#' + str(value & 0xffff) if value & (1 << (self.bits - 1)) else self.string(value + 2)
                    result.append({'Module': dll, 'Name': api, 'Mode': mode})
                else:
                    raise ValueError('Unterminated import thunk')
            if not terminated:
                raise ValueError('Unterminated import descriptor table')
        return result


def main():
    parser = argparse.ArgumentParser()
    for arch in ('x86', 'x64'):
        parser.add_argument('--native-' + arch, required=True)
        parser.add_argument('--kxnt-' + arch, required=True)
    parser.add_argument('--app', action='append', required=True)
    parser.add_argument('--watch-name', action='append', default=[])
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output)
    if output.exists():
        raise ValueError('Preserve earlier audit')
    tables = {}
    for arch in ('x86', 'x64'):
        native = PE(getattr(args, 'native_' + arch))
        kxnt = PE(getattr(args, 'kxnt_' + arch))
        expected = 32 if arch == 'x86' else 64
        if native.bits != expected or kxnt.bits != expected:
            raise ValueError('Architecture mismatch')
        available = native.exports()
        exports = kxnt.exports()
        missing = {name: dest for name, dest in exports.items() if dest and dest.lower().startswith('ntdll.') and dest.split('.', 1)[1] not in available}
        tables[arch] = {'NativePath': native.path, 'NativeSHA256': native.sha256, 'KxNtPath': kxnt.path, 'KxNtSHA256': kxnt.sha256, 'NamedExports': len(exports), 'Unresolved': missing}
    apps = []
    for path in args.app:
        image = PE(path)
        imports = image.imports()
        table = tables['x64' if image.bits == 64 else 'x86']
        nt_imports = [item for item in imports if item['Module'].lower() in ('ntdll.dll', 'ntdll', 'kxnt.dll', 'kxnt')]
        hits = [item for item in nt_imports if item['Name'] in table['Unresolved']]
        candidates = []
        watched = []
        for name in sorted(set(table['Unresolved']) | set(args.watch_name)):
            encoded = name.encode('ascii') + b'\0'
            pos = image.data.find(encoded)
            if pos >= 0 and (pos == 0 or not (image.data[pos - 1:pos].isalnum() or image.data[pos - 1:pos] == b'_')):
                if name in table['Unresolved']:
                    candidates.append(name)
                if name in args.watch_name:
                    watched.append(name)
        context = {name: sorted({item['Module'] for item in imports if item['Name'] == name}) for name in candidates + watched}
        apps.append({'Path': image.path, 'SHA256': image.sha256, 'Bits': image.bits, 'Imports': imports, 'NativeImports': nt_imports, 'UnresolvedKxNtHits': hits, 'UnresolvedNameStringCandidates': candidates, 'WatchedNameStringCandidates': watched, 'StringCandidateImportModules': context})
    output.write_text(json.dumps({'NativeForwarders': tables, 'WatchNames': args.watch_name, 'Applications': apps, 'Scope': 'Named native forwarder plus actual direct/delay PE import audit; null-terminated name strings are candidates only, not proof of a lookup or call. Names may belong to imports from other modules, reported separately. Not runtime calls, recursive dependency loading or proof that every unused forwarder can be ignored.'}, indent=2), encoding='utf-8')
    print(json.dumps([{'Path': app['Path'], 'NativeImports': len(app['NativeImports']), 'Hits': app['UnresolvedKxNtHits'], 'StringCandidates': app['UnresolvedNameStringCandidates']} for app in apps], indent=2))


if __name__ == '__main__':
    main()
