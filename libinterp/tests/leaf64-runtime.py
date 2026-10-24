#!/usr/bin/env python3
"""Exercise production leaf compilers and execute AMD64 leaves on the native Linux host."""
import os
import platform
import re
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
#define OP(name) void name(void)
#define W(r) (*(WORD*)R.r)
#define A(r) (*(Array**)R.r)
#define UW(r) (*(UWORD*)R.r)
char exInval[]="invalid", exZdiv[]="zero divide", exBounds[]="bounds";
VM_OPERATIONS
static Module *active;
static int activeop;
/* Invoke actual interpreter helpers only when native code rejects an input. */
static int dispatch(void *unused) {
    (void)unused;
    if(active->jit->entry[activeop] && active->jit->entry[activeop](&R))return 1;
    switch(activeop) {
    case IDIVW:divw();break;
    case IMODW:modw();break;
    case ISHLW:shlw();break;
    case ISHRW:shrw();break;
    case ILSRW:lsrw();break;
    case IINDB:indb();break;
    case IINDW:indw();break;
    case IINDL:indl();break;
    case IINDF:indf();break;
    case IINDX:indx();break;
    case ILENA:lena();break;
    default:assert(0);
    }
    return 1;
}
/* Compare native execution/fallback with VM exceptions and untouched rejected results. */
static void arithmetic(Module *m) {
    WORD minimum=(UWORD)1<<(sizeof(WORD)*8-1);
    WORD values[]={0,1,-1,minimum,(WORD)((UWORD)minimum-1)};
    WORD a,b,out,wanted;
    Inst *pc=m->prog+1;
    int i,j,op;
    active=m;
    R.PC=pc;
    for(op=0;op<2;op++)for(i=0;i<5;i++)for(j=0;j<5;j++) {
        a=values[i];b=values[j];out=123;
        R.s=&a;R.m=&b;R.d=&out;activeop=op?IMODW:IDIVW;
        if(a==0){assert(!kprotect(dispatch,nil));assert(out==123);}
        else {
            wanted=b==minimum && a==-1 ? (op?0:minimum) : op?b%a:b/a;
            assert(kprotect(dispatch,nil) && out==wanted);
        }
        assert(R.PC==pc);
    }
    for(op=0;op<3;op++)for(i=-1;i<=(int)sizeof(WORD)*8;i++) {
        a=i;b=-7;out=123;
        R.s=&a;R.m=&b;R.d=&out;activeop=op==0?ISHLW:op==1?ISHRW:ILSRW;
        if(i<0 || i>=(int)sizeof(WORD)*8){assert(!kprotect(dispatch,nil));assert(out==123);}
        else {
            wanted=op==0?(UWORD)b<<i:op==1?b>>i:(UWORD)b>>i;
            assert(kprotect(dispatch,nil) && out==wanted);
        }
        assert(R.PC==pc);
    }
}
/* Check real published binary64 sign operations without evaluating a NaN in C. */
static void realnegation(Module *m) {
    uvlong bits[]={0,1,0x8000000000000000ULL,0x3ff0000000000000ULL,
        0x7ff0000000000000ULL,0xfff0000000000000ULL,
        0x7ff8000000000042ULL,0x7ff0000000000042ULL};
    uvlong result, wanted;
    uchar storage[24];
    int i, alias;
    Inst *pc=m->prog+1;
    for(i=0;i<nelem(bits);i++)for(alias=0;alias<3;alias++) {
        memset(storage,0xa5,sizeof(storage));
        memcpy(storage+4,&bits[i],8);
        R.s=storage+4;R.d=storage+(alias==0?12:alias==1?4:8);R.PC=pc;
        wanted=bits[i]^0x8000000000000000ULL;
        assert(m->jit->entry[INEGF](&R)==1);
        memcpy(&result,R.d,8);
        assert(result==wanted && R.PC==pc);
        assert(storage[0]==0xa5 && storage[23]==0xa5);
    }
}
/* Use real Array/REG offsets and actual VM fallback on the native host. */
static void arrays(Module *m) {
    Type type={0};Array array={0},*owner;
    uchar bytes[128];
    WORD index,out;
    int operations[]={IINDB,IINDW,IINDL,IINDF},op,i,stride;
    Inst *pc=m->prog+1;
    active=m;R.PC=pc;
    array.len=8;array.data=bytes;array.t=&type;
    for(op=0;op<4;op++)for(i=-1;i<=8;i++) {
        owner=&array;index=i;out=123;
        R.s=&owner;R.d=&index;R.m=&out;activeop=operations[op];
        if(sizeof(WORD)==4)assert(!m->jit->entry[activeop]);
        if(i<0 || i>=8){assert(!kprotect(dispatch,nil));assert(out==123);}
        else {
            stride=op==0?1:op==1?sizeof(WORD):8;
            assert(kprotect(dispatch,nil) && (UWORD)out==(UWORD)(uintptr)(bytes+i*stride));
        }
        assert(R.PC==pc);
    }
    for(i=0;i<3;i++) {
        owner=i==0?nil:i==1?H:&array;out=123;
        R.s=&owner;R.d=&out;activeop=ILENA;
        assert(kprotect(dispatch,nil) && out==(i==2?8:0));
    }
    owner=&array;array.data=nil;type.size=0;index=3;out=123;
    R.s=&owner;R.d=&index;R.m=&out;activeop=IINDX;
    assert(kprotect(dispatch,nil) && out==0);
    activeop=IINDB;out=123;
    assert(!kprotect(dispatch,nil) && out==123 && R.PC==pc);
}
/* Compile, invoke, reject invalid metadata and retry without changing Dis PCs. */
int main(void) {
    Module m={0}, *owned;Inst instructions[15]={{0}};
    WORD a=3,b=7,out=0;
    int i;
    instructions[0].op=ISUBW;instructions[1].op=IADDW;
    instructions[2].op=IMOVP;instructions[3].op=IADDW;
    instructions[4].op=IDIVW;instructions[5].op=IMODW;
    instructions[6].op=ISHLW;instructions[7].op=ISHRW;instructions[8].op=ILSRW;
    instructions[9].op=ILENA;instructions[10].op=IINDB;instructions[11].op=IINDW;
    instructions[12].op=IINDL;instructions[13].op=IINDF;instructions[14].op=INEGF;
    m.prog=instructions;m.nprog=15;
    for(i=1;i<=5;i++) {
        fail=i;assert(!compile(&m,15,nil));assert(!m.jit && !m.compiled && m.prog==instructions && mappings==0);
    }
    fail=0;
    assert(!compile(nil,15,nil));assert(!compile(&m,3,nil));
    assert(compile(&m,15,nil));assert(m.jit && !m.compiled && m.prog==instructions);
    assert(m.jit->entry[IMOVP]==nil);
    assert(protections==1 && mappings==1);
    R.s=&a;R.m=&b;R.d=&out;
#if EXECUTE_NATIVE
    assert(m.jit->entry[ISUBW](&R)==1 && out==4);
    R.d=&a;assert(m.jit->entry[IADDW](&R)==1 && a==10);
    arithmetic(&m);arrays(&m);realnegation(&m);
#endif
    assert(compile(&m,15,nil));dropmodule(&m);
    for(i=0;i<15;i++)instructions[i].op=IMOVP;
    assert(!compile(&m,15,nil) && !m.jit);
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
xec=(root/'libinterp/xec.c').read_text()
parts=[]
for signature in ('static WORD\nshiftcount(', 'OP(divw)', 'OP(modw)', 'OP(shlw)', 'OP(shrw)', 'OP(lsrw)', 'static uchar*\narrayaddress(', 'OP(indx)', 'OP(indb)', 'OP(indw)', 'OP(indl)', 'OP(indf)', 'OP(lena)'):
    a=xec.index(signature);b=xec.index('\n}',a)+2
    parts.append(xec[a:b])
unit=unit.replace('VM_OPERATIONS','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='leaf64-runtime-') as directory:
    td=Path(directory);source=td/'runtime.c';source.write_text(unit)
    includes=[]
    if os.environ.get('LEAF_WORD_BITS')=='32':
        header=(root/'include/interp.h').read_text()
        header=re.sub(r'typedef long\s+WORD;', 'typedef int WORD;',header,count=1)
        header=re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;',header,count=1)
        (td/'interp.h').write_text(header)
        includes=['-I'+str(td)]
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd+=includes
    cmd+=['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    subprocess.run(cmd+[str(source),'-o',str(td/'runtime')],check=True)
    subprocess.run([str(td/'runtime')],check=True)
