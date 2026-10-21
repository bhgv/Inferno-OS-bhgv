#!/usr/bin/env python3
"""Run production parsemod on bounded fixtures with minimal allocation stubs.

This covers descriptor-index checks and scalar decoding, not general truncated
input handling, cryptography, heap initialization or module execution.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=(root/'libinterp/load.c').read_text()
def extract(name,result):
    start=src.index(result+'\n'+name+'(')
    pos=src.index('{',start)+1; depth=1
    while depth:
        depth+=(src[pos]=='{')-(src[pos]=='}');pos+=1
    return src[start:pos]
unit=r'''
#define malloc testmalloc
#define mallocz testmallocz
#define free testfree
#define strdup teststrdup
#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
extern void hostfree(void*) __asm__("free");
static int live;
static char message[128];
Module *modules;
int cflag,dontcompile;
char exNomem[]="out of memory";
void *testmalloc(size_t n) { void *p=calloc(1,n?n:1);if(p)live++;return p; }
void *testmallocz(ulong n,int z) { (void)z;return testmalloc(n); }
void testfree(void *p) { if(p){live--;hostfree(p);} }
char *teststrdup(const char *s) { size_t n=strlen(s)+1;char *p=testmalloc(n);memcpy(p,s,n);return p; }
void kwerrstr(char *fmt,...) { snprintf(message,sizeof(message),"%s",fmt); }
void freetype(Type *t) { testfree(t); }
void destroy(void *p) { assert(p==H); }
void freeheap(Heap *p,int n) { (void)p;(void)n;assert(0); }
static void *unexpected(void) { assert(0);return nil; }
Type *dtype(void (*f)(Heap*,int),int n,uchar *map,int np) {
    Type *t=testmalloc(sizeof(Type));(void)f;(void)map;t->size=n;t->np=np;return t;
}
Heap *heapz(Type *t) { (void)t;return unexpected(); }
Heap *nheap(int n) { (void)n;return unexpected(); }
String *c2string(char *s,int n) { (void)s;(void)n;return unexpected(); }
void initarray(Type *t,Array *a) { (void)t;(void)a;assert(0); }
int mustbesigned(char *p,uchar *b,ulong n,Dir *d) { (void)p;(void)b;(void)n;(void)d;return 0; }
int verifysigner(uchar *a,int b,uchar *c,ulong d) { (void)a;(void)b;(void)c;(void)d;return 0; }
void mlink(Module *m,Link *l,uchar *s,int sig,int pc,Type *t) {
    (void)m;(void)l;(void)s;(void)sig;(void)pc;(void)t;assert(0);
}
int compile(Module *m,int n,Modlink *ml) { (void)m;(void)n;(void)ml;assert(0);return 0; }
Type Tarray;
#define A(r) *((Array**)(r))
'''+'\n'.join(extract(n,r) for n,r in [('operand','static int'),('disw','static ulong'),
 ('canontod','double'),('brpatch','int'),('freemod','void'),('parsemod','Module*')])+r'''
static uchar buffer[512];
static int at;
static void byte(int v) { buffer[at++]=v; }
static void integer(int v) {
    unsigned int u=(unsigned int)v;
    if(v>=-64 && v<=63) byte(v&127);
    else if(v>=-8192 && v<=8191) { byte(0x80|((u>>8)&63));byte(u&255); }
    else { byte(0xc0|((u>>24)&63));byte((u>>16)&255);byte((u>>8)&255);byte(u&255); }
}
static Module *fixture(int types,int first,int duplicate,int data,int array) {
    Dir dir={0};
    int i;
    memset(buffer,0,sizeof(buffer));at=0;
    integer(XMAGIC);integer(0);integer(1024);integer(1);integer(data);
    integer(types);integer(0);integer(0);integer(0);
    byte(IRUNT);byte(SRC(AXXX)|DST(AXXX));
    for(i=0;i<types;i++) {
        integer(i==0?first:duplicate?first:i);
        integer(0);integer(0);
    }
    if(array) {
        byte(DBYTE(DEFA,1));integer(0);
        byte(0);byte(0);byte(0);byte(types);
    }
    byte(0);byte('m');byte(0);
    message[0]=0;
    return parsemod("fixture",buffer,sizeof(buffer),&dir);
}
int main(void) {
    static const int values[]={-536870912,-8193,-8192,-65,-64,-1,0,1,63,64,8191,8192,536870911};
    uchar *p;
    Module *m;
    unsigned int i;
    for(i=0;i<sizeof(values)/sizeof(values[0]);i++) {
        at=0;integer(values[i]);p=buffer;
        assert(operand(&p)==values[i] && p==buffer+at);
    }
    buffer[0]=0xff;buffer[1]=0xff;buffer[2]=0xff;buffer[3]=0xff;p=buffer;
    assert(disw(&p)==0xffffffffUL && p==buffer+4);
    m=fixture(1,0,0,0,0);assert(m && m->jit==nil);testfree(m->ext);m->ext=nil;freemod(m);modules=nil;assert(live==0);
    assert(fixture(1,1,0,0,0)==nil && strcmp(message,"heap id range")==0 && live==0);
    assert(fixture(1,-1,0,0,0)==nil && strcmp(message,"heap id range")==0 && live==0);
    assert(fixture(2,0,1,0,0)==nil && strcmp(message,"heap id range")==0 && live==0);
    assert(fixture(0,0,0,4,0)==nil && strcmp(message,"missing desc for mp")==0 && live==0);
    assert(fixture(1,0,0,0,1)==nil && strcmp(message,"bad array type")==0 && live==0);
    puts("PASS Dis loader: valid module, negative/equal/duplicate type IDs, missing data descriptor, array type boundary and signed operand decoding");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='dis-loader-') as td:
    td=Path(td);source=td/'loader.c';source.write_text(unit)
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+
                   shlex.split(os.environ.get('CFLAGS','-O1 -g'))+
                   ['-I'+str(root/'Linux/386/include'),'-I'+str(root/'include'),
                    '-I'+str(root/'libinterp'),str(source),'-o',str(td/'loader')],check=True)
    subprocess.run([str(td/'loader')],check=True)
