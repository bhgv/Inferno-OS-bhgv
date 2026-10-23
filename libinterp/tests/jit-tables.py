#!/usr/bin/env python3
"""Exercise the shared production preflight with exact-sized module-data heaps."""
import os
from pathlib import Path
import shlex
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('JIT_TABLE_SOURCE', root/'libinterp/jit-tables.h')).read_text()
unit = r'''
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"
#define nil NULL
#define H ((void*)(uintptr_t)0xffffffff)
#define ulong uint32_t
typedef int32_t WORD;
typedef struct Type {int size;} Type;
typedef struct Inst {int op,add;struct {WORD ind;} d;} Inst;
typedef struct Module {Inst *prog;int ntype;Type **type;unsigned char *origmp;} Module;
static int fail, live, checks;
/* Track the validator's private index independently of fixture data storage. */
static void *indexalloc(size_t n) {void *p;if(fail)return nil;p=malloc(n);if(p)live++;return p;}
static void indexfree(void *p) {if(p){live--;free(p);}}
static int (*plancompare)(void*,void*);
static int hostcompare(const void *a,const void *b) {return plancompare((void*)a,(void*)b);}
static void plansort(void *p,long n,long width,int (*compare)(void*,void*)) {
    plancompare=compare;qsort(p,n,width,hostcompare);
}
#define malloc indexalloc
#define free indexfree
#define qsort plansort
IMPLEMENTATION
#undef malloc
#undef free
/* Use memcpy so the test setup never forms an unaligned WORD pointer. */
static void put(unsigned char *p,int size,int64_t at,int32_t value)
{
    if(at>=0 && at<=size-4)
        memcpy(p+at,&value,4);
}
/* Check boundary decisions with no spare bytes beyond the advertised extent. */
static void one(int kind,int count,int offset,int extent,int badtarget)
{
    Type type={extent}, *types[]={&type};
    Inst instruction={kind,DST(AMP),{offset}};
    Module module={&instruction,1,types,nil};
    unsigned char *data, *saved;
    int64_t start;
    int header,stride,target,need,row,valid,actual;

    data=calloc(1,extent ? extent : 1);saved=malloc(extent ? extent : 1);
    assert(data && saved);module.origmp=data;
    header=kind==ICASEL?2:1;stride=kind==ICASEL?6:(kind==ICASE || kind==ICASEC)?3:1;
    target=kind==ICASEL?4:(kind==ICASE || kind==ICASEC)?2:0;
    start=offset-(kind==IGOTO?4:0);
    put(data,extent,start,count);
    if(count>=0 && count<=8){
        for(row=0;row<count;row++)
            put(data,extent,start+4*(header+row*stride+target),badtarget==1?-2:0);
        if(kind!=IGOTO)
            put(data,extent,start+4*(header+count*stride),badtarget==1?-2:badtarget==2?-1:0);
    }
    memcpy(saved,data,extent);
    /* Arithmetic here is bounded test data, independent of production divisions. */
    need=count>=0 && count<=8 ? (header+count*stride+(kind!=IGOTO))*4 : INT_MAX;
    valid=count>=0 && count<=8 && start>=0 && offset%4==0 &&
        (kind!=ICASEL || offset%8==0) && start<=extent && need<=extent-start &&
        (badtarget!=1 || (kind==IGOTO && count==0));
    fail=0;actual=jitvalidtables(&module,1);
    assert(actual==valid);
    assert(!memcmp(saved,data,extent) && live==0);
    fail=1;assert(!jitvalidtables(&module,1) && live==0);fail=0;
    free(saved);free(data);checks+=2;
}
/* Reject overlaps while accepting exact same-kind aliases in either order. */
static void overlaps(void)
{
    unsigned char data[128]={0},saved[128];
    Type type={sizeof(data)}, *types[]={&type};
    Inst instructions[3]={{ICASE,DST(AMP),{0}},{ICASE,DST(AMP),{0}},{ICASE,DST(AMP),{32}}};
    Module module={instructions,1,types,data};
    int mode;

    for(mode=0;mode<8;mode++){
        memset(data,0,sizeof(data));
        instructions[0].op=ICASE;instructions[0].d.ind=0;
        instructions[1].op=ICASE;instructions[1].d.ind=0;
        instructions[2].op=ICASE;instructions[2].d.ind=32;
        if(mode==1)instructions[1].d.ind=4;
        if(mode==2){instructions[1].op=IGOTO;instructions[1].d.ind=4;}
        if(mode==3){instructions[1].op=ICASEL;instructions[1].d.ind=0;}
        if(mode==4)instructions[2].d.ind=0;
        if(mode==5){instructions[0].d.ind=32;instructions[2].d.ind=0;}
        if(mode==6)instructions[1].add=DST(AFP);
        if(mode==7)instructions[1].add=DST(AIND|AMP);
        memcpy(saved,data,sizeof(data));
        assert(jitvalidtables(&module,3)==(mode==0 || mode==4 || mode==5));
        assert(live==0 && !memcmp(saved,data,sizeof(data)));
        instructions[1].add=DST(AMP);checks++;
    }
    instructions[0].op=IRUNT;module.origmp=H;module.ntype=0;module.type=nil;
    assert(jitvalidtables(&module,1));checks++;
}
/* Exercise complete native-sort plans with many disjoint/aliased table spans. */
static void largeplans(void)
{
    int sizes[]={8,11,127,512,4096};
    uint32_t random=0x12345678;
    int size,mode,n,i,j,k,expected;
    Type type,*types[]={&type};
    Module module;
    Inst *instructions,*saved;
    unsigned char *data;

    for(size=0;size<5;size++){
        n=sizes[size];
        data=calloc(n,16);
        instructions=calloc(n,sizeof(*instructions));
        saved=calloc(n,sizeof(*saved));
        assert(data && instructions && saved);
        type.size=n*16;
        module.prog=instructions;module.ntype=1;module.type=types;module.origmp=data;
        for(mode=0;mode<8;mode++){
            for(i=0;i<n;i++){
                instructions[i].op=ICASE;
                instructions[i].add=DST(AMP);
                instructions[i].d.ind=16*(mode==1?n-1-i:i);
            }
            if(mode==2){
                for(i=n-1;i>0;i--){
                    random=random*1664525U+1013904223U;
                    j=random%(i+1);
                    k=instructions[i].d.ind;
                    instructions[i].d.ind=instructions[j].d.ind;
                    instructions[j].d.ind=k;
                }
            }
            if(mode==3)
                for(i=0;i<n;i++)instructions[i].d.ind=0;
            if(mode==4)instructions[1].d.ind=4;
            if(mode==5){instructions[1].d.ind=0;instructions[1].op=ICASEC;}
            if(mode==6)
                for(i=1;i<n;i+=2)instructions[i].op=IRUNT;
            if(mode==7)instructions[n-1].d.ind++;
            memcpy(saved,instructions,n*sizeof(*saved));
            expected=mode!=4 && mode!=5 && mode!=7;
            assert(jitvalidtables(&module,n)==expected);
            assert(live==0 && !memcmp(saved,instructions,n*sizeof(*saved)));
            for(i=0;i<n*16;i++)assert(data[i]==0);
            fail=1;assert(!jitvalidtables(&module,n) && live==0);fail=0;
            assert(!memcmp(saved,instructions,n*sizeof(*saved)));
            for(i=0;i<n*16;i++)assert(data[i]==0);
            checks+=2;
        }
        free(saved);free(instructions);free(data);
    }
}
/* Cover each count, target and extent transition, including 32-bit extrema. */
int main(void)
{
    int kinds[]={ICASE,ICASEC,ICASEL,IGOTO},counts[]={-1,INT_MIN,0,1,2,8,INT_MAX};
    int offsets[]={-1,0,1,4,8,16,252,256,INT_MAX};
    int kind,count,offset,extent,bad;

    for(kind=0;kind<4;kind++)for(count=0;count<7;count++)
    for(offset=0;offset<9;offset++)for(extent=0;extent<=256;extent+=4)
    for(bad=0;bad<3;bad++)
        one(kinds[kind],counts[count],offsets[offset],extent,bad);
    overlaps();
    largeplans();
    printf("JIT table preflight: %d cases\n",checks);
    return 0;
}
int __lsan_is_turned_off(void) {return 1;}
'''
# Optionally exercise the actual native sorter at the JIT preflight boundary.
native_library = os.environ.get('JIT_TABLE_NATIVE_SORT')
native_sort = None
if native_library:
    if native_library not in ('lib9', 'libkern'):
        raise ValueError('JIT_TABLE_NATIVE_SORT must be lib9 or libkern')
    native_sort = (root / native_library / 'qsort.c').read_text()
    native_sort = native_sort.replace('#include "lib9.h"', '')
    native_sort = re.sub(r'\bqsort\b', 'native_qsort', native_sort)
    native_sort = ('#include <stdint.h>\ntypedef unsigned long ulong;\n'
                   'typedef uintptr_t uintptr;\n' + native_sort)
    unit = unit.replace('plancompare=compare;qsort(p,n,width,hostcompare);',
                        'native_qsort(p,n,width,compare);')
    unit = unit.replace('static void plansort(',
                        'extern void native_qsort(void*,long,long,int(*)(void*,void*));\n'
                        'static void plansort(')

with tempfile.TemporaryDirectory(prefix='jit-tables-') as directory:
    td = Path(directory)
    target = td/'test.c'
    target.write_text(unit.replace('IMPLEMENTATION', source))
    binary = td/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    extra = []
    if native_sort is not None:
        sortfile = td/'native-sort.c'
        sortfile.write_text(native_sort)
        extra.append(str(sortfile))
    subprocess.run(cmd+['-I'+str(root/'include'), str(target)] + extra +
                   ['-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
