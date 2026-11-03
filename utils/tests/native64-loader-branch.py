#!/usr/bin/env python3
"""Validate actual Loader branch snapshot conversion without following any target pointer."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Borrow real headers and protected errors; no decoder, pointer difference or target execution is mocked.
def unit(control):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    loader = (ROOT/'libinterp/loader.c').read_text()
    body = support['function'](loader, 'static WORD\nloaderpc(')
    if control:
        old, new = {
            'width': ('base = (uintptr)m->prog;', 'base = (unsigned int)(uintptr)m->prog;'),
            'bounds': ('if(base > (uintptr)-1-bytes || target < base || target-base >= bytes ||\n\t   (target-base)%sizeof(Inst) != 0)', 'if(0)'),
        }[control]
        if body.count(old) != 1:
            raise RuntimeError('Loader branch control drift: '+control)
        body = body.replace(old, new, 1)
    body += '\n\n'+support['function'](loader, 'static void\nbrunpatch(')
    return support['constructor']()+'\n\n'+(ROOT/'utils/tests/native64-loader-branch.c').read_text().replace('LOADER_BRANCH', body)


# Count only the specific semantic exit; compiler errors, signals and sanitizers invalidate a control.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('width', 'bounds'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    command = shlex.split(env.get('CC', 'cc'))+shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='native64-loader-branch-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        source = work/'fixture.c'
        source.write_text(unit(args.control))
        for bits in ((32,) if args.control else (32, 64)):
            target = work/('word'+str(bits))
            subprocess.run(command+(['-DDIS_WORD32=1'] if bits == 32 else [])+includes+[str(source), '-o', str(target)], env=env, check=True)
            result = subprocess.run(runner+[str(target)], env=env, capture_output=True, text=True, timeout=30)
            if args.control:
                if result.returncode != 10 or 'Loader branch target mismatch' not in result.stderr:
                    raise RuntimeError('Loader branch control missed its predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS Loader branch control:', args.control, arch, flush=True)
            else:
                if result.returncode:
                    raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
