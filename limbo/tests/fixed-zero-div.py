#!/usr/bin/env python3
"""Connect real fixed selection/lowering to actual WORD32 VM zero/division traps."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


def function(source,signature):
    """Extract actual C functions while ignoring quoted/comment braces."""
    start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise ValueError(signature)


types=(ROOT/'limbo/types.c').read_text()
selector=types[types.index('#define I(x)'):types.index('\nint\nispoly(',types.index('#define I(x)'))]
generator=Path(os.environ.get('FIXED_GENERATOR_SOURCE',ROOT/'limbo/gen.c')).read_text()
vm=(ROOT/'libinterp/xec.c').read_text()
unit=r'''
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <setjmp.h>
#include "isa.h"
#define nil NULL // Native null identity.
#define H ((void*)-1) // VM nil-owner sentinel.
typedef uintptr_t uintptr;
typedef int64_t Long;typedef int64_t vlong;typedef uint64_t uvlong;
typedef int32_t WORD;typedef uint32_t UWORD;
typedef struct Src {int start;} Src;
typedef struct Type {double factor;Src src;} Type;
typedef struct Node {Type *ty;WORD value;WORD *slot;int constant;} Node;
typedef struct Inst {int op;Node *s,*m,*d;} Inst;
static Type inttype={1,{0}},*tint=&inttype;
static int compiler_errors,stores,nextconstant;
static Node constants[8];static Inst issued;
static jmp_buf boundary;static char *raised;
static char exZdiv[]="zero", exBounds[]="bounds";
static struct {void *s,*m,*d;} R;
enum {STemp=20,DTemp=28};
/* Observe real selector diagnostics without terminating fixture setup. */
static void error(int where,const char *text) {(void)where;assert(text);compiler_errors++;}
static void fatal(const char *text) {(void)text;abort();}
static double scale(Type *type) {return type->factor;}
SELECTOR
/* Record lowering arguments while retaining source identity and destination aliasing. */
static Node *mkconst(Src *src,Long value) {
    Node *n;(void)src;assert(nextconstant<8);n=&constants[nextconstant++];
    memset(n,0,sizeof(*n));n->ty=tint;n->value=value;n->constant=1;return n;
}
static Node *sumark(Node *node) {return node;}
static void genstore(Src *src,Node *node,int offset) {(void)src;(void)node;(void)offset;stores++;}
static Inst *genrawop(Src *src,int op,Node *s,Node *m,Node *d) {
    (void)src;issued.op=op;issued.s=s;issued.m=m;issued.d=d;return &issued;
}
GENERATOR
/* Execute production VM handlers with a separate error boundary and exact byte images. */
static void vmerror(char *text) {raised=text;longjmp(boundary,1);}
#define error vmerror
#define OP(n) static void n(void)
VM
#undef error
int __lsan_is_turned_off(void) {return 1;}
int main(void) {
    WORD values[]={0,1,-1,INT32_MIN,INT32_MAX},zero=0,sourcevalue,middlevalue;
    double ratios[]={1.125,1.5,1.875};int powers[]={-64,-100,-500};
    union {uint64_t alignment;unsigned char bytes[128];} memory;
    unsigned char before[128],expected[128];
    Type source,numerator,result;Node s,m,d;Src src={0};
    int x,y,r,e,mode,alignment,alias,offset,ok;unsigned long checks=0;
    for(x=0;x<5;x++)for(y=0;y<5;y++)for(r=0;r<3;r++)for(e=0;e<3;e++)
    for(mode=0;mode<4;mode++)for(alignment=0;alignment<8;alignment++)for(alias=0;alias<4;alias++){
        double tiny=ldexp(ratios[r],powers[e]);
        numerator.factor=mode==1 ? 1 : tiny;source.factor=mode==1 ? 1/tiny : mode==3 ? tiny : 1;result.factor=1;
        memset(memory.bytes,0xa5,sizeof(memory.bytes));
        offset=alias==0 ? 88 : alias==1 ? 16+alignment : alias==2 ? 48+(alignment*3)%8 : 17+alignment;
        s=(Node){&source,0,(WORD*)(memory.bytes+16+alignment),0};
        m=(Node){&numerator,0,(WORD*)(memory.bytes+48+(alignment*3)%8),0};
        d=(Node){&result,0,(WORD*)(memory.bytes+offset),0};
        memcpy(s.slot,&values[y],sizeof(WORD));memcpy(m.slot,&values[x],sizeof(WORD));
        memcpy(before,memory.bytes,sizeof(before));memcpy(expected,before,sizeof(expected));
        compiler_errors=stores=nextconstant=0;
        if(mode==3)genfixcastop(&src,ICVTXX,&s,&d);
        else genfixop(&src,mode==2 ? IMULX : IDIVX,&s,mode==1 ? NULL : &m,&d);
        assert(!compiler_errors && stores==0);
        sourcevalue=issued.s->value;middlevalue=issued.m ? issued.m->value : 0;
        R.s=issued.s->constant ? &sourcevalue : issued.s->slot;
        R.m=issued.m ? issued.m->constant ? &middlevalue : issued.m->slot : (void*)1;R.d=issued.d->slot;
        raised=NULL;ok=0;
        if(setjmp(boundary)==0){
            if(issued.op==IMOVW)movw();else {assert(issued.op==IDIVW);divw();}ok=1;
        }
        if(mode<2 && values[y]==0){assert(!ok && raised==exZdiv);}
        else {assert(ok && raised==NULL);memcpy(expected+offset,&zero,sizeof(zero));}
        assert(!memcmp(memory.bytes,expected,sizeof(expected)));
        assert(issued.op==(mode<2 ? IDIVW : IMOVW));
        if(mode<2)assert(issued.s==&s && issued.m->constant && issued.m->value==0);
        checks++;
    }
    printf("PASS fixed zero division: %lu selection/lowering/VM/alias cases\n",checks);return 0;
}
'''
unit=unit.replace('SELECTOR',selector).replace('GENERATOR','\n'.join(function(generator,name) for name in ('static Inst*\ngenfixop(','Inst*\ngenfixcastop('))).replace('VM\n', '\n'.join(function(vm,'OP('+name+')') for name in ('divw','movw'))+'\n')
with tempfile.TemporaryDirectory(prefix='fixed-zero-div-') as directory:
    td=Path(directory);target=td/'regression.c';target.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    binary=td/'regression'
    subprocess.run(command+['-I'+str(ROOT/'include'),str(target),'-lm','-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
