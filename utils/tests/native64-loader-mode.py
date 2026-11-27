#!/usr/bin/env python3
"""Test actual Loader_compile policy before MP mutation or protected compiler entry."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Borrow production functions, retaining real public result/state behavior on each target.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', action='store_true')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    extract = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    body = (ROOT/'libinterp/loader.c').read_text()
    parts = [extract(body, s) for s in ('static void\nloaderword(', 'static int\nloadercompile(', 'void\nLoader_compile(')]
    if args.control:
        marker = 'if(m->cstep != nil)'
        if parts[-1].count(marker) != 1:
            raise RuntimeError('public loader guard marker drift')
        parts[-1] = parts[-1].replace(marker, 'if(0)')
    source = (ROOT/'utils/tests/native64-loader-mode.c').read_text().replace('LOADER_IMPLEMENTATION', '\n\n'.join(parts))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='loader-mode-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        path = work/'fixture.c'
        path.write_text(source)
        includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for bits in ((32,) if args.control else (32, 64)):
            binary = work/('word'+str(bits))
            subprocess.run(compiler+flags+includes+(['-DDIS_WORD32=1'] if bits == 32 else [])+[str(path), '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=30)
            if args.control:
                if result.returncode != 10 or 'loader compile mode mismatch' not in result.stderr:
                    raise RuntimeError('loader control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS public loader mode control:', arch, flush=True)
            else:
                if result.returncode:
                    raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
