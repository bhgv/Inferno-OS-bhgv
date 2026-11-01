#!/usr/bin/env python3
"""Run a complete experimental scalar-only emulator, including its real OS and pools."""
from pathlib import Path
import argparse
import os
import shlex
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Isolate the emulator process group: Linux cleanexit deliberately kills that group.
def execute(command):
    process = subprocess.Popen(command, start_new_session=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        output, _ = process.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate()
        raise RuntimeError('Emulator did not finish within 20 seconds')
    if process.returncode not in (0, -signal.SIGKILL):
        raise RuntimeError('Unexpected emulator status: '+str(process.returncode)+'\n'+output)
    return output


# Require real scheduler completion and JIT selection, plus closed legacy/builtin admission.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emu', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--runner', default='')
    parser.add_argument('--math', action='store_true', help='require trusted Math registration without file import admission')
    args = parser.parse_args()
    command = shlex.split(args.runner)+[str(args.emu.resolve()), '-v', '-r', str(args.root.resolve())]
    with tempfile.TemporaryDirectory(prefix='scalar-boot-', dir=args.root.resolve()) as directory:
        work = Path(directory)
        for name in ('pilot42', 'pilot-init42'):
            image = work/(name+'.dis64')
            image.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64'/(name+'.hex')).read_text()))
            for jit in (0, 1):
                path = '/'+str(image.relative_to(args.root.resolve()))
                output = execute(command+['-c', str(jit), '-d', path])
                registered = 'native64 startup: trusted Math registered; file imports remain disabled' in output
                if registered != args.math:
                    raise RuntimeError('Unexpected Math startup selection: '+output)
                marker = 'native64 scalar module Pilot: jit='+str(jit)+', exit=""'
                if marker not in output or 'panic:' in output or 'Broken:' in output:
                    raise RuntimeError(name+' did not complete: '+output)
                print('PASS complete emulator:', name, 'JIT='+str(jit))
        legacy = work/'legacy.dis'; legacy.write_bytes(b'legacy image rejected before decoding')
        for path, diagnostic in (('/'+str(legacy.relative_to(args.root.resolve())), 'accepts only checked Dis64 scalar images'),
                                 ('$Sys', 'builtin modules are unavailable'), ('$Math', 'builtin modules are unavailable')):
            output = execute(command+['-d', path])
            if diagnostic not in output or 'native64 scalar module ' in output:
                raise RuntimeError('Admission guard missed '+path+': '+output)
            print('PASS complete emulator rejection:', path)


if __name__ == '__main__':
    main()
