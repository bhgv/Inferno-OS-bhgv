#!/usr/bin/env python3
"""Boot an explicitly linked Dis64 pair through the complete emulator."""
import argparse
from pathlib import Path
import runpy
import shlex
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
                missing_symbol=False, wrong_signature=False, other_module=False):
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
    signature = bindings['signature'](1 if wrong_signature else 0)
    provider = RAW['IMAGE'](RAW['SECTIONS'](
        data=words(1, 8, 1, 40), code=words(2)+ret+terminal,
        exports=[binding(export=True, symbol=3 if missing_symbol else 5, entry=1, sig=signature),
                 binding(export=True)],
    ))
    return caller, provider


# Serialize the private Sys clock sequence with the existing indirect LEA codec.
def clock_image(*, result_offset=72, module='$Sys', signature=1, repeats=1,
                copy=False, early_copy=False, copy_destination=64):
    words, ins = RAW['W'], RAW['I']
    bindings = RAW['RAW']['RAW']
    calls = runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))
    frame = ins('IMFRAME', 65, source=0, middle=0, destination=40)
    bind = calls['indirect']('ILEA', result_offset, 40, 32)
    call = ins('IMCALL', 72, source=40, middle=0, destination=0)
    move = ins('IMOVW', 9, source=72, destination=copy_destination)
    group = (move if early_copy else b'')+frame+bind+call+(move if copy else b'')
    count = repeats*(3+copy+early_copy)+1
    names = (module, 'millisec', 'caller')
    pool = words(len(names))+b''.join(words(len(name), *map(ord, name)) for name in names)
    sections = RAW['SECTIONS'](
        name=2, databytes=8,
        imports=[bindings['binding'](module=0, sig=bindings['signature'](signature))],
        types=[RAW['D'](80, 8), RAW['D'](8, 8, b'\x80')],
        code=words(count)+group*repeats+RAW['EXIT'],
    )
    return RAW['IMAGE']([(kind, pool if kind == 7 else body) for kind, body in sections])


# Require the real scheduler to retire after returning or unwinding the pair.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--emu', type=Path, required=True)
    parser.add_argument('--runner', default='')
    parser.add_argument('--sys-clock', action='store_true')
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
            for repeats, copy in ((1, False), (3, True), (32, True)):
                consumer.write_bytes(clock_image(repeats=repeats, copy=copy))
                for jit in (0, 1):
                    output = execute(command+['-c', str(jit)]+builtin)
                    marker = f'native64 scalar module caller: jit={jit}, exit=""'
                    if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                        raise RuntimeError('Sys clock file boot did not complete: '+output)
            for options in (pair[:2], automatic, pair):
                output = execute(command+options)
                if BOOT['RETIRED'] in output or 'panic:' not in output:
                    raise RuntimeError('Sys clock file bypassed explicit selection: '+output)
            for case in ({'result_offset': 64}, {'module': '$sys'}, {'signature': 0},
                         {'early_copy': True}, {'copy': True, 'copy_destination': 40},
                         {'repeats': 33}):
                consumer.write_bytes(clock_image(**case))
                output = execute(command+builtin)
                if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError(f'bad Sys clock input {case} reached the scheduler: '+output)
            print('PASS explicit Sys clock file boot with C/JIT and admission refusals')
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
