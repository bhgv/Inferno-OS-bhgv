#!/usr/bin/env python3
"""Run the named file-call profile through a complete classic-mk emulator and real OS worker."""
import argparse
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BOOT = runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
INFER = runpy.run_path(str(ROOT/'utils/tests/dis64-call-infer.py'))


# Observe complete startup/retirement; exact result bits are checked by dis64-file-calls.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emu', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--runner', default='')
    args = parser.parse_args()
    command = shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(args.root.resolve())]
    execute = BOOT['execute']
    runs = 0
    with tempfile.TemporaryDirectory(prefix='call-boot-', dir=args.root.resolve()) as directory:
        work = Path(directory)
        for kind, name, data, accepted, _ in INFER['goldens']():
            image = work/(str(kind)+'-'+name+'.dis64')
            image.write_bytes(data)
            path = '/'+str(image.relative_to(args.root.resolve()))
            for jit in (0, 1):
                output = execute(command+['-c', str(jit), '-X', 'dis64-calls', '-d', path])
                if accepted:
                    marker = ': jit='+str(jit)+', exit=""'
                    if marker not in output or BOOT['RETIRED'] not in output or any(s in output for s in ('faults:', 'panic:', 'Broken:')):
                        raise RuntimeError('File-call boot failed '+image.name+': '+output)
                    runs += 1
                elif 'panic: loading ' not in output or BOOT['RETIRED'] in output:
                    raise RuntimeError('File-call rejection failed '+image.name+': '+output)
        # Exercise genuine leaf error transfer and retirement through the complete worker.
        failure = work/'divide-zero.dis64'
        failure.write_bytes(bytes.fromhex((INFER['BASE']['GOLDENS']/'divide-zero.hex').read_text()))
        for jit in (0, 1):
            output = execute(command+['-c', str(jit), '-X', 'dis64-calls', '-d', '/'+str(failure.relative_to(args.root.resolve()))])
            if ': jit='+str(jit)+', exit="zero divide"' not in output or BOOT['RETIRED'] not in output or 'Broken: "zero divide"' not in output:
                raise RuntimeError('Leaf error did not retire: '+output)
        # CLI policy is startup-only and opt-in, regardless of option order.
        path = '/'+str((work/'1-normal.dis64').relative_to(args.root.resolve()))
        for options in (['-X', 'dis64-calls'], ['-X', 'unknown', '-d', path], ['-X'], ['-I', '-X', 'dis64-calls']):
            refused = subprocess.run(command+options, start_new_session=True, capture_output=True, text=True, timeout=20)
            output = refused.stdout+refused.stderr
            if refused.returncode != 1 or 'Usage: emu' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError('CLI refusal missed: '+repr(options)+'\n'+output)
        output = execute(command+['-d', path])
        if 'panic: loading ' not in output or BOOT['RETIRED'] in output:
            raise RuntimeError('Default profile unexpectedly admitted calls: '+output)
        output = execute(command+['-c', '1', '-d', path, '-X', 'dis64-calls'])
        if ': jit=1, exit=""' not in output or BOOT['RETIRED'] not in output:
            raise RuntimeError('Reordered CLI selection failed: '+output)
        pilot = work/'pilot.dis64'
        pilot.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64/pilot42.hex').read_text()))
        for path in ('/'+str(pilot.relative_to(args.root.resolve())), '$Sys'):
            output = execute(command+['-X', 'dis64-calls', '-d', path])
            if 'panic: loading ' not in output or BOOT['RETIRED'] in output:
                raise RuntimeError('Cross-profile fallback escaped: '+output)
    print('PASS complete file-call emulator:', runs+1, 'C/RX successful OS roots plus two leaf-error roots; CLI/default/cross-profile refusals; real startup/vmachine/error/retirement')


if __name__ == '__main__':
    main()
