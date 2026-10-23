#!/usr/bin/env python3
"""Production dynamic loader/Type ownership with real Proc error frames.

Heap allocation and compiler are injected boundaries; no native code executes.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
def function(path,name,result):
    source=(root/path).read_text()
    start=source.index(result+'\n'+name+'(');brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(name)
unit=r'''
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"
#include <assert.h>
#include <stdio.h>
static Proc worker;
Proc *getup(void) {return &worker;}
void nexterror(void) {assert(worker.nerr>0);siglongjmp(worker.estack[--worker.nerr],1);}
void error(char *s) {(void)s;nexterror();}
int __lsan_is_turned_off(void) {return 1;}
static char message[128];
void kwerrstr(char *s,...) {snprintf(message,sizeof(message),"%s",s);}
char exNomem[]="out of memory",exHeap[]="heap";
int cflag,mutator,nprop;
Type Tmodlink={1};
static void *owned[128];
static size_t sizes[128];
static int live,fail_after=-1,fail_heap,compile_mode,calls,throw_string;
static void *alloc(size_t n) {
    void *p;int i;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    p=calloc(1,n?n:1);assert(p);
    for(i=0;i<128;i++)if(!owned[i]){owned[i]=p;sizes[i]=n;live++;return p;}
    assert(0);return nil;
}
static void dealloc(void *p) {
    int i;if(!p)return;
    for(i=0;i<128;i++)if(owned[i]==p){owned[i]=nil;live--;free(p);return;}
    assert(0);
}
static void *resize(void *p,size_t n) {
    void *q;int i;
    q=alloc(n);if(!q)return nil;
    for(i=0;i<128;i++)if(owned[i]==p){memcpy(q,p,sizes[i]<n?sizes[i]:n);dealloc(p);return q;}
    assert(0);return nil;
}
static ulong allocated(void *p) {
    int i;for(i=0;i<128;i++)if(owned[i]==p)return sizes[i];
    assert(0);return 0;
}
static void *allocz(size_t n,int z) {(void)z;return alloc(n);}
static char *duplicate(const char *s) {
    size_t n=strlen(s)+1;char *p=alloc(n);if(p)memcpy(p,s,n);return p;
}
#define strdup duplicate
#define malloc alloc
#define mallocz allocz
#define realloc resize
#define free(p) dealloc(p)
#define msize allocated
void freeheap(Heap *h,int n) {(void)h;(void)n;}
void markheap(Type *t,void *p) {(void)t;(void)p;}
Heap *heapz(Type *t) {
    Heap *h;
    if(fail_heap)error("heap allocation failed");
    h=alloc(sizeof(Heap)+t->size);assert(h);
    h->ref=1;h->t=t;t->ref++;
    return h;
}
char *string2c(String *s) {(void)s;if(throw_string)error("string conversion");return "newmod";}
Heap *nheap(int n) {
    Heap *h;if(fail_heap)error("link allocation");
    h=alloc(sizeof(Heap)+n);if(!h)error(exNomem);
    h->ref=1;h->t=nil;return h;
}
int compile(Module *m,int n,Modlink *ml) {
    (void)n;calls++;assert(m->origmp==ml->MP);
    if(compile_mode==2)error("compiler error");
    if(!compile_mode)return 0;
    m->compiled=1;return 1;
}
IMPLEMENTATION
void destroy(void *p) {
    Heap *h;Modlink *ml;if(p==H)return;h=D2H(p);assert(h->ref>0);
    if(--h->ref)return;
    if(h->t==&Tmodlink) {
        ml=p;destroy(ml->MP);destroylinks(ml->m);freemod(ml->m);Tmodlink.ref--;
    } else freetype(h->t);
    dealloc(h);
}
static void drop(Loader_Niladt *p) {
    Heap *h=D2H(p);freetype(h->t);dealloc(h);
}
int main(void) {
    uchar bits[2]={0x80,0};
    Type *t;
    Module m={0};Modlink ml={0};Array map={0};
    F_Loader_tnew tn={0};F_Loader_dnew dn={0};F_Loader_compile co={0};
    F_Loader_ext ext={0};
    Loader_Niladt *object=H;WORD ret;
    uchar saved[8], instance[8];
    int i, baseline;
    assert(!dtype(freeheap,-1,nil,0));assert(!dtype(freeheap,8,bits,-1));
    assert(!dtype(freeheap,8,nil,1));
    assert(!dtype(freeheap,sizeof(void*)+1,bits,1));assert(!dtype(freeheap,8,bits,0x7fffffff));
    for(i=0;i<8;i++) {
        bits[0]=0x80>>i;
        assert(!dtype(freeheap,(i+1)*sizeof(void*)-1,bits,1));
        t=dtype(freeheap,(i+1)*sizeof(void*),bits,1);
        assert(t && t->initialize==nil && t->destroy==nil);freetype(t);
    }
    bits[0]=0;t=dtype(freeheap,1,bits,1);assert(t);freetype(t);
    t=dtype(freeheap,0,nil,0);assert(t);freetype(t);assert(live==0);
    m.origmp=H;ml.m=&m;ml.MP=instance;
    m.type=alloc(sizeof(Type*));ml.type=m.type;
    tn.ret=&ret;tn.mp=&ml;tn.map=H;tn.size=8;
    baseline=live;fail_after=1;Loader_tnew(&tn);
    assert(ret==-1 && live==baseline && m.ntype==0 && ml.type==m.type);
    fail_after=-1;Loader_tnew(&tn);assert(ret==0 && m.ntype==1 && ml.type==m.type);
    baseline=live;fail_after=0;Loader_tnew(&tn);fail_after=-1;
    assert(ret==-1 && live==baseline && m.ntype==1);
    dn.ret=&object;dn.map=H;dn.size=8;
    for(i=0;i<100;i++) {
        baseline=live;fail_heap=1;Loader_dnew(&dn);
        assert(object==H && live==baseline && worker.nerr==0);
        fail_heap=0;Loader_dnew(&dn);
        assert(object!=H && D2H(object)->t->ref==1);drop(object);object=H;assert(live==baseline);
    }
    baseline=live;Loader_dnew(&dn);Loader_dnew(&dn);
    assert(object!=H && live==baseline+2);drop(object);object=H;assert(live==baseline);
    dn.size=-1;Loader_dnew(&dn);assert(object==H);
    map.len=-1;map.data=bits;dn.map=&map;dn.size=8;Loader_dnew(&dn);assert(object==H);
    if(sizeof(WORD)>4) {
        dn.map=H;dn.size=(WORD)((uvlong)1<<32);Loader_dnew(&dn);assert(object==H);
        tn.size=dn.size;baseline=live;Loader_tnew(&tn);assert(ret==-1 && live==baseline);
    }
    co.ret=&ret;co.mp=&ml;co.flag=1;m.origmp=saved;
    for(i=0;i<100;i++) {
        compile_mode=2;Loader_compile(&co);
        assert(ret==-1 && m.origmp==saved && worker.nerr==0);
        compile_mode=0;Loader_compile(&co);assert(ret==-1 && m.origmp==saved);
    }
    co.flag=0;i=calls;Loader_compile(&co);assert(ret==0 && calls==i && m.origmp==saved);
    co.flag=1;compile_mode=1;Loader_compile(&co);
    assert(ret==0 && ml.compiled && m.origmp==saved);
    ext.mp=&ml;ext.ret=&ret;Loader_ext(&ext);assert(ret==-1);
    for(i=0;i<m.ntype;i++)freetype(m.type[i]);dealloc(m.type);
    assert(live==0 && worker.nerr==0);
    {
        Loader_Inst code[2]={{0}};
        String name={0};
        Type insttype={0};Array instructions={0};
        F_Loader_newmod nm={0};Modlink *result=H;
        Loader_Niladt *data;Heap *dataheap;
        int successes=0,failures=0;
        t=dtype(freeheap,8,nil,0);dataheap=heapz(t);freetype(t);
        data=H2D(Loader_Niladt*,dataheap);
        insttype.size=sizeof(Loader_Inst);instructions.t=&insttype;
        instructions.data=(uchar*)code;instructions.len=2;
        code[0].op=IJMP;code[0].addr=SRC(AXXX)|DST(AIMM);code[0].dst=1;
        code[1].op=IRET;code[1].addr=SRC(AXXX)|DST(AXXX);
        nm.ret=&result;nm.name=&name;nm.inst=&instructions;nm.data=data;nm.nlink=1;nm.ss=1024;
        baseline=live;
        /* Nil names must fail before allocation; use a valid name for fault injection. */
        nm.name=H;Loader_newmod(&nm);assert(result==H && live==baseline);
        nm.name=nil;Loader_newmod(&nm);assert(result==H && live==baseline);
        nm.name=&name;
        for(i=0;i<12;i++) {
            fail_after=i;Loader_newmod(&nm);fail_after=-1;
            if(result==H)failures++;
            else {
                successes++;
                assert(result->m->prog[0].d.ins==result->m->prog+1);
                assert(result->MP==(uchar*)data && result->m->origmp==H);
                assert(result->m->ext && result->m->ext[0].name==nil);
                assert(result->m->type[0]==t && dataheap->ref==2 && t->ref==2);
                destroy(result);result=H;
            }
            assert(live==baseline && dataheap->ref==1 && t->ref==1 && worker.nerr==0 && Tmodlink.ref==1);
        }
        assert(successes && failures);
        fail_heap=1;Loader_newmod(&nm);fail_heap=0;
        assert(result==H && live==baseline && t->ref==1 && dataheap->ref==1);
        throw_string=1;Loader_newmod(&nm);throw_string=0;
        assert(result==H && live==baseline && worker.nerr==0);
        code[0].dst=2;Loader_newmod(&nm);assert(result==H && live==baseline);code[0].dst=1;
        code[0].op=MAXDIS;Loader_newmod(&nm);assert(result==H && live==baseline);code[0].op=IJMP;
        code[0].addr=SRC(6)|DST(AIMM);Loader_newmod(&nm);assert(result==H && live==baseline);
        code[0].addr=SRC(AXXX)|DST(AIMM)|AXIMM;code[0].mid=65536;
        Loader_newmod(&nm);assert(result==H && live==baseline);
        code[0].mid=-32768;Loader_newmod(&nm);assert(result!=H);destroy(result);result=H;
        code[0].mid=65535;Loader_newmod(&nm);assert(result!=H);destroy(result);result=H;
        code[0].addr=SRC(AXXX)|DST(AIMM)|AXINF;code[0].mid=-1;
        Loader_newmod(&nm);assert(result==H && live==baseline);
        code[0].mid=0;code[0].addr=SRC(AXXX)|DST(AFP);code[0].dst=42;
        Loader_newmod(&nm);assert(result!=H && result->m->prog[0].d.ind==42);
        Loader_newmod(&nm);assert(result!=H && dataheap->ref==2 && t->ref==2);
        destroy(result);result=H;
        instructions.len=-1;Loader_newmod(&nm);assert(result==H && live==baseline);
        instructions.len=0;Loader_newmod(&nm);assert(result==H && live==baseline);
        if(sizeof(WORD)>4) {
            instructions.len=(WORD)((uvlong)1<<32);Loader_newmod(&nm);assert(result==H && live==baseline);
            instructions.len=2;nm.nlink=(WORD)((uvlong)1<<32);Loader_newmod(&nm);assert(result==H && live==baseline);
        }
        instructions.len=2;nm.nlink=1;insttype.size=1;
        Loader_newmod(&nm);assert(result==H && live==baseline);
        destroy(data);
        assert(live==0 && worker.nerr==0 && Tmodlink.ref==1);
    }
    puts("PASS loader runtime: bitmap bounds, Type ownership, realloc/heap failures, VM errors, retry, MP restoration, compiled export rejection and transactional newmod");
}
'''
loader=(root/'libinterp/loader.c').read_text()
helpers=loader[loader.index('typedef struct LoaderHeap'):loader.index('void\nLoader_dnew(')]
parts=[function('emu/port/dis.c','kprotect','int'),function('libinterp/heap.c','dtype','Type*'),function('libinterp/heap.c','freetype','void'),helpers]
parts += [function('libinterp/link.c','destroylinks','void'),function('libinterp/link.c','mklinkmod','Modlink*'),function('libinterp/load.c','freemod','void'),function('libinterp/load.c','brpatch','int')]
parts += [loader[loader.index('typedef struct LoaderModule'):loader.index('void\nLoader_tnew(')]]
parts += [function('libinterp/loader.c',n,r) for n,r in [('Loader_tnew','void'),('Loader_dnew','void'),('loadercompile','static int'),('Loader_compile','void'),('Loader_ext','void')]]
unit=unit.replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='loader-runtime-') as directory:
    td=Path(directory);p=td/'runtime.c';p.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O2 -g'))
    command += ['-I'+str(root/p) for p in ['Linux/arm/include','emu/port','include','libinterp']]
    subprocess.run(command+[str(p),'-o',str(td/'runtime')],check=True)
    subprocess.run([str(td/'runtime')],check=True)
