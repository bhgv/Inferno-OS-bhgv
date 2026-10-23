#!/usr/bin/env python3
"""Exercise production C frame helpers against invalid metadata and stack spans."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
unit = r'''
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static sigjmp_buf escape;
REG R;
char exModule[] = "module", exHeap[] = "heap";
static char *raised;
static int extended, initialized, fail;
static Frame replacement;
#define T(r) *((void**)(R.r))
/* Track errors without allowing the helper to publish a partial frame. */
void error(char *message) { raised=message; siglongjmp(escape, 1); }
void extend(void) {
    extended++;
    if(fail)error(exHeap);
    R.s=&replacement;
}
void initmem(Type *t, void *f) { assert(t==R.s && f!=nil); initialized++; }
int __lsan_is_turned_off(void) { return 1; }
IMPLEMENTATION
/* Cover both stack boundaries, every invalid descriptor, and failed growth. */
int main(void)
{
    union { double align; uchar bytes[512]; } arena;
    Type type;
    void *output, *before;
    uchar *sp, *ts;
    int mode, size, failure, checks=0, invalid, grow;
    long sizes[]={-1, 0, sizeof(Frame)-1, sizeof(Frame), sizeof(Frame)+1, INT_MAX};
    for(mode=0;mode<10;mode++)for(size=0;size<6;size++)for(failure=0;failure<2;failure++) {
        memset(&R,0,sizeof(R)); memset(&type,0,sizeof(type)); memset(&arena,0xa5,sizeof(arena));
        output=before=&arena.bytes[400]; type.size=sizes[size];
        R.s=&type; R.d=&output; R.SP=arena.bytes; R.TS=arena.bytes+sizeof(Frame)+1;
        switch(mode) {
        case 1: R.s=nil; break;
        case 2: R.s=H; break;
        case 3: R.d=nil; break;
        case 4: R.d=H; break;
        case 5: R.SP=nil; break;
        case 6: R.TS=nil; break;
        case 7: R.SP=arena.bytes+128; R.TS=arena.bytes+64; break;
        case 8: R.TS=arena.bytes+sizeof(Frame); break;
        case 9: R.TS=arena.bytes; break;
        }
        invalid=(mode>=1 && mode<=4) || sizes[size]<(long)sizeof(Frame);
        grow=!invalid && (mode>=5 || sizes[size]>=(long)sizeof(Frame)+1);
        sp=R.SP; ts=R.TS; extended=initialized=0; raised=nil; fail=failure;
        if(sigsetjmp(escape,1)==0)rmfram();
        if(invalid || (grow && failure)) {
            assert(raised==((mode>=1 && mode<=4)?exModule:exHeap));
            assert(output==before && R.SP==sp && R.TS==ts && initialized==0);
            assert(extended==(grow && failure));
        } else if(grow) {
            assert(!raised && extended==1 && initialized==0 && output==&replacement);
            assert(R.SP==sp && R.TS==ts);
        } else {
            Frame *f=(Frame*)arena.bytes;
            assert(!raised && !extended && initialized==1 && output==f);
            assert(R.SP==sp+type.size && R.TS==ts && f->t==&type && f->mr==nil);
        }
        checks++;
    }
    printf("frame helper: %d cases\n",checks);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='jit-frame-stack-') as directory:
    td=Path(directory)
    cmd=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get(
        'CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes=['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    for arch in ('386','arm','mips','power','sparc'):
        if os.environ.get('JIT_STACK_ARCH') not in (None,arch): continue
        folder=Path(os.environ.get('JIT_STACK_SOURCE_DIR',root/'libinterp'))
        source=(folder/('comp-'+arch+'.c')).read_text()
        start=source.index('static void\nrmfram(void)')
        end=source.index('\n}',start)+2
        target=td/(arch+'.c'); target.write_text(unit.replace('IMPLEMENTATION',source[start:end]))
        binary=td/arch
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        print(arch,flush=True)
        subprocess.run([str(binary)],check=True)
