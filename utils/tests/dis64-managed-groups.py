#!/usr/bin/env python3
"""Independent two-symbol String/List trial fixtures and ordinary Limbo sources."""
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def sources(initial=0, calls=4, reloads=(), reverse_provider=False):
    reloads = () if reloads is False else reloads
    argument = 'list of string' if initial else 'string'
    definitions = (
        'unquoted(value: string): list of string { return value :: nil; }',
        'quoted(value: list of string): string { return hd value; }')
    provider = ('implement String;\nString: module {\n'
                'unquoted: fn(value: string): list of string;\n'
                'quoted: fn(value: list of string): string;\n};\n'
                +'\n'.join(definitions[::-1] if reverse_provider else definitions)+'\n')
    body = ['    lib := load String String->PATH;']
    for i in range(calls):
        if i and (reloads is True or i in reloads):
            body.append('    lib = load String String->PATH;')
        if i % 2 == 0:
            body.append(f'    other {":=" if i == 0 else "="} lib->{"quoted" if initial else "unquoted"}(value);')
        else:
            body.append(f'    value = lib->{"unquoted" if initial else "quoted"}(other);')
    caller = ('implement Caller;\ninclude "string.m";\n'
              f'Caller: module {{ init: fn(value: {argument}); }};\n'
              f'init(value: {argument}) {{\n'+'\n'.join(body)+'\n}\n')
    return caller, provider


def cases(raw, path=None):
    word, desc, ins = raw['W'], raw['D'], raw['I']
    base = raw['RAW']['RAW']
    binding = base['binding']
    indirect = runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    names = ('unquoted', 'quoted')
    sig = (word(0xface0010, 1, 0x72, 0x71, 0x71), word(0xface0010, 1, 0x71, 0x72, 0x71))

    def build(pool, **fields):
        strings = word(len(pool))+b''.join(word(len(name), *map(ord, name)) for name in pool)
        return raw['IMAGE']([(kind, strings if kind == 7 else body) for kind, body in raw['SECTIONS'](**fields)])

    def consumer(sequence, order=(0, 1), *, transfers=None, reloads=(), frame_map=b'\x00\xc0',
                 mp_map=None, duplicate=False, wrong_call=False, transfer_source=72,
                 transfer_target=64, double_transfer=False, cleanup=True, signatures=None):
        records = [binding(module=2, symbol=1 if direction == 0 else 4,
                           sig=(signatures or {}).get(direction, sig[direction])) for direction in order]
        if duplicate: records[1] = records[0]
        code, count = b'', 1
        for i, direction in enumerate(sequence):
            if i and (transfers is None or i in transfers):
                move = ins('IMOVP', 9, source=transfer_source, destination=transfer_target)
                code += move*(2 if double_transfer else 1)
                count += 2 if double_transfer else 1
            if i and cleanup:
                code += ins('IMOVP', 1, source=8, destination=72)
                count += 1
            if path is not None and (i == 0 or reloads is True or i in reloads):
                code += ins('ILOAD', 64, source=16, middle=0, destination=0)
                count += 1
            ordinal = order.index(direction) if direction in order else 1
            code += (ins('IMFRAME', 65, source=0, middle=ordinal, destination=40)
                     +indirect('IMOVP', 64, 40, 64)+indirect('ILEA', 72, 40, 32)
                     +ins('IMCALL', 72, source=40, middle=1-ordinal if wrong_call else ordinal, destination=0))
            count += 4
        mp_bytes = 24 if path is not None else 8 if len(sequence) == 1 else 16
        mp_map = mp_map if mp_map is not None else b'\xe0' if path is not None else b'\x80' if len(sequence) == 1 else b'\xc0'
        return build(('caller', 'unquoted', 'String', path or 'unused', 'quoted'), name=0,
                     databytes=mp_bytes, code=word(count)+code+raw['EXIT'],
                     types=[desc(80, 8, frame_map), desc(mp_bytes, 8, mp_map)], imports=records)

    def provider(body_order=(0, 1), export_order=(0, 1), *, module='String', entry_override=None,
                 frame_map=b'\x00\x80', duplicate=False, alias=False, bad_signature=False, bad_body=False):
        code = raw['EXIT']
        for direction in body_order:
            op = 'ICONSP' if direction == 0 else 'IHEADP'
            code += indirect('IHEADP' if bad_body else op, 64, 32, 0)+ins('IRET', 27)
        records = [binding(export=True, symbol=2+direction,
                           entry=(entry_override or {}).get(direction, 1+2*body_order.index(direction)), frame=0,
                           sig=sig[1-direction] if bad_signature else sig[direction]) for direction in export_order]
        if duplicate: records[1] = records[0]
        if alias: records[1] = binding(export=True, symbol=4, entry=3, frame=0, sig=sig[1])
        return build((module, 'unused', *names, 'unquoted'), name=0, profile=0, databytes=0,
                     code=word(1+2*len(body_order))+code, types=[desc(72, 8, frame_map)], exports=records)

    rows = []
    for initial in (0, 1):
        sequence = tuple((initial+i)%2 for i in range(4))
        for order in ((0, 1), (1, 0)):
            for body_order in ((0, 1), (1, 0)):
                for export_order in ((0, 1), (1, 0)):
                    rows.append((initial, 1, consumer(sequence, order), provider(body_order, export_order)))
        good = provider()
        for n in (1, 2, 3, 32):
            chain = tuple((initial+i)%2 for i in range(n))
            for reloads in ((), True) if path is not None else ((),):
                for order in ((0, 1), (1, 0)):
                    rows.append((initial, 1, consumer(chain, order, reloads=reloads), good))
        # One requested method must still validate both provider exports, including the unused one.
        rows.append((initial, 1, consumer((initial,)*4, (initial,), transfers=()), good))
        rows.append((initial, 1, consumer((initial,)*4, (initial,), transfers=()), provider((initial,), (initial,))))
        rows.append((initial, 1, consumer((initial,initial,1-initial,1-initial), transfers=(2,)), good))
        bad_consumers = (
            consumer(sequence, transfers=()), consumer(sequence, frame_map=b'\x00\x80'),
            consumer(sequence, mp_map=b'\x00'), consumer(sequence, duplicate=True),
            consumer(sequence, wrong_call=True), consumer(sequence, transfer_source=64),
            consumer(sequence, transfer_target=72), consumer(sequence, double_transfer=True),
            consumer(sequence, cleanup=False), consumer(sequence, signatures={0: sig[1]}),
            consumer(sequence, (initial,)), consumer(tuple((initial+i)%2 for i in range(33))),
        )
        rows.extend((initial, 0, value, good) for value in bad_consumers)
        bad_providers = (provider(module='Other'), provider(frame_map=b'\x00\x00'),
                         provider(duplicate=True), provider(alias=True), provider(bad_signature=True),
                         provider(bad_body=True), provider(entry_override={0: 3}),
                         provider(entry_override={1: 1}), provider((initial,), (initial,)))
        rows.extend((initial, 0, consumer(sequence), value) for value in bad_providers)
        rows.append((initial, 0, consumer((initial,)*4, (initial,), transfers=()), provider(frame_map=b'\x00\x00')))
        rows.append((initial, 0, consumer((initial,)*4, (initial,), transfers=()), provider(entry_override={1-initial: 1+2*initial})))
    return rows


def header(raw, limbo, work):
    rows = cases(raw)+cases(raw, '/managed-provider.dis64')
    for initial in (0, 1):
        for file_mode in (False, True):
            text, body = sources(initial, 32, reloads=file_mode, reverse_provider=bool(initial))
            text = text.replace('String->PATH', '"/managed-provider.dis64"') if file_mode else text
            compiled = []
            for label, source_text, flag in (('caller', text, '-64l' if file_mode else '-64m'), ('provider', body, '-64m')):
                source = work/f'group-{label}-{initial}-{file_mode}.b'
                output = source.with_suffix('.dis64')
                source.write_text(source_text)
                subprocess.run([str(limbo.resolve()), flag, '-I', str(ROOT/'module'), '-o', str(output), str(source)], check=True, timeout=60)
                compiled.append(output.read_bytes())
            rows.append((initial, 1, *compiled))
    valid_providers = {provider for _, ok, _, provider in rows if ok}
    declarations, entries = [], []
    for index, (initial, ok, consumer, provider) in enumerate(rows):
        for label, payload in (('groupconsumer', consumer), ('groupprovider', provider)):
            declarations.append(f'static const uchar {label}{index}[] = {{'+','.join(map(str, payload))+'};')
        entries.append(f'{{{initial},{ok},{int(provider in valid_providers)},groupconsumer{index},sizeof(groupconsumer{index}),'
                       f'groupprovider{index},sizeof(groupprovider{index})}}')
    return '\n'.join(declarations)+'\nstatic const ManagedImageCase managedgroups[] = {\n'+',\n'.join(entries)+'\n};\n'
