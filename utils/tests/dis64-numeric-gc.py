#!/usr/bin/env python3
"""Run actual numeric VM quanta with production collection between quanta."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Reuse the real execution closure and substitute a sweepable pool for its host allocations.
def unit():
    execution = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-exec.py'))
    extract = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
    source = execution['unit']().split('/* Execute an actual Frame/Modlink/decoder/VM/JIT program')[0]
    source = source.replace('int mutator, nprop, gchalt;', '')
    for signature in ('void\nmarkheap(', 'void*\npoolalloc(', 'void\npoolfree('):
        source = source.replace(extract(source, signature), '', 1)
    collector = (ROOT/'libinterp/gc.c').read_text()
    collector = '\n'.join(line for line in collector.splitlines() if not line.startswith('#include'))
    start = source.index('typedef struct Ptrhash')
    end = source.index('/* Native staging state is deliberately distinct', start)
    source = source[:start]+'''#define malloc(n) trackedallocate(n,0) // Track external root metadata.
'''+collector+'\n#undef malloc\n'+source[end:]
    return source+(ROOT/'utils/tests/dis64-numeric-gc.c').read_text()


# Keep the raw image independent and require the controlled missing trace to be detected.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', nargs='?', const='mp', choices=('mp', 'frame'))
    args = parser.parse_args()
    fixtures = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'))
    instruction = fixtures['I']('IADDW', 16, source=2, destination=8)
    words = fixtures['W']
    raw = fixtures['IMAGE'](fixtures['SECTIONS'](
        data=words(1, 8, 1, 40),
        types=[fixtures['D'](80,8,b'\x00\xc0'), fixtures['D'](16,8,b'\x80')],
        code=words(4)+instruction*3+fixtures['EXIT'],
    ))
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    command = shlex.split(os.environ.get('CC', 'cc'))+flags+['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1']
    if arch in ('arm64', 'riscv64'):
        command += ['-DDIS64_TEST_'+arch.upper()+'=1', '-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
    elif arch != 'amd64':
        raise ValueError('unsupported target '+arch)
    command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='numeric-gc-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        (work/'test.c').write_text(unit())
        (work/'image').write_bytes(raw)
        binary = work/'test'
        subprocess.run(command+[str(work/'test.c'), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], check=True)
        runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
        result = subprocess.run(runner+[str(binary), str(work/'image')]+([args.control] if args.control else []), capture_output=True, text=True, timeout=60)
        if args.control:
            if result.returncode != 10 or result.stderr.strip() != 'numeric GC mismatch: '+args.control+' child was swept':
                raise RuntimeError('mapped trace control failed: '+str(result.returncode)+' '+result.stdout+result.stderr)
            print('PASS numeric GC trace control:', args.control, arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
