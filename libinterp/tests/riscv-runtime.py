#!/usr/bin/env python3
"""Host checks of production compile/freemod/xec code, using fake native leaves.

Generated RV instructions are executed by riscv-jit.py. Here native callbacks
are C stand-ins so scheduler/module/allocator paths can run on the host CPU.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]

def extract(path, name, result):
    src=(root/path).read_text()
    start=src.index(result+'\n'+name+'(')
    pos=src.index('{',start)+1
    depth=1
    while depth:
        depth+=(src[pos]=='{')-(src[pos]=='}')
        pos+=1
    return src[start:pos]

unit=r'''
#define malloc testmalloc
#define mallocz testmallocz
#define free testfree
#include "comp-riscv64.c"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
static jmp_buf errorframes[8];
static int errdepth,throw_flush;
extern void hostfree(void*) __asm__("free");
int __lsan_is_turned_off(void) {return 1;}
static int live, fail_after=-1, flushes, flush_result;
void *testmalloc(size_t n) {
    void *p;
    if(fail_after==0) return nil;
    if(fail_after>0) fail_after--;
    p=calloc(1,n);
    if(p) live++;
    return p;
}
void *testmallocz(ulong n, int clear) { (void)clear; return testmalloc(n); }
void testfree(void *p) { if(p) { live--; hostfree(p); } }
int segflush(void *p, ulong n) { assert(p && n); flushes++; if(throw_flush)error("flush injected"); return flush_result; }
void freetype(Type *t) { (void)t; assert(0); }
void error(char *s) { if(errdepth)longjmp(errorframes[--errdepth],1); fprintf(stderr,"unexpected VM error: %s\n",s); abort(); }
REG R;
void (*optab[256])(void);
void (*dec[256])(void);
int cflag;

typedef struct {
    WORD *ret; // Borrowed full-width Limbo status slot.
    Modlink *mp; // Borrowed module to compile.
    int flag; // Explicit compilation request.
} F_Loader_compile;
char exNilref[]="nil", exHeap[]="heap", exInval[]="invalid", exModule[]="module", exBounds[]="bounds", exCompile[]="compile", exOp[]="opcode"; // Errors used by the shared VM executor.
int kprotect(int (*fn)(void*),void *arg) {
 int r;
 assert(errdepth<8);
 if(setjmp(errorframes[errdepth++]))return 0;
 r=fn(arg);errdepth--;return r;
}
void kwerrstr(char *s, ...) { (void)s; }
''' + extract('libinterp/loader.c','loaderword','static void') + '\n' + extract('libinterp/loader.c','loadercompile','static int') + '\n' + extract('libinterp/loader.c','Loader_compile','void') + '\n' + extract('libinterp/load.c','freemod','void') + '\n' + extract('libinterp/xec.c','xec','void') + r'''
static int natives, helpers, decoded, entered, rejected;
static WORD output, source=7;
static Modlink ma, mb;
static Module a,b;
static Inst ia[3], ib[2];
static Jit ja,jb;
static int phase;
static void decode(void) {
    decoded++;
    R.s=&source; R.d=&output; R.m=&output;
    assert(R.MP==R.M->MP);
}
static void enter(void) { entered++; }
static int native(REG *r) {
    assert(r==&R && R.PC[-1].op==IMOVW);
    assert(entered==natives+rejected+1);
    output=*(WORD*)r->s; natives++;
    return 1;
}
static int refuse(REG *r) {
    assert(r==&R && R.PC[-1].op==IRUNT);
    rejected++;
    return 0;
}
static void fallback(void) { helpers++; }
static void module_switch(void) {
    assert(R.PC[-1].op==IMCALL);
    helpers++;
    R.M=&mb; R.MP=mb.MP; R.PC=ib;
}
static void loop_back(void) {
    helpers++;
    R.M=&ma; R.MP=ma.MP; R.PC=ia;
}
static void compiled_entry(void) { phase++; R.IC=0; }

int main(void) {
    Module m={0}, *owned;
    Inst prog[3]={{0}};
    Prog p={0};
    Jit *installed;
    Modlink dynamic={0};
    WORD loadresult;
    F_Loader_compile request={&loadresult,&dynamic,1};
    int before, n;
    prog[0].op=prog[1].op=IADDW;
    prog[2].op=IADDF;
    m.origmp=H; m.nprog=3; m.prog=prog; m.entry=prog+1;
    before=live;
    assert(!compile(nil,3,nil) && !compile(H,3,nil) && live==before);
    assert(!compile(&m,0,nil) && !compile(&m,-1,nil) && live==before);
    m.nprog=0x7fffffff;assert(!compile(&m,m.nprog,nil) && live==before);m.nprog=3;
    m.prog=H;assert(!compile(&m,3,nil) && live==before);m.prog=prog;
    m.prog=nil;assert(!compile(&m,3,nil) && live==before);m.prog=prog;
    throw_flush=1;
    assert(!compile(&m,3,nil) && !m.jit && live==before && errdepth==0);
    dynamic.m=&m;dynamic.MP=(uchar*)"data";
    Loader_compile(&request);
    assert(loadresult==-1 && m.origmp==H && !m.jit && live==before && errdepth==0);
    throw_flush=0;
    fail_after=0;
    assert(compile(&m,3,nil)==0 && m.jit==nil && live==before);
    fail_after=1;
    assert(compile(&m,3,nil)==0 && m.jit==nil && live==before);
    fail_after=-1; flush_result=-1;
    assert(compile(&m,3,nil)==0 && m.jit==nil && live==before);
    flush_result=0;
    assert(compile(&m,3,nil)==1 && m.jit && live==before+2);
    assert(m.prog==prog && m.entry==prog+1 && m.compiled==0);
    dynamic.m=&m; dynamic.MP=(uchar*)"data"; dynamic.compiled=123;
    Loader_compile(&request);
    assert(loadresult==0 && dynamic.compiled==0 && dynamic.prog==prog && m.origmp==H);
    assert(m.jit->entry[IADDW] && !m.jit->entry[IADDF] && m.jit->enter);
    installed=m.jit; n=flushes;
    assert(compile(&m,3,nil)==1 && m.jit==installed && flushes==n);
    assert(compile(&m,2,nil)==0 && m.jit==installed);
    testfree(m.jit->code); testfree(m.jit); m.jit=nil;
    prog[0].op=prog[1].op=prog[2].op=IADDF;
    assert(compile(&m,3,nil)==0 && !m.jit && live==before);
    m.compiled=1; prog[0].op=IADDW;
    assert(compile(&m,3,nil)==0 && !m.jit);
    m.compiled=0;
    owned=testmalloc(sizeof(Module));
    owned->nprog=1; owned->prog=testmalloc(sizeof(Inst)); owned->prog[0].op=IADDW;
    assert(compile(owned,1,nil)==1);
    freemod(owned);
    assert(live==before);

    /* Real xec loop: current opcode lookup, next PC, helper fallback,
     * module changes, quanta, and a compiled-module entry remain coherent. */
    a.prog=ia; a.nprog=3; a.jit=&ja; b.prog=ib; b.nprog=2; b.jit=&jb;
    ma.m=&a; ma.prog=ia; ma.MP=(uchar*)"a";
    mb.m=&b; mb.prog=ib; mb.MP=(uchar*)"b";
    ia[0].op=IRUNT; ia[1].op=IMOVW; ia[2].op=IMCALL;
    ib[0].op=IMOVW; ib[1].op=IRET;
    ja.enter=jb.enter=enter;
    ja.entry[IRUNT]=refuse; ja.entry[IMOVW]=jb.entry[IMOVW]=native;
    dec[0]=decode;
    optab[IRUNT]=fallback; optab[IMCALL]=module_switch; optab[IRET]=loop_back;
    p.R.M=&ma; p.R.PC=ia; p.quanta=10;
    xec(&p);
    assert(decoded==10 && natives==4 && rejected==2 && helpers==6 && entered==6);
    assert(output==source && p.R.PC==ia && p.R.M==&ma && p.R.IC==0);
    /* Runtime-modified instruction uses the current opcode, not stale code. */
    ia[1].op=IRUNT; p.quanta=2;
    xec(&p);
    assert(rejected==4 && natives==4 && helpers==8 && p.R.PC==ia+2);
    /* Module without sidecar remains entirely interpreted. */
    a.jit=nil; p.R.PC=ia; p.quanta=1;
    xec(&p); assert(helpers==9 && entered==8);
    ma.compiled=1; comvec=compiled_entry;
    xec(&p); assert(phase==1);
    assert(live==0);
    puts("PASS RISC-V runtime: atomic publication, allocation/cache/VM-error failures, cleanup, opcode mutation, quanta and module dispatch");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='riscv-runtime-') as td:
    td=Path(td)
    src=td/'runtime.c'; src.write_text(unit)
    binary=td/'runtime'
    subprocess.run(shlex.split(os.environ.get('CC','cc')) +
                   shlex.split(os.environ.get('CFLAGS','-O1 -g')) +
                   ['-I'+str(root/'Linux/386/include'),'-I'+str(root/'include'),
                    '-I'+str(root/'libinterp'), str(src),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
