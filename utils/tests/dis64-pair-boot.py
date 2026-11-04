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
def images(failure):
    words, ins = RAW['W'], RAW['I']
    frame = ins('IMFRAME', 65, source=0, middle=0, destination=64)
    call = ins('IMCALL', 72, source=64, middle=0, destination=0)
    binding = RAW['RAW']['RAW']['binding']
    caller = RAW['IMAGE'](RAW['SECTIONS'](
        name=5, databytes=8, imports=[binding(module=0)],
        types=[RAW['D'](80, 8), RAW['D'](8, 8, b'\x80')],
        code=words(3)+frame+call+RAW['EXIT'],
    ))
    terminal = ins('IDIVW', 16, source=0, destination=8) if failure else ins('IRET', 27)
    common = dict(data=words(1, 8, 1, 40),
                  code=words(2)+ins('IADDW', 16, source=2, destination=8)+terminal)
    _, provider = next(GC['provider_cases'](RAW, common))[1]
    return caller, provider


# Require the real scheduler to retire after returning or unwinding the pair.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--emu', type=Path, required=True)
    parser.add_argument('--runner', default='')
    args = parser.parse_args()
    root = args.root.resolve()
    command = shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(root)]
    execute = BOOT['execute']
    with tempfile.TemporaryDirectory(prefix='pair-boot-', dir=root) as directory:
        work = Path(directory)
        consumer, provider = work/'consumer.dis64', work/'provider.dis64'
        pair = ['-d', '/'+str(consumer.relative_to(root)), '-P', '/'+str(provider.relative_to(root))]
        for failure in (False, True):
            caller, dependency = images(failure)
            consumer.write_bytes(caller)
            provider.write_bytes(dependency)
            for jit in (0, 1):
                output = execute(command+['-c', str(jit)]+pair)
                status = 'zero divide' if failure else ''
                module = 'unit' if failure else 'other'
                marker = f'native64 scalar module {module}: jit={jit}, exit="{status}"'
                if marker not in output or BOOT['RETIRED'] not in output or 'panic:' in output:
                    raise RuntimeError('paired boot did not complete: '+output)
        for options in (pair[2:], pair+['-X', 'dis64-calls'], ['-P']):
            refused = subprocess.run(command+options, capture_output=True, text=True, timeout=20)
            if refused.returncode != 1 or 'Usage: emu' not in refused.stderr+refused.stdout:
                raise RuntimeError('paired boot CLI did not refuse '+repr(options))
        for dependency in (None, b'invalid provider'):
            if dependency is None:
                provider.unlink()
            else:
                provider.write_bytes(dependency)
            output = execute(command+pair)
            if 'panic: loading Dis64 pair' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError('invalid provider reached the scheduler: '+output)
    print('PASS complete Dis64 pair boot: C and JIT return/unwind, CLI and provider refusals')


if __name__ == '__main__':
    main()
