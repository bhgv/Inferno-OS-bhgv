#!/usr/bin/env python3
"""Compile complete Sys consumers and execute actual fixed-frame C I/O and UTF handlers."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep old boundary fixtures active under NDEBUG without replacing the owning Sys implementation.
def active(source):
    source = source.replace('#include <assert.h>', '#define syscheck(test) do { if(!(test)) { fprintf(stderr,"Sys predicate failed at %d\\n",__LINE__); abort(); } } while(0) // Active oracle.')
    source = source.replace('assert(', 'syscheck(')
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    return support['portfixture'](source)


# Compile full consumers; execute actual UTF objects and I/O bodies with bounded OS transports.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    env = dict(os.environ)
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    with tempfile.TemporaryDirectory(prefix='native64-sys-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = subprocess.check_output([str(args.limbo.resolve()), '-H', str(ROOT/'module/sys.m')], cwd=work, env=env)
        (work/'sys-native64.h').write_bytes(header)
        (work/'runt.h').write_bytes(header)
        compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/Linux', 'emu/port', 'include', 'libinterp')]
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])]:
            command = compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections']
            objects = []
            for source in ('libinterp/runt.c', 'emu/port/inferno.c'):
                obj = work/(label+'-'+Path(source).stem+'.o')
                subprocess.run(command+['-DDIS_NATIVE64_SYS_HEADER="sys-native64.h"', '-c', str(ROOT/source), '-o', str(obj)], env=env, check=True)
                objects.append(str(obj))
            prefix = support['constructor']().split('#define malloc(n)')[0]
            guard = support['function']((ROOT/'emu/port/dis.c').read_text(), 'int\nkprotect(')
            unit = work/'refusal.c'
            unit.write_text(prefix+guard+'\n\n'+(ROOT/'utils/tests/native64-sys-refusal.c').read_text())
            binary = work/(label+'-refusal')
            subprocess.run(command+[str(unit)]+objects+['-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
            subprocess.run(runner+[str(binary)], env=env, check=True)
            for name, obj, extra in [('io', objects[1], []), ('utf', objects[0], [str(ROOT/'lib9/rune.c')])]:
                unit = work/(name+'.c')
                template = active((ROOT/('libinterp/tests/sys-'+name+'.c')).read_text())
                bodies = ''
                linked = [obj]
                if name == 'io':
                    source = (ROOT/'emu/port/inferno.c').read_text()
                    signatures = ['static void\nsysstatusframe(', 'static int\nsysnativeint(', 'static int\nsysiocount(', 'static void\nsysioframe(', 'static int\nsysiofd(']
                    signatures += ['void\nSys_'+n+'(' for n in ('read', 'readn', 'write', 'pread', 'pwrite')]
                    bodies = '\n'.join(support['function'](source, signature) for signature in signatures if signature in source)
                    linked = []
                unit.write_text(template.replace('IMPLEMENTATION', bodies))
                binary = work/(label+'-'+name)
                subprocess.run(command+[str(unit)]+linked+extra+['-Wl,--gc-sections', '-pthread', '-o', str(binary)], env=env, check=True)
                subprocess.run(runner+[str(binary)], env=env, check=True)
                if name == 'io':
                    for case in range(40):
                        subprocess.run(runner+[str(binary)], env=dict(env, SYS_IO_ERROR=str(case)), check=True)
            print('PASS complete Sys consumers and fixed-frame runtime handlers:', label, flush=True)
        refused = subprocess.run(compiler+flags+includes+['-DDIS_NATIVE64_SYS_HEADER="sys-native64.h"', '-c', str(ROOT/'libinterp/runt.c'), '-o', str(work/'invalid.o')], env=env, capture_output=True, text=True)
        if refused.returncode == 0 or 'requires DIS_WORD32=1' not in refused.stderr:
            raise RuntimeError('Experimental Sys accepted an implicit scalar model')


if __name__ == '__main__':
    main()
