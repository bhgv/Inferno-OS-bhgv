#!/usr/bin/env python3
"""Register the complete actual Math module using explicit generated native64 declarations."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Link explicit source-built classic-mk numeric libraries, not replacement numeric callbacks.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--libdir', type=Path, required=True)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    library = args.libdir.resolve()
    limbo = args.limbo.resolve()
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    constructor = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['constructor']
    with tempfile.TemporaryDirectory(prefix='native64-math-register-', dir=ROOT) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        (work/'math-native64.h').write_bytes(subprocess.check_output([str(limbo), '-H', str(ROOT/'module/math.m')], cwd=work, env=env))
        (work/'test.c').write_text(constructor()+(ROOT/'utils/tests/native64-math-register.c').read_text())
        compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/amd64/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])]:
            command = compiler+flags+model+includes
            obj = work/(label+'.o')
            subprocess.run(command+['-DDIS_NATIVE64_MATH_REGISTER=1', '-DDIS_NATIVE64_MATH_HEADER="math-native64.h"', '-c', str(ROOT/'libinterp/math.c'), '-o', str(obj)], env=env, check=True)
            binary = work/label
            subprocess.run(command+[str(work/'test.c'), str(obj), '-Wl,--wrap=fmtinstall', '-Wl,--start-group', str(library/'libmath.a'), str(library/'lib9.a'), '-Wl,--end-group', '-lm', '-pthread', '-o', str(binary)], env=env, check=True)
            subprocess.run([str(binary)], env=env, check=True)
            print('PASS complete Math native64 registration:', label, flush=True)
        refused = subprocess.run(compiler+flags+includes+['-DDIS_NATIVE64_MATH_REGISTER=1', '-c', str(ROOT/'libinterp/math.c'), '-o', str(work/'invalid.o')], env=env, capture_output=True, text=True)
        if refused.returncode == 0 or 'requires an explicit generated header' not in refused.stderr:
            raise RuntimeError('Registration accepted an implicit legacy header')
        for value in ('0', '2'):
            refused = subprocess.run(compiler+flags+['-DDIS_WORD32=1']+includes+['-DDIS_NATIVE64_MATH_REGISTER='+value, '-DDIS_NATIVE64_MATH_HEADER=\"math-native64.h\"', '-c', str(ROOT/'libinterp/math.c'), '-o', str(work/'invalid.o')], env=env, capture_output=True, text=True)
            if refused.returncode == 0 or 'must be 1 when explicitly enabled' not in refused.stderr:
                raise RuntimeError('Registration accepted an ambiguous enable value: '+value)


if __name__ == '__main__':
    main()
