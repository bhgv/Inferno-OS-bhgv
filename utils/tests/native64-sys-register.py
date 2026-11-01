#!/usr/bin/env python3
"""Check the actual selected Sys registration body; descriptor/callback OS effects remain bounded."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Generate metadata with host Limbo and exercise real constructors on the selected target.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='native64-sys-register-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        (work/'sys-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', str(ROOT/'module/sys.m')]))
        runt = (ROOT/'libinterp/runt.c').read_text()
        start = runt.index('static const LimboNative64Entry *const native64sysio[]')
        profile = runt[start:runt.index('\n#endif', start)]
        body = (ROOT/'utils/tests/native64-sys-register.c').read_text().replace('SYS_PROFILE', profile).replace('SYS_REGISTRATION', support['function'](runt, 'void\nsysmodinit('))
        unit = work/'test.c'; unit.write_text(support['constructor']()+body)
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        base = ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1', '-DDIS_NATIVE64_SCALAR_BOOT=1', '-DDIS_NATIVE64_BOOT_SYS_IO=1']
        command = compiler+flags+base+includes
        for source in ('libinterp/runt.c', 'emu/port/inferno.c'):
            subprocess.run(command+['-c', str(ROOT/source), '-o', str(work/(Path(source).stem+'.o'))], env=env, check=True)
        binary = work/'test'
        subprocess.run(command+['-ffunction-sections', '-fdata-sections', str(unit), '-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
        subprocess.run(runner+[str(binary)], env=env, check=True)
        for selection, message in ((base[:-1]+['-DDIS_NATIVE64_BOOT_SYS_IO=0'], 'must be 1'),
                                   (base[:-1]+['-DDIS_NATIVE64_BOOT_SYS_IO=2'], 'must be 1'),
                                   ([f for f in base if 'SCALAR_BOOT' not in f], 'requires DIS_NATIVE64_SCALAR_BOOT=1'),
                                   (base+['-DDIS_NATIVE64_SYS_HEADER="sys-native64.h"'], 'owns its generated metadata')):
            result = subprocess.run(compiler+flags+selection+includes+['-fsyntax-only', str(ROOT/'libinterp/runt.c')], env=env, capture_output=True, text=True)
            if result.returncode == 0 or message not in result.stderr:
                raise RuntimeError('Invalid trusted Sys startup was accepted: '+result.stderr)
        print('PASS trusted Sys startup model/metadata refusal controls')


if __name__ == '__main__':
    main()
