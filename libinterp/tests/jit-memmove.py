#!/usr/bin/env python3
"""Check production JIT dispatch and execute VM block moves against snapshot copies.

The dispatch recorder is a boundary stub. Native punt addressing/call/return
is covered separately by jit-handoff.py; this test executes the C move bodies.
Counts are valid nonnegative VM counts, not malformed-memory validation.
"""
import ctypes as C
import os
from pathlib import Path
import random
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'libinterp/xec.c').read_text()
def op(name):
    a=source.index('OP('+name+')');b=source.index('{',a);depth=1;e=b+1
    while depth:
        depth+=(source[e]=='{')-(source[e]=='}');e+=1
    return source[a:e]
def dispatch(arch):
    s=(root/f'libinterp/comp-{arch}.c').read_text();start=s.index('comp(Inst *i)')
    blocks=s[start:].split('\t\tbreak;')
    selected=[]
    for b in blocks:
        labels=re.findall(r'\tcase (\w+):',b)
        if any(n in labels for n in ['IMOVM','IHEADM']):
            b=b[b.index('\tcase '):]
            # Deliberately compile the actual dispatch block: a native loop
            # is not accepted silently as a successful helper handoff.
            selected.append(b+'\t\tbreak;')
    assert selected,arch
    return '\n'.join(selected)
unit=r'''
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "isa.h"
#if VM_WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef struct List {uintptr_t header[3];unsigned char data[4096];} List;
static struct {void *s,*m,*d;} R;
#define OP(n) static void n(void)
#define W(r) (*(WORD*)R.r)
#define L(r) (*(List**)R.r)
'''+op('movm')+'\n'+op('headm')+r'''
enum {SRCOP=1,DSTOP=2,THREOP=64};
typedef struct Inst {int op;} Inst;
static void (*optab[256])(void);
static int observed;
static void punt(Inst *i,int mask,void(*fn)(void)){
 if(fn!=optab[i->op])abort();observed=mask;
}
int route(int arch,int head){
 Inst obj={head?IHEADM:IMOVM},*i=&obj;observed=0;
 if(arch==0){switch(i->op){X86 default:abort();}}
 else {switch(i->op){ARM32 default:abort();}}
 return observed;
}
void run(unsigned char *bytes,int n,int s,int d,int head,int aliascount){
 WORD length=n;List *l=(List*)bytes;
 R.s=head?(void*)&l:bytes+s;R.d=bytes+d;
 if(aliascount){memcpy(bytes+d,&length,sizeof length);R.m=bytes+d;}
 else R.m=&length;
 if(head)headm();else movm();
}
int dataoffset(void){List l;return l.data-(unsigned char*)&l;}
'''.replace('X86',dispatch('386')).replace('ARM32',dispatch('arm'))
checks=0
rng=random.Random(0xc0ffee)
with tempfile.TemporaryDirectory(prefix='jit-memmove-') as td:
    td=Path(td);(td/'move.c').write_text(unit)
    for bits in [32,64]:
        binary=td/f'move{bits}.so'
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+['-shared','-fPIC',f'-DVM_WORD_BITS={bits}','-I'+str(root/'include'),str(td/'move.c'),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.run.argtypes=[C.c_void_p]+[C.c_int]*5
        for arch in range(2):
            for head in range(2):assert lib.route(arch,head)==67,(arch,head)
        for head in range(2):
            for n in [0,1,2,3,4,7,8,9,15,16,31,32,63,64,127,128,255,256,1023]:
                s=lib.dataoffset() if head else 128
                for delta in sorted(set([-127,-32,-8,-4,-1,0,1,2,3,4,7,8,15,31,127,n-1,n,n+1])):
                    d=s+delta
                    if d<0:continue
                    for aliascount in range(2):
                        if aliascount and d%(bits//8):continue
                        before=bytearray(rng.randbytes(4096))
                        if aliascount:before[d:d+bits//8]=n.to_bytes(bits//8,'little')
                        expected=before[:];expected[d:d+n]=before[s:s+n]
                        buf=(C.c_ubyte*len(before)).from_buffer_copy(before)
                        lib.run(buf,n,s,d,head,aliascount)
                        assert bytes(buf)==expected,(bits,head,n,delta,aliascount)
                        checks+=1
print(f'PASS JIT block moves: {checks} production VM snapshot/overlap/canary cases, WORD32/64 and x86/ARM helper routing')
