#!/usr/bin/env python3
"""Check production TCMP backward-field spans and execute actual AMD64 leaves."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('TCMP_XEC_SOURCE', ROOT / 'libinterp/xec.c')).read_text()
body = source[source.index('OP(tcmp)'):source.index('OP(eclr)')]
unit = r'''
#include <lib9.h>
#include "isa.h"
#include "interp.h"
#include "amd64-emit.h"
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <unistd.h>
REG R;
static jmp_buf boundary; /* Private error boundary; no VM owners are allocated. */
static int raised; /* Captured TCMP error flag for the current invocation. */
char exTcheck[]="type check";
#define OP(name) static void name(void) /* Bind the extracted opcode locally. */


/* Capture a runtime failure without unwinding outside the owned fixture frame. */
void error(char *message)
{
    if(message != exTcheck){
        abort();
    }
    raised=1;
    longjmp(boundary,1);
}
IMPLEMENTATION


/* Test real C and RX paths. Owners are borrowed or deliberately invalid integer
 * identities; low addresses must never be dereferenced. All mapping is released. */
int main(void)
{
    LeafLayout abi={0};
    Heap left={0};
    Heap right={0};
    Type type={0};
    uchar slots[32];
    u32int code[LeafMax];
    void *s;
    void *d;
    void *memory;
    uintptr low;
    uintptr distance=sizeof(Heap)-offsetof(Heap,t);
    int n;
    int side;
    int expected;
    int mode;
    int actual;
    int checks=0;
    int (*native)(REG*);
    abi.wordbytes=sizeof(WORD);
    abi.pc=offsetof(REG,PC);
    abi.s=offsetof(REG,s);
    abi.d=offsetof(REG,d);
    abi.m=offsetof(REG,m);
    abi.heaptype=distance;
    n=leafemit(code,LeafMax,&abi,ITCMP);
    if(n<=0){return 2;}
    memory=mmap(nil,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(memory==MAP_FAILED){return 3;}
    memmove(memory,code,n*sizeof(u32int));
    if(mprotect(memory,4096,PROT_READ|PROT_EXEC)!=0){return 4;}
    native=(int(*)(REG*))memory;
    left.t=&type;
    right.t=&type;
    R.s=slots+4;
    R.d=slots+20;
    for(mode=0;mode<2;mode++){
        for(low=0;low<=distance+2;low++){
            for(side=0;side<2;side++){
                s=side ? (void*)(&left+1) : (void*)low;
                d=side ? (void*)low : (void*)(&right+1);
                if(low==distance+1){s=H;d=nil;}
                if(low==distance+2){s=&left+1;d=&right+1;}
                expected=low>distance;
                memmove(R.s,&s,sizeof(s));
                memmove(R.d,&d,sizeof(d));
                raised=0;
                if(mode){
                    actual=native(&R);
                }else{
                    if(setjmp(boundary)==0){tcmp();}
                    actual=!raised;
                }
                if(actual!=expected){
                    fprintf(stderr,"TCMP prefix failure mode=%d low=%lu side=%d\n",mode,(ulong)low,side);
                    return 5;
                }
                checks++;
            }
        }
    }
    if(munmap(memory,4096)!=0){return 6;}
    printf("PASS C/AMD64 TCMP prefix: %d checks, WORD=%d\n",checks,(int)sizeof(WORD)*8);
    return 0;
}
'''.replace('IMPLEMENTATION', body)
with tempfile.TemporaryDirectory(prefix='typecheck-prefix-') as temporary:
    directory = Path(temporary)
    target = directory / 'test.c'
    target.write_text(unit)
    for wordbytes in (4, 8):
        model = directory / str(wordbytes)
        model.mkdir()
        header = (ROOT / 'include/interp.h').read_text()
        if wordbytes == 4:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model / 'interp.h').write_text(header)
        binary = model / 'test'
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        includes = [model] + [ROOT / p for p in ('Linux/arm/include', 'include', 'libinterp')]
        subprocess.run(command + ['-D_GNU_SOURCE'] + ['-I' + str(p) for p in includes] +
                       [str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
