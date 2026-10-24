#!/usr/bin/env python3
"""Exercise production leaf compilers and execute AMD64 leaves on the native Linux host."""
import os
import platform
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
backend=os.environ.get('LEAF_RUNTIME_BACKEND','amd64')
if backend not in ('amd64','arm64','riscv64'):raise ValueError(backend)
if backend=='amd64' and platform.machine().lower() not in ('x86_64','amd64'):
    raise RuntimeError('native AMD64 execution requires an x86-64 Linux host')
unit=r'''
#define mallocz fixturemallocz
#define mmap fixturemap
#define munmap fixtureunmap
#define mprotect fixtureprotect
#include "comp-amd64.c"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
static int fail, mappings, protections;
extern void *native_map(void*,size_t,int,int,int,off_t) __asm__("mmap");
extern int native_unmap(void*,size_t) __asm__("munmap");
extern int native_protect(void*,size_t,int) __asm__("mprotect");
static jmp_buf frame;
static int trapped;
REG R;
/* Inject descriptor allocation failure without replacing executable mappings. */
void *fixturemallocz(ulong size, int clear) {
    void *p;
    if(fail == 1)return nil;
    if(fail == 2)error("injected");
    p=malloc(size);
    if(p && clear)memset(p,0,size);
    return p;
}
/* Keep the actual compiler's error ownership path active. */
int kprotect(int (*fn)(void*),void *arg) {
    int result;
    trapped=1;
    if(setjmp(frame)){trapped=0;return 0;}
    result=fn(arg);trapped=0;return result;
}
void error(char *s) { (void)s;assert(trapped);longjmp(frame,1); }
int __lsan_is_turned_off(void) {return 1;}
/* Release a published descriptor using its production mapping owner. */
static void dropmodule(Module *m) {
    assert(m->jit && m->jit->freecode);
    m->jit->freecode(m->jit->code,m->jit->size);
    free(m->jit);m->jit=nil;
}
/* Call real mapping APIs while checking each failure and permission transition. */
void *fixturemap(void *p,size_t n,int prot,int flags,int fd,off_t offset) {
    void *result;
    assert(prot==(PROT_READ|PROT_WRITE));
    if(fail==3)return MAP_FAILED;
    result=native_map(p,n,prot,flags,fd,offset);
    if(result!=MAP_FAILED)mappings++;
    return result;
}
int fixtureunmap(void *p,size_t n) {
    assert(mappings>0);mappings--;return native_unmap(p,n);
}
int fixtureprotect(void *p,size_t n,int prot) {
    assert(prot==(PROT_READ|PROT_EXEC));
    if(fail==4)return -1;
    if(fail==5)error("publish injected");
    protections++;return native_protect(p,n,prot);
}
void freetype(Type *t) { (void)t;assert(0); }
FREEMOD
/* Compile, invoke, reject invalid metadata and retry without changing Dis PCs. */
int main(void) {
    Module m={0}, *owned;Inst instructions[4]={{0}};
    WORD a=3,b=7,out=0;
    int i;
    instructions[0].op=ISUBW;instructions[1].op=IADDW;
    instructions[2].op=IMOVP;instructions[3].op=IADDW;
    m.prog=instructions;m.nprog=4;
    for(i=1;i<=5;i++) {
        fail=i;assert(!compile(&m,4,nil));assert(!m.jit && !m.compiled && m.prog==instructions && mappings==0);
    }
    fail=0;
    assert(!compile(nil,4,nil));assert(!compile(&m,3,nil));
    assert(compile(&m,4,nil));assert(m.jit && !m.compiled && m.prog==instructions);
    assert(m.jit->entry[IMOVP]==nil);
    assert(protections==1 && mappings==1);
    R.s=&a;R.m=&b;R.d=&out;
#if EXECUTE_NATIVE
    assert(m.jit->entry[ISUBW](&R)==1 && out==4);
    R.d=&a;assert(m.jit->entry[IADDW](&R)==1 && a==10);
#endif
    assert(compile(&m,4,nil));dropmodule(&m);
    instructions[0].op=instructions[1].op=instructions[2].op=instructions[3].op=IMOVP;
    assert(!compile(&m,4,nil) && !m.jit);
    owned=calloc(1,sizeof(*owned));assert(owned);
    owned->prog=calloc(1,sizeof(Inst));assert(owned->prog);
    owned->nprog=1;owned->prog[0].op=IADDW;
    assert(compile(owned,1,nil) && mappings==1);
    freemod(owned);assert(mappings==0);
    puts("PASS BACKEND compiler: RX publication, fallback table, metadata/failure/retry and unmap");
    return 0;
}
'''
unit=unit.replace('comp-amd64.c','comp-'+backend+'.c').replace('EXECUTE_NATIVE','1' if backend=='amd64' else '0').replace('BACKEND',backend)
if backend=='riscv64':unit='#define RV_CODE_PAGES\n'+unit
load=(root/'libinterp/load.c').read_text()
a=load.index('void\nfreemod(');b=load.index('\nvoid\nunload(',a)
unit=unit.replace('FREEMOD',load[a:b])
with tempfile.TemporaryDirectory(prefix='leaf64-runtime-') as directory:
    td=Path(directory);source=td/'runtime.c';source.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd+=['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    subprocess.run(cmd+[str(source),'-o',str(td/'runtime')],check=True)
    subprocess.run([str(td/'runtime')],check=True)
