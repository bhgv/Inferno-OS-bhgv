#!/usr/bin/env python3
"""Independent two-symbol String/List trial fixtures and ordinary Limbo sources."""
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def sources(initial=0, calls=4, reloads=(), reverse_provider=False, aliases=False, provider_style=0, cleanup=False, module_alias=False, nil_calls=(), nil_style=0):
    reloads = () if reloads is False else reloads
    argument = 'list of string' if initial else 'string'
    bodies = (
        ('return value :: nil;', 'return hd value;'),
        ('copy := value; items := copy :: nil; result := items; return result;',
         'copy := value; result := hd copy; answer := result; return answer;'),
        ('copy := value; value = nil; return copy :: nil;',
         'copy := value; value = nil; return hd copy;'),
        ('result := value :: nil; value = nil; answer := result; return answer;',
         'result := hd value; value = nil; answer := result; return answer;'),
    )[provider_style]
    definitions = (f'unquoted(value: string): list of string {{ {bodies[0]} }}',
                   f'quoted(value: list of string): string {{ {bodies[1]} }}')
    provider = ('implement String;\nString: module {\n'
                'unquoted: fn(value: string): list of string;\n'
                'quoted: fn(value: list of string): string;\n};\n'
                +'\n'.join(definitions[::-1] if reverse_provider else definitions)+'\n')
    body = ['    alias0 := value;'] if aliases else []
    if cleanup:
        if not aliases: raise ValueError('cleanup requires aliased sources')
        body.append('    value = nil;')
    body.append('    lib := load String String->PATH;')
    for i in range(calls):
        if aliases and i:
            body.append(f'    alias{i} := result{i-1};')
            if cleanup: body.append(f'    result{i-1} = nil;')
        if i and (reloads is True or i in reloads):
            body.append('    lib = load String String->PATH;')
        if aliases:
            method = ('quoted' if initial else 'unquoted') if i % 2 == 0 else ('unquoted' if initial else 'quoted')
            body.append(f'    result{i} := lib->{method}(alias{i});')
            if cleanup: body.append(f'    alias{i} = nil;')
        elif i % 2 == 0:
            body.append(f'    other {":=" if i == 0 else "="} lib->{"quoted" if initial else "unquoted"}(value);')
        else:
            body.append(f'    value = lib->{"unquoted" if initial else "quoted"}(other);')
    if nil_calls:
        expanded = []
        for line in body:
            if '->' in line and ('result' in line or 'other' in line or 'value =' in line):
                # Every source variant emits exactly one method call per result.
                ordinal = sum('->' in item and '(' in item for item in expanded)
                if ordinal in nil_calls:
                    argument_text = line[line.index('(')+1:line.index(')')]
                    if nil_style:
                        kind = 'list of string' if '->quoted(' in line else 'string'
                        expanded.append(f'    empty{ordinal}: {kind} = nil;')
                        if nil_style == 2:
                            expanded.append(f'    nilalias{ordinal} := empty{ordinal};')
                            expanded.append(f'    empty{ordinal} = nil;')
                        replacement = f'nilalias{ordinal}' if nil_style == 2 else f'empty{ordinal}'
                    else:
                        replacement = 'nil'
                    line = line.replace('('+argument_text+')', '('+replacement+')')
            expanded.append(line)
        body = expanded
    if module_alias:
        body = [line.replace('lib->', 'active->') for line in body]
        expanded = []
        for line in body:
            expanded.append(line)
            if 'load String' in line:
                expanded.append('    active '+(':=' if ':=' in line else '=')+' lib;')
                expanded.append('    lib = nil;')
        body = expanded
    caller = ('implement Caller;\ninclude "string.m";\n'
              f'Caller: module {{ init: fn(value: {argument}); }};\n'
              f'init(value: {argument}) {{\n'+'\n'.join(body)+'\n}\n')
    return caller, provider


def cases(raw, path=None, include_nil=False):
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
                 transfer_target=64, double_transfer=False, cleanup=True, signatures=None, nil_calls=(), nil_source=8, nil_target=64, double_nil=False):
        records = [binding(module=2, symbol=1 if direction == 0 else 4,
                           sig=(signatures or {}).get(direction, sig[direction])) for direction in order]
        if duplicate: records[1] = records[0]
        code, count = b'', 1
        for i, direction in enumerate(sequence):
            if i and i not in nil_calls and (transfers is None or i in transfers):
                move = ins('IMOVP', 9, source=transfer_source, destination=transfer_target)
                code += move*(2 if double_transfer else 1)
                count += 2 if double_transfer else 1
            if i and cleanup:
                code += ins('IMOVP', 1, source=8, destination=72)
                count += 1
            if path is not None and (i == 0 or reloads is True or i in reloads):
                code += ins('ILOAD', 64, source=16, middle=0, destination=0)
                count += 1
            if i in nil_calls:
                code += ins('IMOVP', 1, source=nil_source, destination=nil_target)*(2 if double_nil else 1)
                count += 2 if double_nil else 1
            ordinal = order.index(direction) if direction in order else 1
            code += (ins('IMFRAME', 65, source=0, middle=ordinal, destination=40)
                     +indirect('IMOVP', 64, 40, 64)+indirect('ILEA', 72, 40, 32)
                     +ins('IMCALL', 72, source=40, middle=1-ordinal if wrong_call else ordinal, destination=0))
            count += 4
        mp_bytes = 24 if path is not None else 8 if len(sequence) == 1 and not nil_calls else 16
        mp_map = mp_map if mp_map is not None else b'\xe0' if path is not None else b'\x80' if len(sequence) == 1 and not nil_calls else b'\xc0'
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
        if include_nil:
            for resets in ((0,), (1,), (2,), (0, 2), (1, 3)):
                for order in ((0, 1), (1, 0)):
                    rows.append((initial, 1, consumer(sequence, order, nil_calls=resets, reloads=path is not None), good))
            rows.append((initial, 1, consumer((initial,), (initial,), nil_calls=(0,)), good))
            for resets in ((0, 2, 16), (1, 3, 17)):
                for order in ((0, 1), (1, 0)):
                    rows.append((initial, 1, consumer(tuple((initial+i)%2 for i in range(32)), order,
                                                    nil_calls=resets, reloads=path is not None), good))
        bad_consumers = (
            consumer(sequence, nil_calls=(0,), nil_source=0),
            consumer(sequence, nil_calls=(0,), nil_source=16),
            consumer(sequence, nil_calls=(1,), nil_target=65),
            consumer(sequence, nil_calls=(1,), nil_target=72),
            consumer(sequence, nil_calls=(0,), double_nil=True),
            consumer(sequence, nil_calls=(0,), mp_map=b'\x80'),
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


def check_alias_refusals(limbo, work):
    """Aliases cannot recover an older value after its native root was replaced."""
    count = 0
    for initial in (0, 1):
        text, _ = sources(initial, 6, aliases=True)
        for old, new in (('alias2 := result1', 'alias2 := value'),
                         ('alias4 := result3', 'alias4 := result1')):
            source = work/'stale-alias.b'
            source.write_text(text.replace(old, new))
            for flag in ('-64m', '-64l'):
                output = work/'stale-alias.dis64'
                output.unlink(missing_ok=True)
                result = subprocess.run([str(limbo.resolve()), flag, '-I', str(ROOT/'module'),
                                         '-o', str(output), str(source)], capture_output=True, text=True, timeout=60)
                if result.returncode != 1 or output.exists() or 'Dis64 managed trial:' not in result.stdout+result.stderr:
                    raise RuntimeError('stale managed alias bypassed compiler refusal: '+result.stdout+result.stderr)
                count += 1
    print(f'PASS managed alias identities: {count} stale-value compiler refusals')


def check_provider_locals(limbo, work):
    """Compare complete images, then prove valid source with other effects is refused."""
    count = 0
    for only in (None, 'unquoted', 'quoted'):
        for reverse in (False, True):
            reference = None
            for style in range(4):
                _, text = sources(reverse_provider=reverse, provider_style=style)
                if only is not None:
                    other = 'quoted' if only == 'unquoted' else 'unquoted'
                    text = '\n'.join(line for line in text.splitlines() if not line.lstrip().startswith(other+'(')
                                     and not line.lstrip().startswith(other+':'))+'\n'
                source = work/'provider-locals.b'
                output = work/'provider-locals.dis64'
                source.write_text(text)
                subprocess.run([str(limbo.resolve()), '-64m', '-o', str(output), str(source)], check=True, timeout=60)
                image = output.read_bytes()
                if reference is None: reference = image
                if image != reference:
                    raise RuntimeError('provider local assignments changed the Dis64 image')
                count += 1
    bodies = (
        ('return value :: value :: nil;', 'return hd value;'),
        ('items := value :: nil; items = nil; return items;', 'return hd value;'),
        ('return nil;', 'return hd value;'),
        ('return value :: nil;', 'return hd tl value;'),
        ('return value :: nil;', 'result := hd value; result = nil; return result;'),
        ('return value :: nil;', 'result := hd value; return hd value;'),
    )
    for unquoted, quoted in bodies:
        _, text = sources()
        text = text.replace('return value :: nil;', unquoted).replace('return hd value;', quoted)
        source = work/'provider-other-effect.b'
        output = work/'provider-other-effect.dis64'
        source.write_text(text)
        subprocess.run([str(limbo.resolve()), '-o', str(output), str(source)], check=True, timeout=60)
        output.unlink()
        result = subprocess.run([str(limbo.resolve()), '-64m', '-o', str(output), str(source)],
                                capture_output=True, text=True, timeout=60)
        if result.returncode != 1 or output.exists() or 'Dis64 managed trial:' not in result.stdout+result.stderr:
            raise RuntimeError('unsupported provider effect bypassed refusal: '+result.stdout+result.stderr)
    print(f'PASS managed provider locals: {count} identical images, {len(bodies)} other-effect refusals')


def check_caller_cleanup(limbo, work):
    """Discarding a source name preserves its live copy and its emitted root lifetime."""
    positives = 0
    for initial in (0, 1):
        for calls in (4, 32):
            for file_mode in (False, True):
                reference = None
                for layout in (0, 1, 2):
                    text, _ = sources(initial, calls, reloads=file_mode, aliases=True, cleanup=layout != 0)
                    if layout == 2:
                        text = text.replace('    value = nil;\n', '').replace(
                            '    lib := load String String->PATH;',
                            '    lib := load String String->PATH;\n    value = nil;')
                    source = work/'caller-cleanup.b'; source.write_text(text)
                    output = work/'caller-cleanup.dis64'
                    subprocess.run([str(limbo.resolve()), '-64l' if file_mode else '-64m', '-I', str(ROOT/'module'),
                                    '-o', str(output), str(source)], check=True, timeout=60)
                    image = output.read_bytes()
                    if reference is None: reference = image
                    if image != reference: raise RuntimeError('discarded local changed the complete caller image')
                    positives += 1
    print(f'PASS managed caller cleanup: {positives} identical images')


def check_module_aliases(limbo, work):
    positives = negatives = 0
    for initial in (0, 1):
        for file_mode in (False, True):
            reference = None
            for alias in (0, 1, 2):
                text, _ = sources(initial, 32, reloads=file_mode, aliases=True, cleanup=True, module_alias=bool(alias))
                if alias == 2:
                    text = text.replace('    lib = load String String->PATH;\n    active = lib;\n    lib = nil;',
                                        '    active = load String String->PATH;')
                source = work/'module-alias.b'; source.write_text(text)
                output = work/'module-alias.dis64'
                subprocess.run([str(limbo.resolve()), '-64l' if file_mode else '-64m', '-I', str(ROOT/'module'),
                                '-o', str(output), str(source)], check=True, timeout=60)
                image = output.read_bytes()
                if reference is None: reference = image
                if image != reference: raise RuntimeError('module alias changed the complete Dis64 image')
                positives += 1
        text, _ = sources(initial, 4, reloads=True, aliases=True, cleanup=True, module_alias=True)
        text = text.replace('    lib = nil;', '    saved := active;\n    lib = nil;', 1)
        for old, new in (('result1 := active->', 'result1 := saved->'),
                         ('result1 := active->', 'result1 := lib->')):
            source = work/'stale-module-alias.b'; source.write_text(text.replace(old, new))
            output = work/'stale-module-alias.dis64'
            subprocess.run([str(limbo.resolve()), '-I', str(ROOT/'module'), '-o', str(output), str(source)], check=True, timeout=60)
            output.unlink()
            result = subprocess.run([str(limbo.resolve()), '-64l', '-I', str(ROOT/'module'), '-o', str(output), str(source)],
                                    capture_output=True, text=True, timeout=60)
            if result.returncode != 1 or output.exists() or 'Dis64 managed trial:' not in result.stdout+result.stderr:
                raise RuntimeError('stale/cleared module alias bypassed refusal: '+result.stdout+result.stderr)
            negatives += 1
    print(f'PASS managed module aliases: {positives} identical images, {negatives} stale/cleared-module refusals')


def check_nil_refusals(limbo, work):
    """Writing H replaces the current root; stale aliases cannot recover its old owner."""
    refusals = 0
    for initial in (0, 1):
        text, _ = sources(initial, 6, aliases=True, nil_calls=(2,))
        for replacement in ('alias0', 'result1'):
            source = work/'nil-stale-owner.b'
            source.write_text(text.replace('alias4 := result3', 'alias4 := '+replacement))
            for flag in ('-64m', '-64l'):
                output = source.with_suffix('.dis64')
                subprocess.run([str(limbo.resolve()), '-I', str(ROOT/'module'), '-o', str(output), str(source)],
                               check=True, timeout=60)
                output.unlink()
                result = subprocess.run([str(limbo.resolve()), flag, '-I', str(ROOT/'module'), '-o', str(output), str(source)],
                                        capture_output=True, text=True, timeout=60)
                if result.returncode != 1 or output.exists() or 'Dis64 managed trial:' not in result.stdout+result.stderr:
                    raise RuntimeError('nil write resurrected a stale root: '+result.stdout+result.stderr)
                refusals += 1
    print(f'PASS managed nil ownership: {refusals} stale-root compiler refusals')


def nil_rows(limbo, work, file_path='/managed-provider.dis64'):
    """Compile nil at first/later calls, aliases and reloads; compare all CODE bytes independently."""
    raw = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'))
    code_bytes = runpy.run_path(str(ROOT/'utils/tests/dis64-pair-boot.py'))['code_bytes']
    rows = []
    for initial in (0, 1):
        for file_mode in (False, True):
            for calls, resets in ((1, (0,)), (4, (0,)), (4, (1,)), (4, (2,)),
                                  (32, (0, 2, 16)), (32, (1, 3, 17))):
                reference = None
                for style in range(3):
                    text, body = sources(initial, calls, reloads=file_mode, aliases=True,
                                         cleanup=True, module_alias=True, nil_calls=resets, nil_style=style)
                    if file_mode: text = text.replace('String->PATH', '"'+file_path+'"')
                    source = work/'nil-caller.b'; output = source.with_suffix('.dis64')
                    source.write_text(text)
                    subprocess.run([str(limbo.resolve()), '-64l' if file_mode else '-64m', '-I', str(ROOT/'module'),
                                    '-o', str(output), str(source)], check=True, timeout=60)
                    image = output.read_bytes()
                    if reference is None: reference = image
                    if image != reference: raise RuntimeError('nil alias changed the complete Dis64 image')
                if not any(ok and direction == initial and code_bytes(candidate) == code_bytes(reference)
                           for direction, ok, candidate, _ in cases(raw, file_path if file_mode else None, include_nil=True)):
                    raise RuntimeError(f'nil compiler CODE differs from independent serializer: {initial=} {file_mode=} {calls=} {resets=} '+code_bytes(reference).hex())
                source = work/'nil-provider.b'; source.write_text(body)
                subprocess.run([str(limbo.resolve()), '-64m', '-o', str(output), str(source)], check=True, timeout=60)
                dependency = output.read_bytes()
                # At each reset, String nil can be consed; HEAD of List nil must unwind.
                failure = next((i+1 for i in resets if (initial+i)%2), 0)
                rows.append((initial, calls, int(file_mode), int(failure), reference, dependency))
    print(f'PASS managed nil compiler: {3*len(rows)} complete images; literal/local/alias, first/later calls and reloads')
    return rows


def header(raw, limbo, work):
    check_alias_refusals(limbo, work)
    check_provider_locals(limbo, work)
    check_caller_cleanup(limbo, work)
    check_module_aliases(limbo, work)
    check_nil_refusals(limbo, work)
    rows = cases(raw, include_nil=True)+cases(raw, '/managed-provider.dis64', include_nil=True)
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
    for initial in (0, 1):
        for file_mode in (False, True):
            text, body = sources(initial, 32, reloads=file_mode, aliases=True, provider_style=2+int(file_mode), cleanup=True, module_alias=True)
            if file_mode: text = text.replace('String->PATH', '"/managed-provider.dis64"')
            source = work/f'alias-caller-{initial}-{file_mode}.b'
            output = source.with_suffix('.dis64')
            source.write_text(text)
            subprocess.run([str(limbo.resolve()), '-64l' if file_mode else '-64m', '-I', str(ROOT/'module'),
                            '-o', str(output), str(source)], check=True, timeout=60)
            consumer = output.read_bytes()
            source = work/f'local-provider-{initial}-{file_mode}.b'
            source.write_text(body)
            subprocess.run([str(limbo.resolve()), '-64m', '-o', str(output), str(source)], check=True, timeout=60)
            rows.append((initial, 1, consumer, output.read_bytes()))
    valid_providers = {provider for _, ok, _, provider in rows if ok}
    declarations, entries = [], []
    for index, (initial, ok, consumer, provider) in enumerate(rows):
        for label, payload in (('groupconsumer', consumer), ('groupprovider', provider)):
            declarations.append(f'static const uchar {label}{index}[] = {{'+','.join(map(str, payload))+'};')
        entries.append(f'{{{initial},{ok},{int(provider in valid_providers)},groupconsumer{index},sizeof(groupconsumer{index}),'
                       f'groupprovider{index},sizeof(groupprovider{index})}}')
    nilentries = []
    for index, (initial, calls, filemode, failure, consumer, provider) in enumerate(nil_rows(limbo, work)):
        for label, payload in (('nilconsumer', consumer), ('nilprovider', provider)):
            declarations.append(f'static const uchar {label}{index}[] = {{'+','.join(map(str, payload))+'};')
        nilentries.append(f'{{{initial},{calls},{filemode},{failure},nilconsumer{index},sizeof(nilconsumer{index}),'
                          f'nilprovider{index},sizeof(nilprovider{index})}}')
    return ('\n'.join(declarations)+'\nstatic const ManagedImageCase managedgroups[] = {\n'+',\n'.join(entries)+'\n};\n'
            +'typedef struct ManagedNilCase ManagedNilCase;\n'
            +'struct ManagedNilCase { int direction, calls, filemode, failure; const uchar *consumer; unsigned int consumerbytes; '
            +'const uchar *provider; unsigned int providerbytes; };\n'
            +'static const ManagedNilCase managednils[] = {\n'+',\n'.join(nilentries)+'\n};\n')
