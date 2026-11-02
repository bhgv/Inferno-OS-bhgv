#!/usr/bin/env python3
"""Execute trusted native local calls without admitting a new Dis64 file ABI."""
import argparse
import os
import resource
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Link real allocation, dispatch and return bodies to the existing bounded heap fixture.
def unit(control=False):
    runtime = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    source = runtime['buildunit']().split('/* Set up an actual root extent, execute to IEXIT, then unwind actual owned storage. */')[0]
    extract = runtime['function']
    stack = (ROOT/'libinterp/stack.c').read_text()
    xec = (ROOT/'libinterp/xec.c').read_text()
    parts = [extract(stack, signature) for signature in ('void\nextend(', 'void\nunextend(')]
    parts += [extract(xec, signature) for signature in ('static Type*\nmoduletype(', 'static int\nframefits(', 'OP(frame)', 'OP(call)', 'OP(ret)', 'OP(movp)')]
    if control:
        source = source.replace('if(valid){', 'if(0 && valid){', 1)
    return source+'\n\n'+'\n\n'.join(parts)+'\n\n'+(ROOT/'utils/tests/native64-local-calls.c').read_text()


# Use the same native and QEMU target configuration as the complete port gate.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', action='store_true', help='remove pending-frame ownership recovery')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='native64-local-calls-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        command = compiler+flags+['-DDIS_WORD32=1']
        if arch == 'arm64':
            command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch == 'riscv64':
            command += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
        elif arch != 'amd64':
            raise RuntimeError('unsupported target: '+arch)
        command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        source = work/'calls.c'
        source.write_text(unit(args.control))
        binary = work/'calls'
        subprocess.run(command+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
        if args.control:
            for jit in (0, 1):
                for grow in (0,):
                    result = subprocess.run(runner+[str(binary), str(jit), str(grow)], env=env, capture_output=True, text=True, timeout=90)
                    if result.returncode != 10 or 'unentered frame owner leaked' not in result.stderr:
                        raise RuntimeError('pending ownership control missed its semantic predicate: '+result.stdout+result.stderr)
                    print('PASS pending ownership removal control:', arch, 'jit='+str(jit), 'grow='+str(grow), flush=True)
        else:
            subprocess.run(runner+[str(binary)], env=env, check=True, timeout=90)


if __name__ == '__main__':
    main()
