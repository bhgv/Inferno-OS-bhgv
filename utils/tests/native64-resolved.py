#!/usr/bin/env python3
"""Exercise late result resolution with actual typed argument maps, collector and port error owners."""
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


# Retain owning producer/protection bodies and replace only the independent transaction entry point.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-result.py'))
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='native64-resolved-', dir=ROOT) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        limbo = args.limbo.resolve()
        roots = work/'roots.m'
        roots.write_text('Roots: module { Node: adt { next: ref Node; }; copy: fn(source: array of ref Node): array of ref Node; caller: fn(padding: big, result: array of byte, tail: array of byte); many: fn('+','.join('a'+str(i)+': array of byte' for i in range(150))+'); };')
        for name, source in [('math', ROOT/'module/math.m'), ('roots', roots)]:
            (work/(name+'-native64.h')).write_bytes(subprocess.check_output([str(limbo), '-H', str(source)], cwd=work, env=env))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        compiler = shlex.split(env.get('CC', 'cc'))
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])]:
            command = compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections']
            objects = []
            for name in ('heap', 'math'):
                obj = work/(label+'-'+name+'.o')
                options = ['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"'] if name == 'math' else []
                subprocess.run(command+options+['-c', str(ROOT/('libinterp/'+name+'.c')), '-o', str(obj)], env=env, check=True)
                objects.append(str(obj))
            for port in ('emu', 'os'):
                source = support['unit'](port).replace('int\nmain(void)', 'int\nnative64_original_result_main(void)', 1)
                source += (ROOT/'utils/tests/native64-resolved.c').read_text()
                unit = work/(port+'.c')
                unit.write_text(source)
                binary = work/(label+'-'+port)
                subprocess.run(command+[str(unit)]+objects+['-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
                subprocess.run([str(binary)], env=env, check=True)
                for control in ('stale', 'arguments'):
                    result = subprocess.run([str(binary), control], env=env, capture_output=True, text=True)
                    if result.returncode != -signal.SIGABRT or 'GC root predicate failed' not in result.stderr:
                        raise RuntimeError('Resolved-result negative control failed to reach its predicate: '+control+'\n'+result.stdout+result.stderr)
                print('PASS resolved result/root controls:', port, label, flush=True)


if __name__ == '__main__':
    main()
