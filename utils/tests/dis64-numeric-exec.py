#!/usr/bin/env python3
"""Execute new owned MP-only numeric Modules through actual C/RX VM entries and error unwind."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Keep actual generated code/decoder/executor; count only actual native fast returns, never fallback completion.
def unit():
    base=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    fn=base['function'];source=base['buildunit']().split('/* Set up an actual root extent, execute to IEXIT, then unwind actual owned storage. */')[0]
    source=source.replace('#include "dis64-call-infer.h"','#include "dis64-call-infer.h"\n#include "dis64-numeric-storage.h"',1)
    # The root metadata uses actual collector add/del bodies; collection is a separate acceptance fixture.
    gc=(ROOT/'libinterp/gc.c').read_text()
    roots=gc[gc.index('typedef struct Ptrhash'):gc.index('\n\tint\tnprop;')]
    roots+='\n#define PTRHASH 32 // Actual native collector external-root bucket population.\nstatic Ptrhash *ptrtab[PTRHASH], *ptrfree; // Actual root registry/cache.\n'
    roots+=next(line for line in gc.splitlines() if line.startswith('#define\tHASHPTR'))+'\n'
    roots+='#define malloc(n) trackedallocate(n,0) // Only root metadata allocation is intercepted here.\n'+fn(gc,'void\nptradd(')+'\n'+fn(gc,'void\nptrdel(')+'\n#undef malloc\n'
    load=(ROOT/'libinterp/load64.c').read_text();numeric=load[load.index('/* Native staging state is deliberately distinct'):]
    # Independent storage cloning has its own real newmp/arraycpy/collector closure.
    numeric=numeric[:numeric.index('typedef struct Dis64StorageClone')]+numeric[numeric.index('/* Retain prepared storage in an empty native Module'): ]
    fixture=(ROOT/'utils/tests/dis64-numeric-exec.c').read_text().replace('ROOT_IMPLEMENTATION',roots).replace('NUMERIC_IMPLEMENTATION',numeric)
    names=('movw','movl','movf','addw','addl','subw','subl','mulw','mull','andw','andl','orw','orl','xorw','xorl','divw','divl','modw','modl','shlw','shrw','shll','shrl','beqw','bnew','bltw','blew','bgtw','bgew','beql','bnel','bltl','blel','bgtl','bgel','beqf','bnef','bltf','blef','bgtf','bgef','jmp','iexit')
    fixture=fixture.replace('INIT_HANDLERS','\n'.join('optab[I'+('EXIT' if n=='iexit' else n.upper())+']='+n+';' for n in names))
    read='''/* Read independent big-endian fixture words without a production writer. */
static unsigned int
readword(FILE *file)
{
    unsigned char b[4];
    require(fread(b,1,4,file)==4);
    return (unsigned int)b[0]<<24 | (unsigned int)b[1]<<16 | (unsigned int)b[2]<<8 | b[3];
}

'''
    # The observer is declared before the actual VM loop; bodies remain unchanged except this call site.
    source=source.replace('void\nxec(Prog *p)', 'static int numericfastrun(int(*)(REG*),REG*); // Fixture-only actual RX observation.\n\nvoid\nxec(Prog *p)',1)
    if source.count('if(fast(&R))')!=1: raise RuntimeError('VM fast entry marker drift')
    source=source.replace('if(fast(&R))','if(numericfastrun(fast,&R))',1)
    return source+read+fixture


# Compile for the actual selected target, and keep an all-C decline control separate from true RX execution.
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--control',action='store_true');args=parser.parse_args()
    data=runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'));word=data['W'];rows=[row for row in data['cases']() if row[5] != 0];stream=word(len(rows))
    for name,accepted,raw,value,offset,width,error in rows:
        name=name.encode();stream+=word(len(name))+name+word(accepted,value>>32,value&0xffffffff,offset,width,error,len(raw))+raw
    arch=os.environ.get('NATIVE64_ARCH','amd64');flags=shlex.split(os.environ.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    command=shlex.split(os.environ.get('CC','cc'))+flags+['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']
    if arch=='arm64':command+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
    elif arch=='riscv64':command+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
    elif arch!='amd64':raise RuntimeError('Unsupported native target')
    command+=['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
    with tempfile.TemporaryDirectory(prefix='numeric-exec-',dir=os.environ.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);(work/'test.c').write_text(unit());(work/'fixtures').write_bytes(stream);binary=work/'test'
        subprocess.run(command+[str(work/'test.c'),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)],check=True)
        result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),str(work/'fixtures')]+(['decline'] if args.control else []),capture_output=True,text=True,timeout=60)
        if args.control:
            if result.returncode!=10 or result.stderr.strip()!='numeric execution mismatch: RX completion':raise RuntimeError('RX decline control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
            print('PASS private numeric RX decline control:',arch)
        elif result.returncode:raise RuntimeError(result.stdout+result.stderr)
        else:print(result.stdout,end='')


if __name__=='__main__':
    main()
