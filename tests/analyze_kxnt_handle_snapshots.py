"""Summarize observed handle identities, without attributing their creation."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def analyze(row):
    phases = {}
    names = {}
    counts = {}
    for line in row['Output'].splitlines():
        match = re.fullmatch(r'Snapshot=(\w+) Handle=([0-9a-fA-F]+) Object=([0-9a-fA-F]+) Access=([0-9a-fA-F]+) TypeStatus=([0-9a-fA-F]+)(?: Type=(.*))?', line)
        if match:
            phase, handle, obj, access, status, kind = match.groups()
            phases.setdefault(phase, {})[(int(handle, 16), int(obj, 16))] = dict(Handle=handle, Object=obj, Access=access, TypeStatus=status, Type=kind)
        match = re.fullmatch(r'ObjectName=(\w+) Handle=([0-9a-fA-F]+) Name=(.*)', line)
        if match:
            phase, handle, name = match.groups()
            names[(phase, int(handle, 16))] = name
        match = re.fullmatch(r'Snapshot=(\w+) Entries=(\d+) HandleCount=(\d+)', line)
        if match:
            phase, entries, count = match.groups()
            counts[phase] = dict(Entries=int(entries), HandleCount=int(count))
    expected = {'before', 'immediate', 'after100ms', 'after1000ms', 'warm100ms'}
    if phases.keys() != expected or counts.keys() != expected:
        raise ValueError('Incomplete snapshot phases')
    for phase in expected:
        if len(phases[phase]) != counts[phase]['Entries']:
            raise ValueError('Parsed identities do not match enumerated entries')
    changes = {}
    for phase in sorted(expected - {'before'}):
        def records(keys, source):
            return [dict(phases[source][key], Name=names.get((source, key[0]))) for key in sorted(keys)]
        changes[phase] = dict(
            Added=records(phases[phase].keys() - phases['before'].keys(), phase),
            Removed=records(phases['before'].keys() - phases[phase].keys(), 'before'))
    return dict(Architecture=row['Architecture'], Mode=row['Mode'], VMX=row['VMX'], Windowless=row.get('Windowless', False), Counts=counts, Changes=changes)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('receipts', nargs='+', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--full-utf', action='store_true', help='Read instrumented full UTF receipts with lookup and conversion outputs')
    args = parser.parse_args()
    rows = []
    for path in args.receipts:
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        if args.full_utf:
            if not data.get('TraceResources') or data['Status'] not in ('Passed', 'Failed') or len(data['Results']) != 2 or not all(row['ContractGate'] for row in data['Results']):
                raise ValueError('Instrumented full UTF observation is incomplete')
            inputs = [dict(Architecture=row['Architecture'], VMX=row['VMX'], Windowless=True, Mode=mode, Output=row[key]) for row in data['Results'] for mode, key in (('lookup', 'ControlOutput'), ('conversion', 'Output'))]
        else:
            if data['State'] != 'Measured' or len(data['Results']) != 5:
                raise ValueError('Resource observation is incomplete')
            inputs = data['Results']
        for row in inputs:
            result = analyze(row)
            rows.append(result)
            print(Path(row['VMX']).parent.name, row['Architecture'], row['Mode'],
                  [(item['Type'], item['Name']) for item in result['Changes']['after100ms']['Added']])
    args.output.write_text(json.dumps(dict(
        Sources={str(path): digest(path) for path in args.receipts},
        AnalyzerSHA256=digest(Path(__file__)), Results=rows,
        Scope='Observed identity differences from each process baseline; no creation stack, lifecycle or leak attribution'), ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
