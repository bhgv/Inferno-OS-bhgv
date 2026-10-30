#!/usr/bin/env python3
"""Link actual Math C with explicit generated native64 declarations and records."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Preserve actual compiler diagnostics and distinguish build failure from runtime rejection.
def run(command, work, env):
    subprocess.run(list(map(str, command)), cwd=work, env=env, check=True)


# Compile the complete owning translation unit; retain only exercised scalar sections at link.
def main():
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='native64-math-') as temporary:
        work = Path(temporary)
        if env.get('NATIVE64_LIMBO'):
            limbo = Path(env['NATIVE64_LIMBO']).resolve()
        else:
            build = dict(env, CFLAGS=shlex.join(flags), INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_LIMBO='1', MKBOOTSTRAP_BINDIR=str(work/'tools'))
            run(['sh', ROOT/'makemk.sh'], work, build)
            limbo = work/'tools/limbo'
        header = subprocess.check_output([str(limbo), '-H', str(ROOT/'module/math.m')], env=env, cwd=work)
        (work/'math-native64.h').write_bytes(header)
        compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for name, selection in (('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])):
            command = compiler+flags+selection+includes+['-ffunction-sections', '-fdata-sections']
            obj = work/(name+'.o')
            run(command+['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"', '-c', ROOT/'libinterp/math.c', '-o', obj], work, env)
            binary = work/name
            run(command+[ROOT/'utils/tests/native64-math.c', obj, '-Wl,--gc-sections', '-lm', '-o', binary], work, env)
            run([binary], work, env)
        rejected = subprocess.run(compiler+flags+includes+['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"',
            '-c', str(ROOT/'libinterp/math.c'), '-o', str(work/'invalid.o')], cwd=work, env=env, capture_output=True, text=True)
        if rejected.returncode == 0 or 'requires DIS_WORD32=1' not in rejected.stderr:
            raise RuntimeError('experimental Math accepted the legacy scalar model')
        print('PASS complete actual Math compilation and selected generated-entry linkage; no builtin registration or reference-result GC admission')


if __name__ == '__main__':
    main()
