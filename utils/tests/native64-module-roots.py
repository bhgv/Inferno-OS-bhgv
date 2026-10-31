#!/usr/bin/env python3
"""Force actual collection during production module instance construction."""
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GC = runpy.run_path(str(ROOT/'utils/tests/native64-gc.py'))


# Keep source replacements explicit for independently compiled construction-root controls.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    compiler = shlex.split(env.get('CC', 'cc'))
    with tempfile.TemporaryDirectory(prefix='native64-module-roots-') as directory:
        work = Path(directory)
        if env.get('NATIVE64_LIMBO'):
            limbo = Path(env['NATIVE64_LIMBO']).resolve()
        else:
            build = dict(env, CFLAGS=shlex.join(flags), INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_LIMBO='1', MKBOOTSTRAP_BINDIR=str(work/'tools'))
            GC['run'](['sh', ROOT/'makemk.sh'], work, build)
            limbo = work/'tools/limbo'
        roots = work/'roots.m'
        roots.write_text('Roots: module { Node: adt { next: ref Node; }; copy: fn(source: array of ref Node): array of ref Node; caller: fn(padding: big, result: array of ref Node, tail: array of byte); many: fn('+', '.join('a'+str(i)+': array of byte' for i in range(150))+'); };')
        for module, source in (('math', ROOT/'module/math.m'), ('roots', roots)):
            (work/(module+'-native64.h')).write_bytes(subprocess.check_output([str(limbo), '-H', str(source)], cwd=work, env=env))
        unit = GC['unit']().replace('#define ArenaSlots 8 ', '#define ArenaSlots 12 ').replace('void\nunload(', 'void\nunusedunload(', 1).replace('int\nmain(int argc, char **argv)', 'int\nnative64_unused_main(int argc, char **argv)', 1)
        source = work/'nested.c'
        source.write_text(unit+(ROOT/'utils/tests/native64-module-roots.c').read_text())
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for label, selection in (('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])):
            command = compiler+flags+selection+includes+['-ffunction-sections', '-fdata-sections']
            objects = []
            for name in ('heap', 'math', 'link'):
                obj = work/(label+'-'+name+'.o')
                path = Path(env.get('NATIVE64_HEAP_SOURCE', ROOT/'libinterp/heap.c')) if name == 'heap' else ROOT/('libinterp/'+name+'.c')
                if name == 'link':
                    path = Path(env.get('NATIVE64_LINK_SOURCE', path)).resolve()
                options = ['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"'] if name == 'math' else []
                GC['run'](command+options+['-c', path, '-o', obj], work, env)
                objects.append(obj)
            binary = work/label
            GC['run'](command+[source]+objects+['-Wl,--gc-sections', '-lm', '-o', binary], work, env)
            GC['run']([binary], work, env)
            print('PASS actual module construction roots:', label, flush=True)


if __name__ == '__main__':
    main()
