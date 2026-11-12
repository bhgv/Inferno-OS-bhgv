#!/usr/bin/env python3
"""Construct actual owned map8 Types/data, inject every refusal and run the real collector."""
import argparse
import os
import re
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep actual heap/collector/protected-constructor bodies; bound only native pools and platform jumps.
def unit(control, clones=False):
    fn = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
    prefix = (ROOT/'libinterp/tests/gc-roots.c').read_text().split('COLLECTOR_IMPLEMENTATION')[0]
    for signature in ('void\nfreetype(', 'void\nerror(', 'int\n__lsan_is_turned_off('):
        prefix = prefix.replace(fn(prefix,signature),'',1)
    prefix += '\n#include <limits.h>\n#include <float.h>\n#include "isa.h"\n#include "dis64-storage-image.h"\n'
    collector = '\n'.join(line for line in (ROOT/'libinterp/gc.c').read_text().splitlines() if not line.startswith('#include'))
    heap = (ROOT/'libinterp/heap.c').read_text()
    heaps = '\n\n'.join(fn(heap,s) for s in ('void\nfreeptrs(', 'void\nfreeheap(', 'void\ndestroy(', 'Type*\ndtype(', 'void\nfreetype(', 'void\ninitmem(', 'Heap*\nheapz('))
    owner = (ROOT/'libinterp/load64.c').read_text()
    bodies = '#define Dis64RuntimeObject (1024U*1024U) // Actual constructor resource policy.\n#define Dis64RuntimeCode 65536U // Actual constructor instruction policy.\n#define Dis64RuntimeBytes (8U*1024U*1024U) // Actual complete-source policy.\n'
    bodies += fn(owner,'static int\ndis64runtimemodel(')+'\n'+owner[owner.index('/* Native staging state is deliberately distinct'):]
    if not clones:
        bodies = bodies.split('\n\ntypedef struct Dis64StorageClone')[0]
    else:
        heaps += '\n'+heap[heap.index('typedef struct ArrayCopy'):heap.index('/* Read packed maps and validate complete spans')]
        heaps += '\n\n'+'\n\n'.join(fn(heap,s) for s in ('void\nfreearray(', 'void\nacheck(', 'Heap*\nnheap(', 'Heap*\nheaparray(', 'void\ninitarray(', 'void\nnewmp('))
        heaps = 'void freearray(Heap*,int);\nvoid *arraycpy(Array*);\n'+next(line for line in heap.splitlines() if line.startswith('Type\tTarray'))+'\n'+heaps
    # Numeric code/entry construction has its own verifier and C/RX execution fixture.
    bodies = bodies.split('\n\ntypedef struct Dis64NumericLoad')[0]
    mutations = {
        'map': ('(uchar*)type.map, type.mapbytes', 'nil, 0'),
        'pin': ('ptradd(heap);', '/* omitted root acquisition */'),
        'release': ('ptrdel(D2H(data));', '/* omitted root retirement */'),
        'types': ('freetype(owner->types[i]);', 'USED(owner->types[i]);'),
        'data': ('if(!dis64initializersapply(', 'if(0 && !dis64initializersapply('),
        'preflight': ('!dis64storageimagecheck(image, bytes, &limits, &view)', '(dis64storageimagecheck(image, bytes, &limits, &view), 0)'),
    }
    if control:
        old,new = mutations[control]
        if bodies.count(old)!=1: raise RuntimeError('storage owner marker drift')
        bodies = bodies.replace(old,new,1)
    heaps = re.sub(r'(?<!->)\bfree\(', 'storagefree(', heaps)
    bodies = re.sub(r'(?<!->)\bfree\(', 'storagefree(', bodies)
    services = (ROOT/'utils/tests/dis64-storage-owner.c').read_text().replace('snprint(diagnostic','snprintf(diagnostic')
    protect = fn((ROOT/'emu/port/dis.c').read_text(),'int\nkprotect(')
    services = services.replace('PROTECTION_IMPLEMENTATION',protect).replace('COLLECTOR_IMPLEMENTATION',collector).replace('HEAP_IMPLEMENTATION',heaps).replace('OWNER_IMPLEMENTATION',bodies)
    return prefix+services


# Reuse independent raw structural fixtures, excluding only their deliberately narrower policy rows.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control',choices=('map','pin','release','types','data','preflight'))
    args = parser.parse_args()
    oracle = runpy.run_path(str(ROOT/'utils/tests/dis64-storage-image.py'))
    rows = [row for row in oracle['cases']() if not row[0].startswith('policy-')]
    word = oracle['words']
    stream = word(len(rows))
    for name,expected,raw,policy in rows:
        encoded = name.encode('ascii')
        stream += word(len(encoded))+encoded+word(expected,len(raw))+raw
    arch = os.environ.get('NATIVE64_ARCH','amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='storage-owner-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work = Path(directory);source = work/'test.c';binary = work/'test'
        source.write_text(unit(args.control));(work/'fixture.bin').write_bytes(stream)
        selections = [('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1'])] if args.control else [
            ('WORD32',['-DDIS_WORD32=1']), ('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']), ('legacy',[])]
        for label,selection in selections:
            command = shlex.split(os.environ.get('CC','cc'))+flags+includes+selection+[str(source),'-o',str(binary)]
            subprocess.run(command,check=True)
            result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(work/'fixture.bin')],capture_output=True,text=True,timeout=60)
            if args.control:
                stage = {'map':'descriptor map','pin':'owned pin','release':'root baseline','types':'descriptor baseline','data':'initialized data','preflight':'construction verdict'}[args.control]
                if result.returncode!=10 or result.stderr.strip()!='storage owner mismatch: '+stage:
                    raise RuntimeError('owner control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS Dis64 storage owner control:',args.control,arch)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(label+': '+result.stdout,end='')



if __name__ == '__main__':
    main()
