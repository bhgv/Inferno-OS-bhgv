#!/usr/bin/env python3
"""Trace generated frames with actual heap/Math/collector implementations and bounded pool services."""
import os
import resource
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Preserve actual build failures separately from active runtime root predicates.
def run(command, work, env):
    subprocess.run(list(map(str, command)), cwd=work, env=env, check=True)


# Reuse only bounded test services; include the complete current production collector body.
def unit():
    prefix = (ROOT/'libinterp/tests/gc-roots.c').read_text().split('/* Retain exception cycles')[0]
    start = prefix.index('/* Count descriptor ownership')
    end = prefix.index('COLLECTOR_IMPLEMENTATION', start)
    prefix = prefix[:start]+prefix[end:]
    prefix = prefix.replace('void (*heapmonitor)(int, void*, ulong);', 'extern void (*heapmonitor)(int, void*, ulong);')
    collector = (ROOT/'libinterp/gc.c').read_text()
    collector = '\n'.join(line for line in collector.splitlines() if not line.startswith('#include'))
    return prefix.replace('COLLECTOR_IMPLEMENTATION', collector)+(ROOT/'utils/tests/native64-gc.c').read_text()


# Build real Limbo unless an exact matched source-built executable is supplied by the layout suite.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    compiler = shlex.split(env.get('CC', 'cc'))
    with tempfile.TemporaryDirectory(prefix='native64-gc-') as temporary:
        work = Path(temporary)
        if env.get('NATIVE64_LIMBO'):
            limbo = Path(env['NATIVE64_LIMBO']).resolve()
        else:
            build = dict(env, CFLAGS=shlex.join(flags), INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_LIMBO='1', MKBOOTSTRAP_BINDIR=str(work/'tools'))
            run(['sh', ROOT/'makemk.sh'], work, build)
            limbo = work/'tools/limbo'
        roots = work/'roots.m'
        roots.write_text('Roots: module { Node: adt { next: ref Node; }; copy: fn(source: array of ref Node): array of ref Node; caller: fn(result: array of ref Node); many: fn('+', '.join('a'+str(i)+': array of byte' for i in range(150))+'); };')
        for module, source in (('math', ROOT/'module/math.m'), ('roots', roots)):
            (work/(module+'-native64.h')).write_bytes(subprocess.check_output([str(limbo), '-H', str(source)], cwd=work, env=env))
        source = work/'collector.c'
        source.write_text(unit())
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for label, selection in (('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])):
            command = compiler+flags+selection+includes+['-ffunction-sections', '-fdata-sections']
            objects = []
            for name in ('heap', 'math'):
                obj = work/(label+'-'+name+'.o')
                options = ['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"'] if name == 'math' else []
                run(command+options+['-c', ROOT/('libinterp/'+name+'.c'), '-o', obj], work, env)
                objects.append(obj)
            binary = work/label
            run(command+[source]+objects+['-Wl,--gc-sections', '-lm', '-o', binary], work, env)
            run([binary], work, env)
            for control in ('arguments', 'temporary', 'caller', 'wide'):
                rejected = subprocess.run([str(binary), control], cwd=work, env=env, capture_output=True, text=True)
                if rejected.returncode != -signal.SIGABRT or 'GC root predicate failed' not in rejected.stderr:
                    raise RuntimeError('root-removal control did not reach its active runtime predicate: '+control+'\n'+rejected.stdout+rejected.stderr)
            print('PASS native64 GC negative controls:', label, 'argument/temporary/caller/wide roots')
        print('PASS generated native64 frame maps with actual dtype, heap, C calls, arraycpy and collector; bounded pool, not runtime registration')


if __name__ == '__main__':
    main()
