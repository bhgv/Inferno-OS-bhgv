#!/usr/bin/env python3
"""Execute live-register ownership hooks through the actual guarded xec and vmachine bodies."""
from pathlib import Path
import argparse
import os
import resource
import runpy
import shlex
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Instrument owner transitions only; preserve the actual executor, scheduler and exception bodies.
def unit(filemode, control):
    support = runpy.run_path(str(ROOT/'libinterp/tests/dis64-vmachine.py'))
    source = support['buildunit'](filemode)
    source = source.replace('#include "vmfetch.h"', '#include "vmfetch.h"\n#include "native64-caller.h"', 1)
    source = source.replace('native64callerenter(p);', 'native64callerenter(p);\n\trequire(p->native64live == &R);', 1)
    # This assertion is inside the instruction loop, so removing enter cannot remove its oracle.
    source = source.replace('if(!vmfetch(&fetched)){', 'require(p->native64live == &R);\n\t\tif(!vmfetch(&fetched)){', 1)
    source = source.replace('handlercalls++;', 'require(currun()->native64live == nil);\n\thandlercalls++;', 1)
    source = source.replace('xec(process);', 'xec(process);\n\trequire(process->native64live == nil);', 1)
    source = source.replace('n->native64live = nil;', 'n->native64live = nil;\n\trequire(n->native64holds == 0 && n->native64epoch == 1);', 1)
    if control == 'enter':
        source = source.replace('native64callerenter(p);', '/* Removed live owner entry control. */', 1)
    if control == 'unwind':
        source = source.replace('native64callerleave(currun());', '/* Removed error retirement control. */', 1)
    return source


# Run target C/RX execution, retaining active negative controls and bounded OS/GC boundaries.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('enter', 'unwind'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    runtime = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    with tempfile.TemporaryDirectory(prefix='native64-live-vm-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory); env['TMPDIR'] = str(work)
        command = compiler+flags+['-DDIS_WORD32=1', '-DDIS_NATIVE64_FIXED_MATH_CALL=1', '-DDIS_NATIVE64_CALLER_GUARD=1']
        if arch == 'arm64':
            command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch != 'amd64':
            parser.error('This owning vmachine gate currently supports amd64 and arm64')
        command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for filemode in (False, True):
            label = 'file' if filemode else 'direct'
            source = work/(label+'.c'); source.write_text(unit(filemode, args.control))
            binary = work/label
            subprocess.run(command+(['-DDIS64_PILOT_LOADER=1'] if filemode else [])+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
            for name in ('pilot42', 'pilot-init42', 'divide-zero', 'shift-invalid'):
                image = work/(name+'.dis64')
                mode = 1 if name == 'divide-zero' else 2 if name == 'shift-invalid' else 0
                data = (runtime['image']([bytes.fromhex('43100000' if mode == 1 else '4e102000'), bytes.fromhex('0f1b')])
                        if mode else bytes.fromhex((ROOT/'libinterp/tests/dis64'/(name+'.hex')).read_text()))
                image.write_bytes(data)
                result = subprocess.run(runner+[str(binary), str(image), str(mode)], env=env, capture_output=True, text=True, timeout=90)
                if args.control:
                    if result.returncode not in (-signal.SIGABRT, 128+signal.SIGABRT) or 'Dis64 runtime check failed' not in result.stderr:
                        raise RuntimeError('Live ownership control missed its predicate: '+args.control+'\n'+result.stdout+result.stderr)
                elif result.returncode:
                    raise RuntimeError('Live vmachine failed: '+label+'/'+name+'\n'+result.stdout+result.stderr)
                print('PASS live vmachine:', arch, label, name, args.control or 'entry/return/unwind', flush=True)


if __name__ == '__main__':
    main()
