#!/usr/bin/env python3
"""Exercise actual hosted pool geometry with a bounded OS arena and active mutation control."""
from pathlib import Path
import os
import shlex
import signal
import resource
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Compile the owning allocator, preserving active checks under optimization.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O2 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    aliases = ['-Dmalloc=pooltestmalloc', '-Dfree=pooltestfree', '-Drealloc=pooltestrealloc']
    with tempfile.TemporaryDirectory(prefix='pool-native64-', dir=os.environ.get('NATIVE64_TMPDIR')) as directory:
        work = Path(directory)
        env = dict(os.environ, TMPDIR=str(work))
        for label, replacement in (('actual', None), ('old-32-byte-blocks', '31'), ('missing-size-guards', 'sizes'), ('missing-new-owner-tag', 'tag'), ('inverted-tag-read', 'read')):
            source = (ROOT/'utils/tests/pool-native64.c').read_text()
            source = source.replace('#include "../../emu/port/alloc.c"', '#include "'+str(ROOT/'emu/port/alloc.c')+'"')
            if replacement:
                allocator = (ROOT/'emu/port/alloc.c').read_text()
                if replacement == 'sizes':
                    allocator = allocator.replace('if(size >= 1024UL*1024*1024)', 'if(0)')
                elif replacement == 'tag':
                    allocator = allocator.replace('setmalloctag(nv, getcallerpc(&v));', 'setmalloctag(v, getcallerpc(&v));')
                elif replacement == 'read':
                    allocator = allocator.replace('if(Npadlong > ReallocOffset)', 'if(Npadlong <= ReallocOffset)')
                else:
                    allocator = allocator.replace('#include "error.h"', '#include "error.h"\n#undef POOLQUANTA\n#define POOLQUANTA 31 // Deliberately undersized LP64 control.')
                source = source.replace('#include "'+str(ROOT/'emu/port/alloc.c')+'"', allocator)
                if replacement == '31':
                    source = source.replace('if(POOLQUANTA+1 < sizeof(Bhdr)+sizeof(Btail))', 'if(0)')
            unit = work/(label+'.c'); unit.write_text(source)
            binary = work/label
            subprocess.run(compiler+flags+aliases+includes+['-ffunction-sections', '-fdata-sections', str(unit), '-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], capture_output=True, text=True, timeout=30)
            if label == 'actual':
                if result.returncode:
                    raise RuntimeError('status '+str(result.returncode)+': '+result.stdout+result.stderr)
                print(result.stdout, end='')
            elif replacement in ('sizes', 'tag', 'read'):
                expected = 8 if replacement == 'sizes' else 11
                if result.returncode != expected:
                    raise RuntimeError('Allocation API control missed its oracle: '+label+' status '+str(result.returncode))
                print('PASS allocation API control:', label)
            elif result.returncode not in (-signal.SIGABRT, 128+signal.SIGABRT, 2, 5):
                raise RuntimeError('Old block geometry did not fail the active pool/payload oracle: '+str(result.returncode))
            else:
                print('PASS negative control: old LP64 blocks fail actual pool/payload checks')


if __name__ == '__main__':
    main()
