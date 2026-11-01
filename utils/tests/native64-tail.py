#!/usr/bin/env python3
"""Check agreed internal typed-tail preflight and actual dynamic-map collector behavior."""
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


# Keep metadata refusal and actual collector/root failures as independent executable gates.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='native64-tail-', dir=ROOT) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/amd64/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1']), ('legacy-refused', ['-DNATIVE64_REFUSED']), ('packed-refused', ['-DDIS_WORD32=1', '-fpack-struct=1', '-DNATIVE64_REFUSED'])]:
            binary = work/label
            subprocess.run(compiler+flags+model+includes+[str(ROOT/'utils/tests/native64-tail.c'), '-o', str(binary)], env=env, check=True)
            subprocess.run([str(binary)], env=env, check=True)
        limbo = args.limbo.resolve()
        roots = work/'roots.m'
        roots.write_text('Roots: module { Node: adt { next: ref Node; }; copy: fn(source: array of ref Node): array of ref Node; caller: fn(padding: big, result: array of ref Node, tail: array of byte); many: fn('+','.join('a'+str(i)+': array of byte' for i in range(150))+'); };')
        for module, source in [('math', ROOT/'module/math.m'), ('roots', roots)]:
            (work/(module+'-native64.h')).write_bytes(subprocess.check_output([str(limbo), '-H', str(source)], cwd=work, env=env))
        gc = runpy.run_path(str(ROOT/'utils/tests/native64-gc.py'))['unit']()
        gc = gc[:gc.index('/* Exercise real frame/type/heap/collector bodies across every process state')]
        (work/'collector.c').write_text(gc+(ROOT/'utils/tests/native64-tail-gc.c').read_text())
        for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])]:
            binary = work/(label+'-gc')
            command = compiler+flags+model+includes+['-ffunction-sections', '-fdata-sections']
            subprocess.run(command+[str(work/'collector.c'), str(ROOT/'libinterp/heap.c'), '-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
            subprocess.run([str(binary)], env=env, check=True)
            for root in ('fixed', 'tail'):
                result = subprocess.run([str(binary), root], env=env, capture_output=True, text=True)
                if result.returncode != -signal.SIGABRT or 'GC root predicate failed' not in result.stderr:
                    raise RuntimeError('Root-removal control did not reach its active predicate: '+root+'\n'+result.stdout+result.stderr)
            print('PASS typed-tail GC root-removal controls:', label, flush=True)


if __name__ == '__main__':
    main()
