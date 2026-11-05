#!/usr/bin/env python3
"""Boot an explicitly linked Dis64 pair through the complete emulator."""
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
                sleep_tail=False, period=0, period_offset=64, sleep_index=1):
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
    if sleep_tail:
        tail = (ins('IMFRAME', 65, source=slot, middle=1, destination=40)
                + calls['indirect']('IMOVW', period, 40, period_offset, sm=2)
                + bind
                + ins('IMCALL', 72, source=40, middle=sleep_index, destination=slot))
        count += 4
    names = (module, 'millisec', 'caller')
    imports = [bindings['binding'](module=0, sig=bindings['signature'](signature))]
    if second is not None:
        symbol, parameters = second
        names += (symbol,)
        imports.append(bindings['binding'](module=0, symbol=3,
                                          sig=bindings['signature'](1, parameters)))
    pool = words(len(names))+b''.join(words(len(name), *map(ord, name)) for name in names)
    sections = RAW['SECTIONS'](
        name=2, databytes=16 if load else 8,
        imports=imports,
        types=[RAW['D'](80, 8), RAW['D'](16, 8, load_map) if load else RAW['D'](8, 8, b'\x80')],
        code=words(count)+prefix+group*repeats+tail+RAW['EXIT'],
    )
    return RAW['IMAGE']([(kind, pool if kind == 7 else body) for kind, body in sections])


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

    def code_bytes(image):
        # The compiler puts CODE second, as does the independent fixture.
        kind, _, offset, size = struct.unpack_from('>HHII', image, 44)
        if kind != 2 or len(image) < offset+size:
            raise RuntimeError('invalid clock CODE section')
        return image[offset:offset+size]

    command = [str(limbo), '-I', str(ROOT/'module'), '-64', '-o', str(output), str(source)]
    for bad in (template.replace('Sys->PATH', '"$Other"'),
                template.replace('millisec()', 'sleep(1)'),
                repeated(33),
                repeated(3).replace('    now = sys->millisec();\n', '    now++;\n', 1)):
        source.write_text(bad)
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        if result.returncode <= 0 or output.exists() or 'requires managed linkage' not in result.stderr:
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
        yield repeats, image
    mixed = template.replace('    now := sys->millisec();', '    now := sys->millisec();\n    now = sys->sleep(0);')
    output.unlink()
    source.write_text(mixed.replace('sleep(0)', 'sleep(1)'))
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    if result.returncode <= 0 or output.exists() or 'requires managed linkage' not in result.stderr:
        raise RuntimeError('compiler did not refuse a positive sleep period: '+result.stderr)
    source.write_text(mixed)
    subprocess.run(command, check=True, timeout=30)
    image = output.read_bytes()
    expected = clock_image(load=True, second=('sleep', (1,)), sleep_tail=True)
    if code_bytes(image) != code_bytes(expected):
        raise RuntimeError('compiler did not preserve the checked sleep call')
    yield 'clock plus zero sleep', image


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
            if args.limbo is not None:
                for repeats, image in compile_clocks(args.limbo.resolve(), work):
                    consumer.write_bytes(image)
                    for jit in (0, 1):
                        output = execute(command+['-c', str(jit)]+builtin)
                        marker = f'native64 scalar module Clock: jit={jit}, exit=""'
                        if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                            raise RuntimeError(f'compiler-produced {repeats}-call clock did not complete: '+output)
                print('PASS Limbo source to Dis64 LOAD and imported clock execution in C/JIT')
            for options in (pair[:2], automatic, pair):
                output = execute(command+options)
                if BOOT['RETIRED'] in output or 'panic:' not in output:
                    raise RuntimeError('Sys clock file bypassed explicit selection: '+output)
            for case in ({'result_offset': 64}, {'module': '$sys'}, {'signature': 0},
                         {'early_copy': True}, {'copy': True, 'copy_destination': 40},
                         {'repeats': 33}, {'load': True, 'load_slot': 0},
                         {'load': True, 'second': ('missing', (1,))},
                         {'load': True, 'second': ('sleep', (2,))},
                         {'load': True, 'second': ('sleep', (1,)), 'call_index': 1},
                         {'load': True, 'sleep_tail': True},
                         {'load': True, 'second': ('sleep', (1,)), 'sleep_tail': True, 'period': 1},
                         {'load': True, 'second': ('sleep', (1,)), 'sleep_tail': True, 'period_offset': 32},
                         {'load': True, 'second': ('sleep', (1,)), 'sleep_tail': True, 'sleep_index': 0},
                         {'second': ('sleep', (1,))},
                         {'load': True, 'load_index': 1}, {'load': True, 'load_map': b'\x80'}):
                consumer.write_bytes(clock_image(**case))
                output = execute(command+builtin)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError(f'bad Sys clock input {case} reached the scheduler: '+output)
            print('PASS explicit Sys clock file boot with prelink and executing LOAD, C/JIT and admission refusals')
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
