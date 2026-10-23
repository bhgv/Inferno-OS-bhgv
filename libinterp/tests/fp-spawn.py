#!/usr/bin/env python3
"""Run both production newprog bodies with scheduler/allocation/FPU boundary stubs.

Real emulator headers supply Prog/Osenv/Heap. The native-kernel function is
compiled through an incref boundary adapter; this is not a native-kernel build.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
unit=r'''
#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "raise.h"
#include <assert.h>
#include <stdio.h>
static struct {Prog *head,*tail;} isched;
static Prog *slots[32],*running;
static uint live_fpscr;
static int saves,restores,destructive;
int mutator,nprop;
void *testmalloc(size_t n) {return calloc(1,n);}
int __lsan_is_turned_off(void) {return 1;}
char exNomem[]="out of memory";
static Prog **pidlook(int pid) {assert(pid>0 && pid<32);return &slots[pid];}
Prog *currun(void) {return running;}
void xec(Prog *p) {(void)p;}
void addrun(Prog *p) {(void)p;}
void newgrp(Prog *p) {(void)p;}
static void addgrp(Prog *p,Prog *parent) {(void)p;(void)parent;}
static int testincref(Ref *r) {return ++r->ref;}
#define incref(p) testincref((Ref*)(p))
void kstrdup(char **p,char *s) {*p=malloc(strlen(s)+1);assert(*p);strcpy(*p,s);}
void panic(char *text,...) {(void)text;assert(0);}
void error(char *text) {(void)text;assert(0);}
int kprotect(int (*fn)(void*),void *arg) {return fn(arg);}
void krethrow(void) {assert(0);}
void FPsave(void *v) {
    FPU *f=v;saves++;f->fpscr=live_fpscr;
    if(destructive)live_fpscr=0;
}
void FPrestore(void *v) {FPU *f=v;restores++;live_fpscr=f->fpscr;}
NEWPROG_IMPLEMENTATION
static void discard(Prog *p,Modlink *m) {
    free(((Osenv*)p->osenv)->user);free(p);D2H(m)->ref--;
    isched.head=isched.tail=nil;memset(slots,0,sizeof(slots));
}
int main(void) {
    Heap *h;
    Modlink *m;
    Prog parent={0},other={0},*child;
    Osenv env={0};
    Pgrp pg={0};Fgrp fg={0};Egrp eg={0};
    int i,j;
    uint values[]={0,0x00400011,0x00800002,0x00c00f1f,0xf900809f};
    h=calloc(1,sizeof(Heap)+sizeof(Modlink));assert(h);h->ref=1;m=H2D(Modlink*,h);m->MP=H;
    env.pgrp=&pg;env.fgrp=&fg;env.egrp=&eg;env.user="owner";
    parent.osenv=&env;parent.flags=Prestrict;
    for(i=0;i<2;i++)for(j=0;j<5;j++) {
        env.fpu.fpscr=0x111;live_fpscr=values[j];running=&parent;
        destructive=i;saves=restores=0;
        child=newprog(&parent,m);
        assert(((Osenv*)child->osenv)->fpu.fpscr==values[j] && env.fpu.fpscr==values[j]);
        assert(live_fpscr==values[j] && saves==1 && restores==1);
        assert(child->flags&Prestricted);
        assert(((Osenv*)child->osenv)->user!=env.user && strcmp(((Osenv*)child->osenv)->user,env.user)==0);
        assert(((Osenv*)child->osenv)->errstr==((Osenv*)child->osenv)->errbuf0);
        live_fpscr=0x222;env.fpu.fpscr=0x333;
        assert(((Osenv*)child->osenv)->fpu.fpscr==values[j]);
        discard(child,m);
    }
    running=&other;env.fpu.fpscr=0x555;live_fpscr=0x777;saves=restores=0;
    child=newprog(&parent,m);
    assert(((Osenv*)child->osenv)->fpu.fpscr==0x555 && live_fpscr==0x777 && !saves && !restores);
    discard(child,m);
    saves=restores=0;child=newprog(nil,m);
    assert(!saves && !restores);discard(child,m);
    assert(h->ref==1);free(h);
    puts("PASS spawn FPU inheritance: live/stored parent, destructive-save restoration, independent child and root creation");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='fp-spawn-') as directory:
    td=Path(directory)
    for variant in ['emu','os']:
        source=(root/variant/'port/dis.c').read_text()
        start=source.index('static int\nnewrootgrp(');end=source.index('void\ndelprog(',start)
        path=td/(variant+'.c');path.write_text(unit.replace('NEWPROG_IMPLEMENTATION',source[start:end]))
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g'))
        command+=['-Dmalloc=testmalloc']
        command+=['-I'+str(root/p) for p in ['Linux/arm/include','emu/port','include']]
        command+=[str(path),'-o',str(td/variant)]
        subprocess.run(command,check=True)
        environment=dict(os.environ);environment.setdefault('UBSAN_OPTIONS','halt_on_error=1')
        subprocess.run([str(td/variant)],check=True,env=environment)
