#!/usr/bin/env python3
"""Boot an explicitly linked Dis64 pair through the complete emulator."""
from itertools import permutations
import argparse
from pathlib import Path
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BOOT = runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
GC = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-gc.py'))
RAW = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'))


# Serialize the caller and provider independently of the runtime decoder.
def images(failure, dependency_name="unit", *, calls=1, late_failure=False, wrong_slot=False):
    words, ins = RAW['W'], RAW['I']
    frame = ins('IMFRAME', 65, source=0, middle=0, destination=64)
    call = ins('IMCALL', 72, source=64, middle=0, destination=0)
    tail_frame = ins('IMFRAME', 65, source=0, middle=0, destination=72)
    tail_call = ins('IMCALL', 72, source=64 if wrong_slot else 72, middle=0, destination=0)
    sequence = frame+call+(tail_frame+tail_call)*(calls-1)
    binding = RAW['RAW']['RAW']['binding']
    sections = RAW['SECTIONS'](
        name=5, databytes=8, imports=[binding(module=0)],
        types=[RAW['D'](80, 8), RAW['D'](8, 8, b'\x80')],
        code=words(2*calls+1)+sequence+RAW['EXIT'],
    )
    names = (dependency_name, 'symbol', 'symbol', 'module', 'module', 'other')
    pool = words(len(names))+b''.join(words(len(name), *(ord(rune) for rune in name)) for name in names)
    caller = RAW['IMAGE']([(kind, pool if kind == 7 else body) for kind, body in sections])
    terminal = ins('IDIVW', 16, source=0, destination=8) if failure else ins('IRET', 27)
    common = dict(data=words(1, 8, 1, 40),
                  code=words(2)+ins('IADDW', 16, source=2, destination=8)+terminal)
    if late_failure:
        common = dict(data=words(1, 8, 1, 2),
                      code=words(3)+ins('ISUBW', 16, source=1, destination=8)
                      +ins('IDIVW', 0, source=8, destination=12)+ins('IRET', 27))
    _, provider = next(GC['provider_cases'](RAW, common))[1]
    return caller, provider


# Use reversed exports so consumer ordinals cannot be mistaken for export ordinals.
def two_symbols(*, second=1, terminal_failure=False, wrong_ordinal=False,
                missing_symbol=False, wrong_signature=False, other_module=False,
                library=False, frame_map=b"\x00\x00", end_with_exit=False):
    words, ins = RAW['W'], RAW['I']
    bindings = RAW['RAW']['RAW']
    binding = bindings['binding']
    frame = ins('IMFRAME', 65, source=0, middle=0, destination=64)
    call = ins('IMCALL', 72, source=64, middle=0, destination=0)
    tail = ins('IMFRAME', 65, source=0, middle=second, destination=64)
    tail += ins('IMCALL', 72, source=64, middle=0 if wrong_ordinal else second, destination=0)
    caller = RAW['IMAGE'](RAW['SECTIONS'](
        name=5, databytes=8,
        imports=[binding(module=0), binding(module=3 if other_module else 0, symbol=5)],
        types=[RAW['D'](80, 8), RAW['D'](8, 8, b'\x80')],
        code=words(5)+frame+call+tail+RAW['EXIT'],
    ))
    ret = ins('IRET', 27)
    terminal = ins('IDIVW', 16, source=0, destination=8) if terminal_failure else ret
    body = ret+terminal
    if library and terminal_failure:
        body += ret
    if end_with_exit:
        body += RAW['EXIT']
    count = 2+int(library and terminal_failure)+int(end_with_exit)
    signature = bindings['signature'](1 if wrong_signature else 0)
    provider = RAW['IMAGE'](RAW['SECTIONS'](
        profile=0 if library else 1, data=words(1, 8, 1, 40), code=words(count)+body,
        types=[RAW['D'](80, 8), RAW['D'](16, 8, b'\x80'), RAW['D'](88, 8, frame_map)],
        exports=[binding(export=True, symbol=3 if missing_symbol else 5, entry=1,
                         frame=2 if library else 0, sig=signature),
                 binding(export=True)],
    ))
    return caller, provider


# Encode a distinct mapped provider slot per import using existing operands.
def sibling_images(*, failure=False, second='peer', wrong_slot=False, mapbyte=0xc0):
    words, ins = RAW['W'], RAW['I']
    binding = RAW['RAW']['RAW']['binding']
    names = ('unit', second, 'symbol', 'caller', 'peer')
    pool = words(len(names))+b''.join(words(len(name), *map(ord, name)) for name in names)
    code = b''
    for slot in (0, 8):
        code += ins('IMFRAME', 65, source=slot, middle=0, destination=64)
        code += ins('IMCALL', 72, source=64, middle=0,
                    destination=0 if wrong_slot and slot else slot)
    sections = RAW['SECTIONS'](
        name=3, databytes=16, imports=[binding(module=0, symbol=2), binding(module=1, symbol=2)],
        types=[RAW['D'](80, 8), RAW['D'](16, 8, bytes([mapbyte]))],
        code=words(5)+code+RAW['EXIT'],
    )
    caller = RAW['IMAGE']([(kind, pool if kind == 7 else body) for kind, body in sections])
    terminal = ins('IDIVW', 16, source=0, destination=8) if failure else ins('IRET', 27)
    body = terminal+ins('IRET', 27) if failure else terminal
    sections = RAW['SECTIONS'](
        name=4, data=words(0), code=words(2+failure)+RAW['EXIT']+body,
        types=[RAW['D'](80, 8), RAW['D'](16, 8, b'\x80'), RAW['D'](88, 8)],
        exports=[binding(export=True, symbol=2, entry=1, frame=2)],
    )
    peer = RAW['IMAGE']([(kind, pool if kind == 7 else body) for kind, body in sections])
    return caller, images(False)[1], peer


# Serialize the private Sys clock sequence with the existing indirect LEA codec.
def clock_image(*, result_offset=72, module='$Sys', signature=1, repeats=1,
                copy=False, early_copy=False, copy_destination=64, load=False, load_slot=8,
                load_index=0, load_map=b"\xc0", second=None, call_index=0,
                sleep_tail=False, period=0, period_offset=64, sleep_index=1,
                sleep_first=False, sleep_only=False, reverse=False):
    words, ins = RAW['W'], RAW['I']
    bindings = RAW['RAW']['RAW']
    calls = runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))
    slot = 8 if load else 0
    frame = ins('IMFRAME', 65, source=slot, middle=call_index, destination=40)
    bind = calls['indirect']('ILEA', result_offset, 40, 32)
    call = ins('IMCALL', 72, source=40, middle=call_index, destination=slot)
    move = ins('IMOVW', 9, source=72, destination=copy_destination)
    group = (move if early_copy else b'')+frame+bind+call+(move if copy else b'')
    prefix = ins('ILOAD', 64, source=0, middle=load_index, destination=load_slot) if load else b''
    count = repeats*(3+copy+early_copy)+1+load
    tail = b''
    if sleep_tail or sleep_first:
        tail = (ins('IMFRAME', 65, source=slot, middle=sleep_index, destination=40)
                + calls['indirect']('IMOVW', period, 40, period_offset, sm=2)
                + bind
                + ins('IMCALL', 72, source=40, middle=sleep_index, destination=slot))
        count += 4*(sleep_tail+sleep_first)
    if sleep_only:
        group = (ins('IMFRAME', 65, source=slot, middle=call_index, destination=40)
                 + calls['indirect']('IMOVW', period, 40, period_offset, sm=2)
                 + bind + ins('IMCALL', 72, source=40, middle=call_index, destination=slot))
        count = repeats*4+1+load
    names = (module, 'sleep' if sleep_only or reverse else 'millisec', 'caller')
    imports = [bindings['binding'](module=0, sig=bindings['signature'](signature, (1,) if sleep_only or reverse else ()))]
    if second is not None:
        symbol, parameters = ('millisec', ()) if reverse else second
        names += (symbol,)
        imports.append(bindings['binding'](module=0, symbol=3,
                                          sig=bindings['signature'](1, parameters)))
    pool = words(len(names))+b''.join(words(len(name), *map(ord, name)) for name in names)
    sections = RAW['SECTIONS'](
        name=2, databytes=16 if load else 8,
        imports=imports,
        types=[RAW['D'](80, 8), RAW['D'](16, 8, load_map) if load else RAW['D'](8, 8, b'\x80')],
        code=words(count)+prefix+tail*sleep_first+group*repeats+tail*sleep_tail+RAW['EXIT'],
    )
    return RAW['IMAGE']([(kind, pool if kind == 7 else body) for kind, body in sections])


# Independently serialize scalar Sys groups with zero, one or two integer arguments.
def sys_integer_image(sequence, *, symbols=None, signatures=None, offsets=None, call_index=None, initial=None, copies=(), steps=None):
    words, ins = RAW['W'], RAW['I']
    bindings = RAW['RAW']['RAW']
    indirect = runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    kinds = {'millisec': (), 'sleep': (1,), 'dup': (1, 1)}
    symbols = symbols or tuple(sorted({name for name, _ in sequence}))
    names = ('$Sys', symbols[0], 'caller', *symbols[1:])
    imports = [bindings['binding'](module=0, symbol=1 if i == 0 else i+2,
               sig=bindings['signature'](1, (signatures or {}).get(name, kinds[name])))
               for i, name in enumerate(symbols)]
    code = ins('ILOAD', 64, source=0, middle=0, destination=8)
    count = 2
    if initial is not None:
        initializers = initial if isinstance(initial, tuple) else (initial,)
        for value in initializers:
            code += ins('IMOVW', 17, source=value, destination=64)
            count += 1
    def scalar_word(operation, source, destination, middle=None):
        sm = 1 if isinstance(source, tuple) and source[0] == 'fp' else 0 if isinstance(source, tuple) else 2
        value = source[1] if isinstance(source, tuple) else source
        mode = (sm<<3)|1
        lhs = 0
        if middle is not None:
            lhs = middle[1] if isinstance(middle, tuple) else middle
            mode |= (128 if middle[0] == 'fp' else 192) if isinstance(middle, tuple) else 64
        return ins(operation, mode, source=value, middle=lhs, destination=destination)

    def scalar_steps(index):
        return b''.join(scalar_word(*operation) for operation in (steps or {}).get(index, ()))

    for step, (name, arguments) in enumerate(sequence):
        code += scalar_steps(step)
        count += len((steps or {}).get(step, ()))
        index = symbols.index(name) if call_index is None else call_index
        code += ins('IMFRAME', 65, source=8, middle=index, destination=40)
        for ordinal, value in enumerate(arguments):
            offset = 64+4*ordinal if offsets is None else offsets[ordinal]
            source, mode = (value[1], 1) if isinstance(value, tuple) else (value, 2)
            code += indirect('IMOVW', source, 40, offset, sm=mode)
        code += indirect('ILEA', 72, 40, 32)
        code += ins('IMCALL', 72, source=40, middle=index, destination=8)
        count += 3+len(arguments)
        if step in copies:
            code += ins('IMOVW', 9, source=72, destination=copies[step] if isinstance(copies, dict) else 64)
            count += 1
    code += scalar_steps(len(sequence))
    count += len((steps or {}).get(len(sequence), ()))
    pool = words(len(names))+b''.join(words(len(name), *map(ord, name)) for name in names)
    sections = RAW['SECTIONS'](name=2, databytes=16, imports=imports,
               types=[RAW['D'](80, 8), RAW['D'](16, 8, b'\xc0')],
               code=words(count)+code+RAW['EXIT'])
    return RAW['IMAGE']([(kind, pool if kind == 7 else body) for kind, body in sections])


def code_bytes(image):
    # The compiler puts CODE second, as does the independent fixture.
    kind, _, offset, size = struct.unpack_from('>HHII', image, 44)
    if kind != 2 or len(image) < offset+size:
        raise RuntimeError('invalid clock CODE section')
    return image[offset:offset+size]

# Compile an actual Limbo LOAD and imported call, then reject nearby unsupported bodies.
def compile_clocks(limbo, work):
    source = work/'clock.b'
    output = work/'clock.dis64'
    template = """implement Clock;
include "sys.m";
Clock: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    now := sys->millisec();
}
"""
    def repeated(count):
        return template.replace('    now := sys->millisec();\n',
                                '    now := sys->millisec();\n' + '    now = sys->millisec();\n'*(count-1))


    command = [str(limbo), '-I', str(ROOT/'module'), '-64', '-o', str(output), str(source)]
    for bad in (template.replace('Sys->PATH', '"$Other"'),
                template.replace('millisec()', 'sleep(536870912)'),
                repeated(33),
                repeated(3).replace('    now = sys->millisec();\n', '    if(now < 0) now = 0;\n', 1)):
        source.write_text(bad)
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        if result.returncode <= 0 or output.exists() or 'Dis64 scalar:' not in result.stderr:
            raise RuntimeError('compiler did not refuse changed clock source: '+result.stderr)
    source.write_text(repeated(1))
    for flag in ('-6', '-640'):
        invalid = [flag if value == '-64' else value for value in command]
        result = subprocess.run(invalid, capture_output=True, text=True, timeout=30)
        if result.returncode <= 0 or output.exists() or '[-64]' not in result.stderr:
            raise RuntimeError('compiler accepted invalid Dis64 flag '+flag)
    for repeats in (1, 3, 32):
        source.write_text(repeated(repeats))
        subprocess.run(command, check=True, timeout=30)
        image = output.read_bytes()
        expected = clock_image(repeats=repeats, load=True)
        if code_bytes(image) != code_bytes(expected):
            raise RuntimeError(f'compiler did not preserve all {repeats} clock calls')
        yield repeats, image, ''
    for repeats in (1, 3, 32):
        source.write_text(repeated(repeats).replace('millisec()', 'sleep(0)'))
        subprocess.run(command, check=True, timeout=30)
        image = output.read_bytes()
        expected = clock_image(repeats=repeats, load=True, sleep_only=True)
        if code_bytes(image) != code_bytes(expected):
            raise RuntimeError('compiler changed the sleep-only import group')
        yield f'{repeats} sleep-only', image, ''
    mixed = template.replace('    now := sys->millisec();', '    now := sys->millisec();\n    now = sys->sleep(0);')
    source.write_text(mixed)
    subprocess.run(command, check=True, timeout=30)
    image = output.read_bytes()
    expected = clock_image(load=True, second=('sleep', (1,)), sleep_tail=True)
    if code_bytes(image) != code_bytes(expected):
        raise RuntimeError('compiler did not preserve the checked sleep call')
    yield 'clock plus zero sleep', image, ''
    source.write_text(mixed.replace('    now := sys->millisec();',
                                    '    now := sys->sleep(0);\n    now = sys->millisec();'))
    subprocess.run(command, check=True, timeout=30)
    image = output.read_bytes()
    expected = clock_image(load=True, second=('sleep', (1,)), sleep_first=True, sleep_tail=True)
    if code_bytes(image) != code_bytes(expected):
        raise RuntimeError('compiler changed the order of mixed Sys calls')
    yield 'sleep, clock, sleep', image, ''
    source.write_text(repeated(31).replace('    now := sys->millisec();',
                                          '    now := sys->sleep(0);\n    now = sys->millisec();'))
    subprocess.run(command, check=True, timeout=30)
    image = output.read_bytes()
    expected = clock_image(load=True, repeats=31, second=('sleep', (1,)), sleep_first=True)
    if code_bytes(image) != code_bytes(expected):
        raise RuntimeError('compiler did not preserve the 32-call mixed sequence')
    yield '32 mixed Sys calls', image, ''
    output.unlink()
    source.write_text(repeated(32).replace('    now := sys->millisec();',
                                          '    now := sys->sleep(0);\n    now = sys->millisec();'))
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    if result.returncode <= 0 or output.exists() or 'Dis64 scalar:' not in result.stderr:
        raise RuntimeError('compiler did not refuse 33 mixed Sys calls: '+result.stderr)

    sequences = (
        [('sleep', (1,))], [('sleep', (-1,))], [('dup', (-1, -1))],
        [('dup', (-1, -1)), ('sleep', (1,)), ('millisec', ())],
        [('sleep', (1,)), ('dup', (-1, -1)), ('millisec', ())],
        [('dup', (-1, -1))]*32, [('dup', (-65, -8193))],
        [('dup', (-8193, -65))],
        [('sleep', (value,)) for value in (-64, -65, -8192, -8193, -536870912)],
    )
    for sequence in sequences:
        statements = []
        for i, (name, values) in enumerate(sequence):
            operator = ':=' if i == 0 else '='
            statements.append(f'    now {operator} sys->{name}('+', '.join(map(str, values))+');')
        source.write_text(template.replace('    now := sys->millisec();', '\n'.join(statements)))
        subprocess.run(command, check=True, timeout=30)
        image = output.read_bytes()
        if code_bytes(image) != code_bytes(sys_integer_image(sequence)):
            raise RuntimeError('compiler changed scalar Sys arguments or call order')
        yield f'{len(sequence)} integer-argument', image, ''
    result = ('fp', 72)
    local = ('fp', 64)
    second, third = ('fp', 68), ('fp', 76)
    bodies = (
        ("""    one := -1;
    two := -2;
    three := -3;
    status := sys->dup(one, two);
    one = sys->sleep(0);
    two = sys->dup(three, three);
    three += two;
    status = sys->dup(one, three);
    status = sys->sleep(one);""",
         [('dup', (local, second)), ('sleep', (0,)), ('dup', (third, third)),
          ('dup', (local, third)), ('sleep', (local,))], -1, {1: 64, 2: 68},
         {0: [('IMOVW', -2, 68), ('IMOVW', -3, 76)], 3: [('IADDW', result, 76)]}, ''),
        ("""    one := -1;
    two := -2;
    three := -3;
    status := sys->dup(one, two);
    three = sys->sleep(0);
    status = sys->dup(two, three);
    one += 1;
    status /= one;""",
         [('dup', (local, second)), ('sleep', (0,)), ('dup', (second, result))],
         -1, {1: 76}, {0: [('IMOVW', -2, 68), ('IMOVW', -3, 76)],
                             3: [('IADDW', 1, 64), ('IDIVW', local, 72)]}, 'zero divide'),
        ("""    status := sys->dup(-1, -1);
    status = sys->dup(status, status);
    status = sys->sleep(status);""",
         [('dup', (-1, -1)), ('dup', (result, result)), ('sleep', (result,))], None, (), {}, ''),
        ("""    value := -1;
    status := sys->dup(value, value);
    status = sys->dup(status, value);
    value = sys->sleep(0);
    status = sys->dup(-1, value);
    status = sys->sleep(value);""",
         [('dup', (local, local)), ('dup', (result, local)), ('sleep', (0,)),
          ('dup', (-1, result)), ('sleep', (local,))], -1, (2,), {}, ''),
        ("""    value := 0;
    value = sys->sleep(value);
    value = sys->sleep(value);""",
         [('sleep', (local,)), ('sleep', (result,))], 0, (0, 1), {}, ''),
        ("""    value := -5;
    value += 4;
    status := sys->dup(value, value);
    status += value;
    value = status * 2;
    value /= 2;
    value ^= 1;
    status = sys->dup(status, value);
    status = sys->sleep(0);""",
         [('dup', (local, local)), ('dup', (result, local)), ('sleep', (0,))], -5, (),
         {0: [('IADDW', 4, 64)],
          1: [('IADDW', local, 72), ('IMULW', 2, 64, result),
              ('IDIVW', 2, 64), ('IXORW', 1, 64)]}, ''),
        ("""    status := sys->dup(-1, -1);
    status += 4;
    status %= 3;
    status -= 1;
    status = sys->dup(status, status);""",
         [('dup', (-1, -1)), ('dup', (result, result))], None, (),
         {1: [('IADDW', 4, 72), ('IMODW', 3, 72), ('ISUBW', 1, 72)]}, ''),
        ("""    value := 3;
    value <<= 2;
    value >>= 1;
    value |= 8;
    value &= 15;
    value ^= 1;
    value %= 7;
    value -= 2;
    status := sys->dup(-1, value);
    status += value;
    status = sys->sleep(0);""",
         [('dup', (-1, local)), ('sleep', (0,))], 3, (),
         {0: [('ISHLW', 2, 64), ('ISHRW', 1, 64), ('IORW', 8, 64),
              ('IANDW', 15, 64), ('IXORW', 1, 64), ('IMODW', 7, 64), ('ISUBW', 2, 64)],
          1: [('IADDW', local, 72)]}, ''),
        ("""    value := 536870911;
    value += 1;
    value <<= 2;
    value /= -1;
    value %= -1;
    status := sys->sleep(value);""",
         [('sleep', (local,))], 536870911, (),
         {0: [('IADDW', 1, 64), ('ISHLW', 2, 64), ('IDIVW', -1, 64), ('IMODW', -1, 64)]}, ''),
        ("""    value := -1;
    status := sys->dup(value, value);
    value += 1;
    status /= value;""",
         [('dup', (local, local))], -1, (),
         {1: [('IADDW', 1, 64), ('IDIVW', local, 72)]}, 'zero divide'),
        ("""    value := -1;
    status := sys->dup(value, value);
    value += 1;
    status %= value;""",
         [('dup', (local, local))], -1, (),
         {1: [('IADDW', 1, 64), ('IMODW', local, 72)]}, 'zero divide'),
        ("    value := 0;\n" + "    value ^= 1;\n"*31 +
         "    value = sys->sleep(value);\n"*32,
         [('sleep', (local,))]+[('sleep', (result,))]*31, 0, tuple(range(32)),
         {0: [('IXORW', 1, 64)]*31}, ''),
    )
    for body, sequence, initial, copies, steps, status in bodies:
        source.write_text(template.replace('    now := sys->millisec();', body))
        subprocess.run(command, check=True, timeout=30)
        image = output.read_bytes()
        expected = sys_integer_image(sequence, initial=initial, copies=copies, steps=steps)
        if code_bytes(image) != code_bytes(expected):
            raise RuntimeError('compiler changed an initialized scalar or previous-result argument')
        yield 'scalar expressions and arguments', image, status
    for value in (-536870912, -8193, -8192, -65, -64, 63, 64, 8191, 8192, 536870911):
        source.write_text(template.replace('millisec()', f'sleep({value})'))
        subprocess.run(command, check=True, timeout=30)
        if code_bytes(output.read_bytes()) != code_bytes(sys_integer_image([('sleep', (value,))])):
            raise RuntimeError(f'compiler changed signed30 argument {value}')
    output.unlink()
    for changed in ('sleep(536870912)', 'sleep(-536870913)', 'dup(-1, -1, -1)'):
        source.write_text(template.replace('millisec()', changed))
        refused = subprocess.run(command, capture_output=True, text=True, timeout=30)
        if refused.returncode <= 0 or output.exists():
            raise RuntimeError('compiler accepted an unsupported scalar Sys argument')


    for body in (
            "    one := -1; two := -2; three := -3; four := -4; status := sys->dup(one, four);",
            "    value := -1;\n" + "    value ^= 1;\n"*64 + "    status := sys->sleep(value);",
            "    value := 0;\n" + "    value ^= 1;\n"*32 + "    value = sys->sleep(value);\n"*32,
            "    one := sys->dup(-1, -1); two := sys->dup(-2, -2); status := sys->dup(one, two);"):
        source.write_text(template.replace('    now := sys->millisec();', body))
        refused = subprocess.run(command, capture_output=True, text=True, timeout=30)
        if refused.returncode <= 0 or output.exists() or 'unsupported checked Sys body' not in refused.stderr:
            raise RuntimeError('compiler accepted scalar state outside the checked Sys caller')




# Compile a real entryless library whose second export observes the first export's write.
def compile_library(limbo, work):
    source = work/'library.b'
    output = work/'library.dis64'
    template = """implement unit;
unit: module {
    symbol: fn();
    other: fn();
};
counter := 40;
divisor := 2;
symbol() { divisor -= 2; }
other() { counter /= divisor; }
"""
    command = [str(limbo), '-64', '-o', str(output), str(source)]
    invalid = template.replace('other: fn();', 'other: fn(value: int);')
    invalid = invalid.replace('other() { counter /= divisor; }',
                              'other(value: int) { counter /= value; }')
    source.write_text(invalid)
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    if result.returncode <= 0 or output.exists() or 'entryless library' not in result.stderr:
        raise RuntimeError('compiler accepted an unsupported library signature: '+result.stderr)
    source.write_text(template)
    subprocess.run(command, check=True, timeout=30)
    return output.read_bytes()


# Require the real scheduler to retire after returning or unwinding the pair.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--emu', type=Path, required=True)
    parser.add_argument('--runner', default='')
    parser.add_argument('--sys-clock', action='store_true')
    parser.add_argument('--limbo', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    command = shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root)]
    execute = BOOT['execute']
    with tempfile.TemporaryDirectory(prefix='pair-boot-', dir=root) as directory:
        work = Path(directory)
        consumer, provider = work/'consumer.dis64', work/'provider.dis64'
        pair = ['-d', '/'+str(consumer.relative_to(root)), '-P', '/'+str(provider.relative_to(root))]
        sibling = work/'unit.dis64'
        automatic = pair[:2]+['-X', 'dis64-pair']
        scenarios = ((False, 1, False), (True, 1, False), (False, 4, False),
                     (False, 1, True), (True, 2, True))
        for failure, calls, late_failure in scenarios:
            caller, dependency = images(failure, calls=calls, late_failure=late_failure)
            consumer.write_bytes(caller)
            provider.write_bytes(dependency)
            sibling.write_bytes(dependency)
            for jit in (0, 1):
                for options in (pair, automatic):
                    output = execute(command+['-c', str(jit)]+options)
                    status = 'zero divide' if failure else ''
                    module = 'unit' if failure else 'other'
                    marker = f'native64 scalar module {module}: jit={jit}, exit="{status}"'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError(f'paired boot did not complete ({failure}, {calls}, {late_failure}): '+output)
        for second, terminal_failure in ((0, True), (1, False), (1, True)):
            caller, dependency = two_symbols(second=second, terminal_failure=terminal_failure)
            consumer.write_bytes(caller)
            provider.write_bytes(dependency)
            sibling.write_bytes(dependency)
            failure = second == 1 and terminal_failure
            module, status = ('unit', 'zero divide') if failure else ('other', '')
            for jit in (0, 1):
                for options in (pair, automatic):
                    output = execute(command+['-c', str(jit)]+options)
                    marker = f'native64 scalar module {module}: jit={jit}, exit="{status}"'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('two-symbol boot did not complete: '+output)
        for failure in (False, True):
            caller, dependency = two_symbols(library=True, terminal_failure=failure)
            consumer.write_bytes(caller)
            provider.write_bytes(dependency)
            sibling.write_bytes(dependency)
            module, status = ('unit', 'zero divide') if failure else ('other', '')
            for jit in (0, 1):
                for options in (pair, automatic):
                    output = execute(command+['-c', str(jit)]+options)
                    marker = f'native64 scalar module {module}: jit={jit}, exit="{status}"'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('entryless library calls did not complete: '+output)
        if args.limbo is not None:
            dependency = compile_library(args.limbo.resolve(), work)
            provider.write_bytes(dependency)
            sibling.write_bytes(dependency)
            for second in (0, 1):
                consumer.write_bytes(two_symbols(second=second)[0])
                module, status = ('unit', 'zero divide') if second else ('other', '')
                for jit in (0, 1):
                    for options in (pair, automatic):
                        output = execute(command+['-c', str(jit)]+options)
                        marker = f'native64 scalar module {module}: jit={jit}, exit="{status}"'
                        if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError('compiler-produced library did not complete: '+output)
            print('PASS Limbo source to entryless Dis64 library and shared MP in C/JIT')
        for changes in ({'frame_map': b'\x80\x00'}, {'end_with_exit': True}):
            caller, dependency = two_symbols(library=True, **changes)
            consumer.write_bytes(caller)
            provider.write_bytes(dependency)
            output = execute(command+pair)
            if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError(f'invalid library {changes} reached scheduler: '+output)
        provider.write_bytes(two_symbols(library=True)[1])
        output = execute(command+['-d', '/'+str(provider.relative_to(root))])
        if 'panic: loading' not in output or BOOT['RETIRED'] in output:
            raise RuntimeError('entryless library accepted as startup program: '+output)
        print('PASS entryless Dis64 libraries with distinct export frame types, C/JIT and refusals')
        for case in ({'wrong_ordinal': True}, {'second': 2}, {'missing_symbol': True},
                     {'wrong_signature': True}, {'other_module': True}):
            caller, dependency = two_symbols(**case)
            consumer.write_bytes(caller)
            provider.write_bytes(dependency)
            sibling.write_bytes(dependency)
            for options in (pair, automatic):
                output = execute(command+options)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError(f'{case} reached the scheduler: '+output)
        peer_file = work/'peer.dis64'
        for failure in (False, True):
            caller, first, peer = sibling_images(failure=failure)
            consumer.write_bytes(caller)
            sibling.write_bytes(first)
            peer_file.write_bytes(peer)
            for jit in (0, 1):
                output = execute(command+['-c', str(jit)]+automatic)
                module, status = ('peer', 'zero divide') if failure else ('caller', '')
                marker = f'native64 scalar module {module}: jit={jit}, exit="{status}"'
                if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                    raise RuntimeError('multi-sibling boot did not complete: '+output)
        for case in ({'second': '../peer'}, {'second': 'unit'}, {'wrong_slot': True},
                     {'mapbyte': 0x80}, {'second': 'missing'}):
            caller, first, peer = sibling_images(**case)
            consumer.write_bytes(caller)
            output = execute(command+automatic)
            if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError(f'{case} reached the scheduler: '+output)
        consumer.write_bytes(sibling_images()[0])
        peer_file.write_bytes(sibling_images()[1])
        output = execute(command+automatic)
        if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
            raise RuntimeError('second sibling with the wrong name reached the scheduler: '+output)
        print('PASS multiple Dis64 sibling providers with C/JIT, unwind and atomic refusal')
        caller, dependency = images(False, calls=2, wrong_slot=True)
        consumer.write_bytes(caller)
        provider.write_bytes(dependency)
        sibling.write_bytes(dependency)
        for options in (pair, automatic):
            output = execute(command+options)
            if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError('mismatched second frame reached scheduler: '+output)
        consumer.write_bytes(images(False)[0])
        for options in (pair[2:], pair+['-X', 'dis64-calls'], ['-P'], automatic[2:],
                        automatic+pair[2:], automatic+['-X', 'dis64-calls']):
            refused = subprocess.run(command+options, capture_output=True, text=True, timeout=20)
            if refused.returncode != 1 or 'Usage: emu' not in refused.stderr+refused.stdout:
                raise RuntimeError('paired boot CLI did not refuse '+repr(options))
        if args.sys_clock:
            builtin = pair[:2]+['-P', '$Sys']
            consumer.write_bytes(clock_image())
            for repeats, copy, load, second in ((1, False, False, None), (3, True, True, None),
                                                (32, True, True, None), (3, True, True, ('sleep', (1,)))):
                consumer.write_bytes(clock_image(repeats=repeats, copy=copy, load=load, second=second))
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+builtin)
                    marker = f'native64 scalar module caller: jit={jit}, exit=""'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('Sys clock file boot did not complete: '+output)
            for case in ({'sleep_only': True}, {'sleep_only': True, 'repeats': 32},
                         {'reverse': True, 'call_index': 1, 'sleep_index': 0,
                          'second': ('sleep', (1,)), 'sleep_tail': True},
                         {'reverse': True, 'call_index': 1, 'sleep_index': 0,
                          'second': ('sleep', (1,)), 'sleep_first': True}):
                consumer.write_bytes(clock_image(load=True, **case))
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+builtin)
                    if f'native64 scalar module caller: jit={jit}, exit=""' not in output or \
                            BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('Sys group order or sleep-only execution failed: '+output)
            sequence = [('dup', (-1, -1)), ('sleep', (1,)), ('millisec', ())]
            for symbols in permutations(('dup', 'millisec', 'sleep')):
                consumer.write_bytes(sys_integer_image(sequence, symbols=symbols))
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+builtin)
                    marker = f'native64 scalar module caller: jit={jit}, exit=""'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('three-symbol Sys group did not complete: '+output)
            invalid = (
                ([('dup', (-1,))], {}),
                ([('dup', (-1, -1, -1))], {}),
                ([('sleep', ())], {}), ([('sleep', (0, 0))], {}),
                ([('millisec', (0,))], {}),
                ([('dup', (-1, -1))], {'signatures': {'dup': (1,)}}),
                ([('dup', (-1, -1))], {'signatures': {'dup': (1, 2)}}),
                ([('dup', (-1, -1))], {'signatures': {'dup': (1, 1, 1)}}),
                ([('dup', (-1, -1))], {'offsets': (64, 72)}),
                ([('dup', (-1, -1))], {'offsets': (64, 64)}),
                ([('dup', (-1, -1))], {'call_index': 1}),
                (sequence, {'call_index': 3}),
                ([('dup', (-1, -1))]*33, {}),
            )
            for sequence, options in invalid:
                consumer.write_bytes(sys_integer_image(sequence, **options))
                output = execute(command+builtin)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('invalid integer-argument Sys group reached scheduler: '+output)
            result, local = ('fp', 72), ('fp', 64)
            variable_cases = (
                ([('dup', (-1, -1)), ('dup', (result, result)), ('sleep', (result,))], {}),
                ([('dup', (local, local)), ('dup', (result, local))], {'initial': -1}),
                ([('sleep', (local,))]*32, {'initial': 0, 'copies': tuple(range(32))}),
                ([('dup', (-1, -1)), ('dup', (local, result))], {'copies': (0,)}),
            )
            for sequence, options in variable_cases:
                consumer.write_bytes(sys_integer_image(sequence, **options))
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+builtin)
                    marker = f'native64 scalar module caller: jit={jit}, exit=""'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('initialized scalar Sys arguments did not complete: '+output)
            for sequence, options in (
                    ([('sleep', (result,))], {}),
                    ([('dup', (local, -1))], {}),
                    ([('sleep', (('fp', 40),))], {'initial': 0}),
                    ([('sleep', (('fp', 68),))], {'initial': 0}),
                    ([('sleep', (('fp', 76),))], {'initial': 0}),
                    ([('sleep', (local,))], {'steps': {0: [('IMOVW', 0, 68), ('IMOVW', 0, 76)]}}),
                    ([('sleep', (('fp', 0),))], {'initial': 0}),
                    ([('sleep', (local,))], {'initial': (0,)*65}),
                    ([('sleep', (local,))], {'initial': (0,)*64, 'copies': (0,)})):
                consumer.write_bytes(sys_integer_image(sequence, **options))
                output = execute(command+builtin)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('uninitialized or reserved scalar Sys input reached scheduler: '+output)
            arithmetic = (
                ([('dup', (('fp', 68), ('fp', 76))), ('dup', (local, ('fp', 68)))],
                 {'steps': {0: [('IMOVW', -1, 64), ('IMOVW', -2, 68), ('IMOVW', -3, 76)],
                            1: [('IADDW', ('fp', 76), 68), ('IMOVW', ('fp', 68), 64)]}}, ''),
                ([('dup', (-1, -1)), ('sleep', (('fp', 76),))],
                 {'copies': {0: 76}, 'steps': {1: [('IADDW', 1, 76)]}}, ''),
                ([('dup', (-1, local))],
                 {'steps': {0: [('IADDW', 1, 64, 2)]}}, ''),
                ([('dup', (-1, local))],
                 {'steps': {0: [('IADDW', 1, 64, -32768)]}}, ''),
                ([('dup', (-1, local))],
                 {'steps': {0: [('IADDW', 1, 64, 32767)]}}, ''),
                ([('sleep', (local,))],
                 {'initial': (0,)*64}, ''),
                ([('dup', (local, local)), ('dup', (result, local))],
                 {'initial': -5, 'steps': {0: [('IADDW', 4, 64)],
                  1: [('IADDW', local, 72), ('IMULW', 2, 64, result),
                      ('IDIVW', 2, 64), ('IXORW', 1, 64)]}}, ''),
                ([('dup', (local, local))],
                 {'initial': -1, 'steps': {1: [('IADDW', 1, 64), ('IDIVW', local, 72)]}}, 'zero divide'),
                ([('dup', (local, local))],
                 {'initial': -1, 'steps': {1: [('IADDW', 1, 64), ('IMODW', local, 72)]}}, 'zero divide'),
            )
            for cell in (64, 68, 76):
                arithmetic += (([('dup', (-1, -1))],
                    {'steps': {0: [('IMOVW', -1, cell)],
                               1: [('IADDW', 1, cell), ('IDIVW', ('fp', cell), 72)]}}, 'zero divide'),)
            for sequence, options, status in arithmetic:
                consumer.write_bytes(sys_integer_image(sequence, **options))
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+builtin)
                    marker = f'native64 scalar module caller: jit={jit}, exit="{status}"'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('Sys scalar expression did not retire correctly: '+output)
            for operation in (
                    ('IADDW', 1, 68),
                    ('IADDW', 1, 76),
                    ('IMOVW', ('fp', 68), 64),
                    ('IMOVW', ('fp', 76), 64),
                    ('IADDW', 1, 64),
                    ('IADDW', 1, 64, local),
                    ('IADDW', ('fp', 72), 64, 1),
                    ('IADDW', 1, 64, 32768),
                    ('IADDW', 1, 64, -32769),
                    ('IADDW', 1, 40, 2),
                    ('IADDW', ('mp', 0), 64, 1),
                    ('IMOVW', 0, 72),
                    ('IADDW', 1, 72, 2),
                    ('IADDW', 1, 64, ('fp', 40)),
                    ('IADDW', 1, 64, ('mp', 0)),
                    ('IMOVW', 0, 70),
                    ('IMOVW', 0, 80),
                    ('IADDL', 1, 64, 2),
                    ('IMOVL', 0, 64)):
                consumer.write_bytes(sys_integer_image([('sleep', (0,))], steps={0: [operation]}))
                output = execute(command+builtin)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('unsafe Sys scalar expression reached the scheduler: '+output)
            if args.limbo is not None:
                for repeats, image, status in compile_clocks(args.limbo.resolve(), work):
                    consumer.write_bytes(image)
                    for jit in (0, 1):
                        output = execute(command+['-c', str(jit)]+builtin)
                        marker = f'native64 scalar module Clock: jit={jit}, exit="{status}"'
                        if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError(f'compiler-produced {repeats}-call clock did not complete: '+output)
                print('PASS Limbo source to Dis64 LOAD and imported clock execution in C/JIT')
            for options in (pair[:2], automatic, pair):
                output = execute(command+options)
                if BOOT['RETIRED'] in output or 'panic:' not in output:
                    raise RuntimeError('Sys clock file bypassed explicit selection: '+output)
            for case in ({'reverse': True, 'load': True, 'second': ('sleep', (1,))},
                         {'sleep_only': True, 'load': True, 'signature': 2},
                         {'sleep_only': True, 'load': True, 'call_index': 1},
                         {'load': True, 'call_index': -1},
                         {'load': True, 'call_index': 1},
                         {'result_offset': 64}, {'module': '$sys'}, {'signature': 0},
                         {'early_copy': True}, {'copy': True, 'copy_destination': 40},
                         {'repeats': 33}, {'load': True, 'load_slot': 0},
                         {'load': True, 'second': ('missing', (1,))},
                         {'load': True, 'second': ('sleep', (2,))},
                         {'load': True, 'second': ('sleep', (1,)), 'call_index': 1},
                         {'load': True, 'sleep_tail': True},
                         {'load': True, 'second': ('sleep', (1,)), 'sleep_tail': True, 'period_offset': 32},
                         {'load': True, 'second': ('sleep', (1,)), 'sleep_tail': True, 'sleep_index': 0},
                         {'load': True, 'second': ('sleep', (1,)), 'sleep_first': True, 'repeats': 32},
                         {'second': ('sleep', (1,))},
                         {'load': True, 'load_index': 1}, {'load': True, 'load_map': b'\x80'}):
                consumer.write_bytes(clock_image(**case))
                output = execute(command+builtin)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError(f'bad Sys clock input {case} reached the scheduler: '+output)
            print('PASS explicit Sys clock file boot with prelink and executing LOAD, C/JIT and admission refusals')
            consumer.write_bytes(images(False)[0])
        trial = ['-X', 'dis64-managed-trial']
        trial_input = ['-A', 'Grüße 世界']
        for direction, accepted, payload, dependency in GC['managed_image_cases'](RAW):
            consumer.write_bytes(payload)
            provider.write_bytes(dependency)
            if accepted:
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+pair+trial)
                    module, status = ('String', 'dereference of nil') if direction else ('caller', '')
                    marker = f'native64 scalar module {module}: jit={jit if direction else 0}, exit="{status}"'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('managed file execution did not retire: '+output)
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+pair+trial+trial_input)
                    if 'native64 scalar module caller: jit=0, exit=""' not in output or \
                            BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('seeded managed file execution did not retire: '+output)
                output = execute(command+pair)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('managed trial bypassed explicit selection: '+output)
                if args.limbo is not None:
                    symbol = 'quoted' if direction else 'unquoted'
                    expression = 'hd value' if direction else 'value :: nil'
                    caller_source, text = GC['managed_sources'](direction)
                    source = work/'managed-provider.b'
                    provider.unlink()
                    compile_command = [str(args.limbo.resolve()), '-o', str(provider)]
                    for flag, code in (('-64', text), ('-64m', text.replace(expression, 'nil')),
                                       ('-64m', text.replace('string', 'int'))):
                        source.write_text(code)
                        refused = subprocess.run(compile_command+[flag, str(source)], capture_output=True, text=True, timeout=30)
                        if refused.returncode <= 0 or provider.exists() or 'Dis64' not in refused.stderr:
                            raise RuntimeError('unsupported managed provider compiled: '+refused.stderr)
                    source.write_text(text)
                    subprocess.run(compile_command+['-64m', str(source)], check=True, timeout=30)
                    for jit in (0, 1):
                        output = execute(command+['-c', str(jit)]+pair+trial)
                        marker = f'native64 scalar module {module}: jit={jit if direction else 0}, exit="{status}"'
                        if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError('compiler-produced managed provider did not retire: '+output)
                    source = work/'managed-caller.b'
                    text = caller_source
                    consumer.unlink()
                    compile_command = [str(args.limbo.resolve()), '-I', str(ROOT/'module'),
                                       '-o', str(consumer)]
                    for flag, code in (('-64', text),
                                       ('-64m', text.replace('String->PATH', '"/wrong.dis"')),
                                       ('-64m', text.replace(f'lib->{symbol}(value)', f'lib->{symbol}(nil)'))):
                        source.write_text(code)
                        refused = subprocess.run(compile_command+[flag, str(source)], capture_output=True,
                                                 text=True, timeout=30)
                        if refused.returncode <= 0 or consumer.exists() or 'Dis64' not in refused.stderr:
                            raise RuntimeError('unsupported managed caller compiled: '+refused.stderr)
                    source.write_text(text)
                    subprocess.run(compile_command+['-64m', str(source)], check=True, timeout=30)
                    for jit in (0, 1):
                        output = execute(command+['-c', str(jit)]+pair+trial)
                        selected = module if direction else 'Caller'
                        marker = f'native64 scalar module {selected}: jit={jit if direction else 0}, exit="{status}"'
                        if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError('compiler-produced managed pair did not retire: '+output)
                        for seed in (trial_input, ['-A', '']):
                            output = execute(command+['-c', str(jit)]+pair+trial+seed)
                            if 'native64 scalar module Caller: jit=0, exit=""' not in output or \
                                    BOOT['RETIRED'] not in output or 'panic:' in output:
                                raise RuntimeError('seeded compiler-produced managed pair did not retire: '+output)
                    statement = f'    result := lib->{symbol}(value);'
                    for repetitions in (2, 3, 32):
                        body = statement+'\n'+f'    result = lib->{symbol}(value);\n'*(repetitions-1)
                        source.write_text(text.replace(statement, body))
                        subprocess.run(compile_command+['-64m', str(source)], check=True, timeout=30)
                        expected = next(raw for d, ok, raw, _ in GC['managed_image_cases'](RAW)
                                        if ok and d == direction and int.from_bytes(code_bytes(raw)[:4], 'big') == 5*repetitions)
                        if code_bytes(consumer.read_bytes()) != code_bytes(expected):
                            raise RuntimeError('compiler changed repeated managed call/cleanup islands')
                        for jit in (0, 1):
                            for seed in (trial_input, ['-A', '']):
                                output = execute(command+['-c', str(jit)]+pair+trial+seed)
                                if 'native64 scalar module Caller: jit=0, exit=""' not in output or \
                                        BOOT['RETIRED'] not in output or 'panic:' in output:
                                    raise RuntimeError('repeated compiler-produced managed pair did not retire: '+output)
                    consumer.unlink()
                    source.write_text(text.replace(statement, statement+'\n'+f'    result = lib->{symbol}(value);\n'*32))
                    refused = subprocess.run(compile_command+['-64m', str(source)], capture_output=True,
                                             text=True, timeout=30)
                    if refused.returncode <= 0 or consumer.exists() or 'Dis64' not in refused.stderr:
                        raise RuntimeError('compiler accepted more than 32 managed calls')
            else:
                output = execute(command+pair+trial)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('invalid managed pair reached scheduler: '+output)
        load_path = '/'+str(provider.relative_to(args.root.resolve()))
        load_cases = GC['managed_image_cases'](RAW, load_path)
        for direction, accepted, payload, dependency in load_cases:
            consumer.write_bytes(payload)
            provider.write_bytes(dependency)
            if accepted:
                for jit in (0, 1):
                    for seed in ([], trial_input, ['-A', '']):
                        output = execute(command+['-c', str(jit)]+pair+trial+seed)
                        module, status = ('String', 'dereference of nil') if direction and not seed else ('caller', '')
                        marker = f'native64 scalar module {module}: jit={jit if direction and not seed else 0}, exit="{status}"'
                        if marker not in output or \
                                BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError('managed filesystem ILOAD did not complete: '+output)
            else:
                output = execute(command+pair+trial)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('malformed managed filesystem image reached scheduler: '+output)
        reload_cases = GC['managed_image_cases'](RAW, load_path, reloads=True)
        for direction, accepted, payload, dependency in reload_cases:
            consumer.write_bytes(payload)
            provider.write_bytes(dependency)
            if accepted:
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+pair+trial+trial_input)
                    if 'native64 scalar module caller: jit=0, exit=""' not in output or \
                            BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('managed file reload did not complete: '+output)
            else:
                output = execute(command+pair+trial)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('malformed managed file reload reached scheduler: '+output)
        if args.limbo is not None:
            for direction in (0, 1):
                text, body = GC['managed_sources'](direction)
                source = work/'load-provider.b'
                source.write_text(body)
                subprocess.run([str(args.limbo.resolve()), '-64m', '-o', str(provider), str(source)], check=True, timeout=30)
                source = work/'load-caller.b'
                symbol = 'quoted' if direction else 'unquoted'
                statement = f'    result := lib->{symbol}(value);'
                text = text.replace('String->PATH', '"'+load_path+'"')
                compile_command = [str(args.limbo.resolve()), '-64l', '-I', str(ROOT/'module'), '-o', str(consumer), str(source)]
                for repetitions in (1, 2, 32):
                    source.write_text(text.replace(statement, statement+'\n'+f'    result = lib->{symbol}(value);\n'*(repetitions-1)))
                    subprocess.run(compile_command, check=True, timeout=30)
                    expected = next(raw for d, ok, raw, _ in load_cases if ok and d == direction and
                                    int.from_bytes(code_bytes(raw)[:4], 'big') == 5*repetitions+1)
                    if code_bytes(consumer.read_bytes()) != code_bytes(expected):
                        raise RuntimeError('compiler changed managed filesystem ILOAD or call islands')
                    for jit in (0, 1):
                        output = execute(command+['-c', str(jit)]+pair+trial+trial_input)
                        if 'native64 scalar module Caller: jit=0, exit=""' not in output or \
                                BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError('compiled managed filesystem ILOAD did not complete: '+output)
                for repetitions, reload_at in ((2, (1,)), (3, (1, 2)), (32, tuple(range(1, 32))), (32, (1, 31))):
                    lines = [statement]
                    for index in range(1, repetitions):
                        if index in reload_at:
                            lines.append('    lib = load String "'+load_path+'";')
                        lines.append(f'    result = lib->{symbol}(value);')
                    source.write_text(text.replace(statement, '\n'.join(lines)))
                    subprocess.run(compile_command, check=True, timeout=30)
                    candidates = GC['managed_image_cases'](RAW, load_path, reloads=reload_at)
                    expected = next(raw for d, ok, raw, _ in candidates if ok and d == direction and
                                    int.from_bytes(code_bytes(raw)[:4], 'big') == 5*repetitions+1+len(reload_at))
                    if code_bytes(consumer.read_bytes()) != code_bytes(expected):
                        raise RuntimeError('compiler changed managed LOAD positions')
                    for jit in (0, 1):
                        for seed in (trial_input, ['-A', '']):
                            output = execute(command+['-c', str(jit)]+pair+trial+seed)
                            if 'native64 scalar module Caller: jit=0, exit=""' not in output or \
                                    BOOT['RETIRED'] not in output or 'panic:' in output:
                                raise RuntimeError('compiled managed reload did not complete: '+output)
                # No dangling LOAD, extra import, or 33rd call may reach the file producer.
                reload_line = '    lib = load String "'+load_path+'";'
                for invalid in (statement+'\n'+reload_line,
                                statement+'\n'+(reload_line+'\n'+f'    result = lib->{symbol}(value);\n')*32,
                                statement+'\n'+reload_line+'\n'+reload_line+'\n'+f'    result = lib->{symbol}(value);'):
                    consumer.unlink(missing_ok=True)
                    source.write_text(text.replace(statement, invalid))
                    refused = subprocess.run(compile_command, capture_output=True, text=True, timeout=30)
                    if refused.returncode <= 0 or consumer.exists() or 'Dis64' not in refused.stderr:
                        raise RuntimeError('compiler accepted an incomplete or excessive managed reload body')
                # Pair mode must not silently discard source-level reloads.
                consumer.unlink(missing_ok=True)
                old_source = text.replace('"'+load_path+'"', 'String->PATH')
                old_source = old_source.replace(statement, statement+'\n    lib = load String String->PATH;\n'+f'    result = lib->{symbol}(value);')
                source.write_text(old_source)
                refused = subprocess.run(['-64m' if item == '-64l' else item for item in compile_command],
                                         capture_output=True, text=True, timeout=30)
                if refused.returncode <= 0 or consumer.exists() or 'Dis64' not in refused.stderr:
                    raise RuntimeError('prelinked mode silently discarded a managed file reload')
                # A serialized path cannot silently bind the explicitly supplied different file.
                source.write_text(text.replace(load_path, '/wrong-provider.dis64'))
                subprocess.run(compile_command, check=True, timeout=30)
                output = execute(command+pair+trial+trial_input)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('managed filesystem path mismatch bypassed admission: '+output)
        print('PASS managed filesystem ILOAD through readmod/parsemod/linkmod, generated hashes, C/JIT and path/map refusals')
        group = runpy.run_path(str(ROOT/'utils/tests/dis64-managed-groups.py'))
        for file_path in (None, load_path):
            for initial, accepted, payload, dependency in group['cases'](RAW, file_path):
                consumer.write_bytes(payload); provider.write_bytes(dependency)
                if accepted:
                    for jit in (0, 1):
                        output = execute(command+['-c', str(jit)]+pair+trial+trial_input)
                        if 'native64 scalar module caller: jit=0, exit=""' not in output or \
                                BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError('two-symbol managed pair did not retire: '+output)
                else:
                    output = execute(command+pair+trial)
                    if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                        raise RuntimeError('invalid managed import group reached the scheduler: '+output)
        if args.limbo is not None:
            for initial in (0, 1):
                for file_mode in (False, True):
                    for calls, reloads in ((4, ()), (32, file_mode)):
                        text, body = group['sources'](initial, calls, reloads, reverse_provider=bool(initial))
                        if file_mode: text = text.replace('String->PATH', '"'+load_path+'"')
                        source = work/'group-provider.b'; source.write_text(body)
                        subprocess.run([str(args.limbo.resolve()), '-64m', '-o', str(provider), str(source)], check=True, timeout=30)
                        source = work/'group-caller.b'; source.write_text(text)
                        subprocess.run([str(args.limbo.resolve()), '-64l' if file_mode else '-64m', '-I', str(ROOT/'module'),
                                        '-o', str(consumer), str(source)], check=True, timeout=30)
                        # Compare complete CODE against independently serialized ordinal orders.
                        candidates = group['cases'](RAW, load_path if file_mode else None)
                        expected_count = 6*calls-1+(calls if file_mode and reloads else int(file_mode))
                        actual = code_bytes(consumer.read_bytes())
                        if int.from_bytes(actual[:4], 'big') != expected_count or not any(
                                ok and initial == direction and code_bytes(image) == actual
                                for direction, ok, image, _ in candidates):
                            raise RuntimeError('compiler changed managed transfer/call count')
                        for jit in (0, 1):
                            for seed in (trial_input, ['-A', ''], []):
                                output = execute(command+['-c', str(jit)]+pair+trial+seed)
                                module, status = ('String', 'dereference of nil') if initial and not seed else ('Caller', '')
                                marker = f'native64 scalar module {module}: jit={jit if initial and not seed else 0}, exit="{status}"'
                                if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                                    raise RuntimeError('compiled dual managed call group did not retire: '+output)
        print('PASS two managed String imports/exports: both ordinal orders, typed result chains, file LOAD, C/JIT and malformed group refusals')
        for options in (pair[:2]+trial, pair+trial+['-X', 'dis64-pair'], pair[:2]+trial+['-P', '$Sys'],
                        pair+trial_input, pair+trial+['-A', 'x'*4097]):
            refused = subprocess.run(command+options, capture_output=True, text=True, timeout=20)
            if refused.returncode != 1 or 'Usage: emu' not in refused.stderr+refused.stdout:
                raise RuntimeError('managed trial CLI did not refuse '+repr(options))
        print('PASS explicit managed file trial: both directions, C/JIT, invalid pairs and options')
        consumer.write_bytes(images(False)[0])
        for dependency in (None, b'invalid provider'):
            if dependency is None:
                provider.unlink()
            else:
                provider.write_bytes(dependency)
            output = execute(command+pair)
            if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError('invalid provider reached the scheduler: '+output)
        sibling.unlink()
        output = execute(command+automatic)
        if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
            raise RuntimeError('missing sibling reached scheduler: '+output)
        for name in ('', '../unit', 'sub/unit', 'unit.dis64', 'unit\0', 'ü', 'x'*256):
            consumer.write_bytes(images(False, name)[0])
            output = execute(command+automatic)
            if 'invalid Dis64 sibling dependency name or path' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError('unsafe sibling name reached lookup: '+repr(name)+' '+output)
        caller, dependency = images(False, 'other')
        consumer.write_bytes(caller)
        (work/'other.dis64').write_bytes(dependency)
        output = execute(command+automatic)
        if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
            raise RuntimeError('sibling with wrong declared name reached scheduler: '+output)
    print('PASS complete Dis64 pair boot: explicit and sibling C/JIT single/repeated and two-symbol calls, return and late unwind, CLI and dependency refusals')


if __name__ == '__main__':
    main()
