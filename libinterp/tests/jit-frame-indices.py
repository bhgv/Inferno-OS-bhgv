#!/usr/bin/env python3
"""Execute the production IFRAME selection blocks of all five native-PC backends."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
unit = r'''
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static Module module, *mod = &module, *m = &module;
typedef MARKTYPE Mark;
static Mark marks[3], *tinit = marks;
static uchar storage[16], *base = storage;
static ulong macro[1];
void (*optab[256])(void);
static int punts, emitted;
static Type *expected;
enum { SRCOP=1, DSTOP=2, MacFRAM=0, RA3=3, RA2=2, RTA=4, RCX=1,
       Ri=5, Ro1=6, Stw=7, Ostw=8, Osw=9, Ocall=10, Ojal=11 };
/* Poison ordinary allocations; a requested cleared allocation must erase the marks. */
static void *poisonmalloc(ulong n) { assert(n==sizeof(marks)); memset(marks,0xa5,n); return marks; }
void *mallocz(ulong n, int clear) { void *p=poisonmalloc(n); if(clear)memset(p,0,n); return p; }
#define malloc poisonmalloc
/* Disable unsupported LeakSanitizer process inspection only. */
int __lsan_is_turned_off(void) { return 1; }
/* Boundary stubs check fallback selection and constant descriptor identity. */
static void punt(Inst *i, int flags, void (*target)(void)) {
    assert(i->op == IFRAME && flags == (SRCOP|DSTOP) && target == optab[IFRAME]); punts++;
}
static void con(ulong value, int reg, ...) { (void)reg; assert(value == (ulong)expected); emitted++; }
static void ldc(ulong value, int reg) { con(value, reg); }
static void rbra(ulong address, int op) { (void)address; (void)op; emitted++; }
static void jmpl(uchar *address) { assert(address == base); emitted++; }
static void opwst(Inst *i, int op, int reg) { (void)i; (void)op; (void)reg; emitted++; }
static void op3(Inst *i, int op, int reg, int zero) { assert(zero == 0); opwst(i, op, reg); }
static void xchg(void) { emitted++; }
#define CALL(address) jmpl(address)
#define J(op,address) jmpl(address)
#define NOOP ((void)(emitted++))
/* Keep the actual backend switch arm intact, substituting emission boundaries. */
static void selectframe(Inst *i) { switch(i->op) { BLOCK } }
/* Check native selection, fallback, and absence of writes outside tinit. */
int main(void) {
    Inst i = {0}; Type types[3] = {{0}}, *table[3];
    long indices[] = {-1, INT_MIN, 0, 1, 2, 3, INT_MAX};
    int n, mode, size, checks=0, valid, j;
    long sizes[] = {-1, 0, sizeof(Frame)-1, sizeof(Frame), sizeof(Frame)+16};
    i.op=IFRAME; module.ntype=3; module.type=table;
    INIT
    for(j=0;j<3;j++)assert(tinit[j]==0); checks++;
    if(INITONLY)return 0;
    for(n=0;n<3;n++)table[n]=&types[n];
    for(mode=0;mode<3;mode++)for(size=0;size<5;size++)
    for(n=0;n<(int)(sizeof(indices)/sizeof(indices[0]));n++) {
        for(j=0;j<3;j++)types[j].size=sizes[size];
        i.add=SRC(mode==0?AIMM:mode==1?AFP:AMP)|DST(AFP);
        i.s.imm=indices[n]; punts=emitted=0; memset(marks,0,sizeof(marks));
        valid=mode==0 && indices[n]>=0 && indices[n]<3 && sizes[size]>=(long)sizeof(Frame);
        expected=valid?table[indices[n]]:nil;
        selectframe(&i);
        assert(punts==!valid && (emitted>0)==valid);
        for(j=0;j<3;j++)assert(marks[j]==(valid && j==indices[n])); checks++;
    }
    i.add=SRC(AIMM)|DST(AFP); i.s.imm=1;
    for(n=0;n<2;n++) {
        module.type=n?table:nil; table[1]=nil;
        punts=emitted=0; memset(marks,0,sizeof(marks)); selectframe(&i);
        assert(punts==1 && emitted==0 && marks[0]==0 && marks[1]==0 && marks[2]==0); checks++;
    }
    printf("IFRAME: %d cases\n",checks); return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='jit-frame-indices-') as directory:
    td = Path(directory)
    cmd = shlex.split(os.environ.get('CC', 'cc')) + shlex.split(os.environ.get(
        'CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    for arch in ('arm','386','mips','sparc','power'):
        if os.environ.get('JIT_FRAME_ARCH') not in (None, arch):
            continue
        folder = Path(os.environ.get('JIT_FRAME_SOURCE_DIR', root/'libinterp'))
        source = (folder/('comp-'+arch+'.c')).read_text()
        start = source.index('\tcase IFRAME:')
        end = source.index('\n\tcase ', start+5)
        initialization = re.search(r'tinit\s*=\s*mallocz?\(m->ntype\*sizeof\(\*tinit\)[^;]*;', source).group()
        if os.environ.get('JIT_FRAME_CASES') == 'indices':
            initialization = 'memset(marks,0,sizeof(marks));'
        marktype = 'ulong' if arch in ('mips','power') else 'uchar'
        target = td/(arch+'.c')
        target.write_text(unit.replace('BLOCK', source[start:end]).replace('MARKTYPE', marktype).replace('INITONLY', '1' if os.environ.get('JIT_FRAME_CASES') == 'initialization' else '0').replace('INIT', initialization))
        binary = td/arch
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        print(arch,flush=True)
        subprocess.run([str(binary)],check=True)
