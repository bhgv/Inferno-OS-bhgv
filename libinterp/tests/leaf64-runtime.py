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
#include <fenv.h>
static int fail, mappings, protections;
extern void *native_map(void*,size_t,int,int,int,off_t) __asm__("mmap");
extern int native_unmap(void*,size_t) __asm__("munmap");
extern int native_protect(void*,size_t,int) __asm__("mprotect");
static jmp_buf frame;
static int trapped;
REG R;
/* Supply the live collector color used by guarded owner leaves. */
int mutator, nprop;
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
#define B(r) (*(BYTE*)R.r)
#define SH(r) (*(SHORT*)R.r)
#define W(r) (*(WORD*)R.r)
#define A(r) (*(Array**)R.r)
#define S(r) (*(String**)R.r)
#define UW(r) (*(UWORD*)R.r)
#define F(r) (*(REAL*)R.r)
#define SR(r) (*(SREAL*)R.r)
#define V(r) (*(LONG*)R.r)
#define UV(r) (*(ULONG*)R.r)
char exInval[]="invalid", exZdiv[]="zero divide", exBounds[]="bounds", exHeap[]="heap", exNilref[]="nil";
VM_OPERATIONS
/* Invoke the C fallback through the fixture VM exception boundary. */
static int fixedscaleerror(void *arg) { (void)arg;cvtxx();return 1; }
SCALAR_TRANSFERS
FIXED_SCALE
INTEGER_ALU
SHIFT_DIV
INTEGER_REAL
REAL_ARITHMETIC
FIXED_FRAME
DESCRIPTOR_INDEX

static Module *active;
static int activeop;
/* Invoke actual interpreter helpers only when native code rejects an input. */
static int dispatch(void *unused) {
    (void)unused;
    if(active->jit->entry[activeop] && active->jit->entry[activeop](&R))return 1;
    switch(activeop) {
    case IEXPW:iexpw();break;
    case IEXPL:iexpl();break;
    case IEXPF:iexpf();break;
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
    case ILENC:lenc();break;
    case IINDC:indc();break;
    case IMOVM:movm();break;
    case IHEADB:headb();break;
    case IHEADW:headw();break;
    case IHEADL:headl();break;
    case IHEADF:headf();break;
    case IHEADM:headm();break;
    case IBEQF:beqf();break;
    case IBNEF:bnef();break;
    case IBLTF:bltf();break;
    case IBLEF:blef();break;
    case IBGTF:bgtf();break;
    case IBGEF:bgef();break;
    case ILENL:lenl();break;
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
/* Compare copied records with libc memmove, including overwritten length slots. */
static void records(Module *m) {
    uchar bytes[512], expected[512];
    int sizes[]={0,1,7,8,31,64,127,128,129};
    int deltas[]={-129,-64,-7,-1,0,1,3,7,64,129};
    int slots[]={400,200,204};
    int i,j,k,n;
    WORD length;
    Inst *pc=m->prog+1;
    active=m;activeop=IMOVM;
    for(i=0;i<nelem(sizes);i++)for(j=0;j<nelem(deltas);j++)for(k=0;k<nelem(slots);k++) {
        for(n=0;n<sizeof(bytes);n++)bytes[n]=n*29+17;
        length=sizes[i];memcpy(bytes+slots[k],&length,sizeof(length));
        memcpy(expected,bytes,sizeof(bytes));
        memmove(expected+200+deltas[j],expected+200,length);
        R.s=bytes+200;R.d=bytes+200+deltas[j];R.m=bytes+slots[k];R.PC=pc;
        assert(kprotect(dispatch,nil) && !memcmp(bytes,expected,sizeof(bytes)) && R.PC==pc);
    }
    for(i=0;i<3;i++){
        length=i==0?0:i==1?-1:(WORD)((UWORD)1<<(sizeof(WORD)*8-1));
        R.s=R.d=(void*)1;R.m=&length;R.PC=pc;
        assert(kprotect(dispatch,nil)==(i==0) && R.PC==pc);
    }
}
/* Verify native lengths and actual VM fallback against the real String layout. */
static void stringlengths(Module *m) {
    String string={0}, *source;
    WORD out;
    int values[]={-0x7fffffff,-65537,-1,0,1,65537,0x7fffffff};
    int i;
    Inst *pc=m->prog+1;
    active=m;activeop=ILENC;
    for(i=0;i<nelem(values);i++){
        string.len=values[i];source=&string;out=123;
        R.s=&source;R.d=&out;R.PC=pc;
        assert(kprotect(dispatch,nil) && out==(values[i]<0?-(WORD)values[i]:values[i]) && R.PC==pc);
    }
    source=H;R.s=&source;R.d=&out;assert(kprotect(dispatch,nil) && out==0);
    for(i=0;i<2;i++){
        source=i?&string:nil;string.len=(-0x7fffffff-1);out=123;
        R.s=&source;R.d=&out;
        assert(!kprotect(dispatch,nil) && out==123 && R.PC==pc);
    }
}
/* Compare native and interpreter heads with real layouts, aliases and errors. */
static void listheads(Module *m) {
    struct { List list; uchar extra[32]; } node;
    union { uvlong align; uchar bytes[64]; } slots;
    uchar original[sizeof(node)], expected[sizeof(node)], wantslots[64];
    uvlong bits[]={0,1,0xffffffffffffffffULL,0x8000000000000000ULL,
        0x7ff8000000000042ULL,0x7ff0000000000042ULL};
    int ops[]={IHEADB,IHEADW,IHEADL,IHEADF}, op,i,alias,mode,width;
    int (*entry)(REG*);
    List *owner;
    Inst *pc=m->prog+1;
    active=m;
    for(op=0;op<4;op++) {
        activeop=ops[op];entry=m->jit->entry[activeop];assert(entry);
        width=op==0?1:op==1?sizeof(WORD):8;
        for(mode=0;mode<2;mode++) {
            m->jit->entry[activeop]=mode?nil:entry;
            for(i=0;i<nelem(bits);i++)for(alias=0;alias<4;alias++) {
                memset(&node,0,sizeof(node));node.list.tail=H;
                memcpy(node.list.data,&bits[i],8);
                memcpy(original,&node,sizeof(node));memcpy(expected,&node,sizeof(node));
                memset(slots.bytes,0xa5,sizeof(slots.bytes));
                owner=&node.list;memcpy(slots.bytes+4,&owner,sizeof(owner));
                memcpy(wantslots,slots.bytes,sizeof(wantslots));
                R.s=slots.bytes+4;R.PC=pc;
                if(alias<2) {
                    R.d=slots.bytes+(alias?4:20);
                    memcpy(wantslots+(alias?4:20),&bits[i],width);
                }else {
                    R.d=(uchar*)node.list.data+(alias==2?4:-4);
                    memcpy(expected+((uchar*)R.d-(uchar*)&node),&bits[i],width);
                }
                assert(kprotect(dispatch,nil) && R.PC==pc);
                assert(!memcmp(&node,expected,sizeof(node)) && !memcmp(slots.bytes,wantslots,sizeof(wantslots)));
            }
            for(i=0;i<2;i++) {
                owner=i?nil:H;memcpy(slots.bytes+4,&owner,sizeof(owner));
                R.s=slots.bytes+4;R.d=slots.bytes+20;R.PC=pc;
                memcpy(wantslots,slots.bytes,sizeof(wantslots));
                assert(!kprotect(dispatch,nil) && R.PC==pc && !memcmp(slots.bytes,wantslots,sizeof(wantslots)));
            }
        }
        m->jit->entry[activeop]=entry;
    }
}
/* Scan short lists natively and long or malformed lists through the VM helper. */
static void listlengths(Module *m) {
    List nodes[130], *owner;
    union { uvlong align; uchar bytes[64]; } slots;
    uchar expected[64];
    WORD result;
    int n,i,terminal,alias,mode;
    int (*entry)(REG*)=m->jit->entry[ILENL];
    Inst *pc=m->prog+1;
    assert(entry);active=m;activeop=ILENL;
    for(mode=0;mode<2;mode++) {
        m->jit->entry[ILENL]=mode?nil:entry;
        for(n=0;n<=130;n++)for(terminal=0;terminal<2;terminal++)for(alias=0;alias<2;alias++) {
            for(i=0;i<n;i++)nodes[i].tail=i+1<n?&nodes[i+1]:terminal?nil:H;
            owner=n?nodes:terminal?nil:H;
            memset(slots.bytes,0xa5,sizeof(slots.bytes));
            memcpy(slots.bytes+4,&owner,sizeof(owner));memcpy(expected,slots.bytes,sizeof(expected));
            result=n;
            if(!terminal)memcpy(expected+(alias?4:20),&result,sizeof(result));
            R.s=slots.bytes+4;R.d=slots.bytes+(alias?4:20);R.PC=pc;
            assert(kprotect(dispatch,nil)==!terminal && R.PC==pc);
            assert(!memcmp(slots.bytes,expected,sizeof(expected)));
        }
    }
    m->jit->entry[ILENL]=entry;
}
/* Compare character reads with forced VM execution in both string representations. */
static void characters(Module *m) {
    struct { String string; uchar extra[64]; } node;
    union { uvlong align; uchar bytes[64]; } slots;
    uchar original[sizeof(node)], expected[sizeof(node)], wantslots[64];
    Rune values[]={0,0x7f,0x80,0xff,0x10ffff,0x7fffffff,0x80000000U,0xffffffffU};
    WORD index,result;
    String *owner;
    Inst *pc=m->prog+1;
    int (*entry)(REG*)=m->jit->entry[IINDC];
    int mode,wide,i,alias,bad,valid;
    assert(entry);active=m;activeop=IINDC;
    for(mode=0;mode<2;mode++) {
        m->jit->entry[IINDC]=mode?nil:entry;
        for(wide=0;wide<2;wide++)for(i=-1;i<=8;i++)for(alias=0;alias<4;alias++) {
            memset(&node,0,sizeof(node));node.string.len=wide?-8:8;
            for(bad=0;bad<8;bad++) {
                if(wide)memcpy((uchar*)&node+O(String, Sascii)+bad*4,&values[bad],4);
                else *((uchar*)&node+O(String, Sascii)+bad)=(uchar)values[bad];
            }
            memcpy(original,&node,sizeof(node));memcpy(expected,&node,sizeof(node));
            memset(slots.bytes,0xa5,sizeof(slots.bytes));
            owner=&node.string;index=i;
            memcpy(slots.bytes+4,&owner,sizeof(owner));memcpy(slots.bytes+20,&index,sizeof(index));
            memcpy(wantslots,slots.bytes,sizeof(wantslots));
            R.s=slots.bytes+4;R.m=slots.bytes+20;R.PC=pc;
            R.d=alias<3?slots.bytes+(alias==0?36:alias==1?4:20):(void*)(node.string.Sascii+4);
            valid=i>=0 && i<8;
            if(valid) {
                result=wide?(int)values[i]:(uchar)values[i];
                if(alias<3)memcpy(wantslots+(alias==0?36:alias==1?4:20),&result,sizeof(result));
                else memcpy(expected+((uchar*)R.d-(uchar*)&node),&result,sizeof(result));
            }
            assert(kprotect(dispatch,nil)==valid && R.PC==pc);
            assert(!memcmp(&node,expected,sizeof(node)) && !memcmp(slots.bytes,wantslots,sizeof(wantslots)));
        }
        for(bad=0;bad<3;bad++) {
            owner=bad==0?nil:bad==1?H:&node.string;node.string.len=(-0x7fffffff-1);
            index=0;result=77;R.s=&owner;R.m=&index;R.d=&result;R.PC=pc;
            assert(!kprotect(dispatch,nil) && result==77 && R.PC==pc);
        }
    }
    m->jit->entry[IINDC]=entry;
}
/* Compare both dispatch paths with a high-bit-first modular power oracle. */
static void powers(Module *m) {
    union { uvlong align; uchar bytes[64]; } slots;
    uchar expected[64],captured[64];
    WORD minimum=(UWORD)1<<(sizeof(WORD)*8-1),n,wordbase;
    WORD exponents[]={minimum,-3,-2,-1,0,1,2,3,7,31,32,63,64,(WORD)((UWORD)minimum-1)};
    LONG bases[]={0,1,-1,2,-2,(-9223372036854775807LL-1),9223372036854775807LL},base;
    uvlong want, factor;
    int op,mode,i,j,alias,width,valid,bit,offset;
    int (*entry)(REG*);
    Inst *pc=m->prog+1;
    active=m;
    for(op=0;op<2;op++) {
        activeop=op?IEXPL:IEXPW;entry=m->jit->entry[activeop];assert(entry);
        width=op?8:sizeof(WORD);
        for(mode=0;mode<2;mode++) {
            m->jit->entry[activeop]=mode?nil:entry;
            for(i=0;i<nelem(exponents);i++)for(j=0;j<nelem(bases);j++)for(alias=0;alias<5;alias++) {
                n=exponents[i];wordbase=bases[j];base=op?bases[j]:(LONG)wordbase;
                valid=n>=0 || base!=0;
                want=1;factor=base;
                if(n<0)want=base==1?1:base==-1?(n&1?(uvlong)-1:1):0;
                else for(bit=(int)sizeof(WORD)*8-1;bit>=0;bit--) {
                    want*=want;
                    if(((UWORD)n>>bit)&1)want*=factor;
                }
                memset(slots.bytes,0xa5,sizeof(slots.bytes));
                memcpy(slots.bytes+4,&n,sizeof(n));
                if(op)memcpy(slots.bytes+20,&base,8);
                else memcpy(slots.bytes+20,&wordbase,sizeof(wordbase));
                memcpy(expected,slots.bytes,sizeof(expected));
                offset=alias==0?36:alias==1?4:alias==2?20:alias==3?8:24;
                if(valid)memcpy(expected+offset,&want,width);
                R.s=slots.bytes+4;R.m=slots.bytes+20;R.d=slots.bytes+offset;R.PC=pc;
                if(!mode){memmove(captured,slots.bytes,sizeof(captured));assert(entry(&R)==valid);memmove(slots.bytes,captured,sizeof(captured));}
                assert(kprotect(dispatch,nil)==valid && R.PC==pc);
                assert(!memcmp(slots.bytes,expected,sizeof(expected)));
            }
        }
        m->jit->entry[activeop]=entry;
    }
    /* Exercise native and C dispatch for full-width minimum real exponents. */
    entry=m->jit->entry[IEXPF];assert(entry);activeop=IEXPF;n=minimum;
    for(mode=0;mode<2;mode++)for(j=0;j<2;j++)for(alias=0;alias<5;alias++) {
        REAL input=j?2.0:-1.0, answer=j?0.0:1.0;
        m->jit->entry[IEXPF]=mode ? nil : entry;
        memset(slots.bytes,0xa5,sizeof(slots.bytes));
        memcpy(slots.bytes+4,&n,sizeof(n));memcpy(slots.bytes+20,&input,sizeof(input));
        memcpy(expected,slots.bytes,sizeof(expected));
        offset=alias==0?36:alias==1?4:alias==2?20:alias==3?8:24;
        memcpy(expected+offset,&answer,sizeof(answer));
        R.s=slots.bytes+4;R.m=slots.bytes+20;R.d=slots.bytes+offset;R.PC=pc;
        assert(kprotect(dispatch,nil) && R.PC==pc && !memcmp(slots.bytes,expected,sizeof(expected)));
    }

    m->jit->entry[IEXPF]=entry;
}
/* Raw heads share memmove semantics, guard precedence and large-record fallback. */
static void headrecords(Module *m) {
    struct { List list; uchar extra[256]; } node;
    union { uvlong align; uchar bytes[256]; } slots;
    uchar expected[sizeof(node)], wantslots[256];
    List *owner;
    WORD length;
    int sizes[]={0,1,7,8,31,64,127,128,129};
    int deltas[]={-4,-1,0,1,4,17};
    int mode,i,j,k,alias;
    int (*entry)(REG*)=m->jit->entry[IHEADM];
    Inst *pc=m->prog+1;
    assert(entry);active=m;activeop=IHEADM;
    for(mode=0;mode<2;mode++) {
        m->jit->entry[IHEADM]=mode?nil:entry;
        for(i=0;i<nelem(sizes);i++)for(j=0;j<nelem(deltas);j++)for(alias=0;alias<3;alias++) {
            memset(&node,0,sizeof(node));node.list.tail=H;
            for(k=0;k<256;k++)((uchar*)node.list.data)[k]=k*29+17;
            memset(slots.bytes,0xa5,sizeof(slots.bytes));
            owner=&node.list;length=sizes[i];
            memcpy(slots.bytes+4,&owner,sizeof(owner));memcpy(slots.bytes+20,&length,sizeof(length));
            memcpy(expected,&node,sizeof(node));memcpy(wantslots,slots.bytes,sizeof(wantslots));
            R.s=slots.bytes+4;R.m=slots.bytes+20;R.PC=pc;
            R.d=alias==0?(uchar*)node.list.data+deltas[j]:slots.bytes+(alias==1?4:20);
            if(alias==0)memmove(expected+((uchar*)R.d-(uchar*)&node),node.list.data,length);
            else memmove(wantslots+(alias==1?4:20),node.list.data,length);
            assert(kprotect(dispatch,nil) && R.PC==pc);
            assert(!memcmp(&node,expected,sizeof(node)) && !memcmp(slots.bytes,wantslots,sizeof(wantslots)));
        }
        for(i=0;i<3;i++) {
            owner=i==0?H:i==1?nil:&node.list;
            length=-1;memcpy(slots.bytes+4,&owner,sizeof(owner));
            R.s=slots.bytes+4;R.d=slots.bytes+36;R.m=i<2?(void*)1:&length;R.PC=pc;
            memcpy(wantslots,slots.bytes,sizeof(wantslots));
            assert(!kprotect(dispatch,nil) && R.PC==pc && !memcmp(slots.bytes,wantslots,sizeof(wantslots)));
        }
    }
    m->jit->entry[IHEADM]=entry;
}
/* Numeric C predicates are independent of the native integer key transform. */
static int realrelation(int condition, REAL left, REAL right) {
    volatile REAL a=left,b=right;
    switch(condition) {
    case 0:return a==b;
    case 1:return a!=b;
    case 2:return a<b;
    case 3:return a<=b;
    case 4:return a>b;
    default:return a>=b;
    }
}
/* Observe native completion/fallback without changing rounding or FP status. */
static void realbranches(Module *m) {
    uvlong bits[]={0,0x8000000000000000ULL,0x3ff0000000000000ULL,0xbff0000000000000ULL,
        0x0010000000000000ULL,0x8010000000000000ULL,0x7fefffffffffffffULL,0xffefffffffffffffULL,
        0x7ff0000000000000ULL,0xfff0000000000000ULL,1,0x8000000000000001ULL,
        0xfffffffffffffULL,0x800fffffffffffffULL,0x7ff8000000000042ULL,0xfff8000000000042ULL,
        0x7ff0000000000042ULL,0xfff0000000000042ULL};
    int ops[]={IBEQF,IBNEF,IBLTF,IBLEF,IBGTF,IBGEF};
    int rounds[]={FE_TONEAREST,FE_DOWNWARD};
    int mode,round,op,i,j,take,valid,expa,expb,flags,completed;
    union { uvlong align; uchar bytes[64]; } slots;
    uchar before[64];
    REAL a,b;
    fenv_t saved;
    Inst *pc=m->prog,*target=m->prog+2;
    int (*entry)(REG*);
    assert(fegetenv(&saved)==0);active=m;
    for(op=0;op<6;op++) {
        activeop=ops[op];entry=m->jit->entry[activeop];assert(entry);
        for(mode=0;mode<2;mode++)for(round=0;round<2;round++) {
            m->jit->entry[activeop]=mode?nil:entry;assert(fesetround(rounds[round])==0);
            for(i=0;i<nelem(bits);i++)for(j=0;j<nelem(bits);j++) {
                memcpy(&a,&bits[i],8);memcpy(&b,&bits[j],8);take=realrelation(op,a,b);
                expa=(bits[i]>>52)&2047;expb=(bits[j]>>52)&2047;
                valid=(expa!=2047 || (bits[i]&0xfffffffffffffULL)==0) &&
                    (expb!=2047 || (bits[j]&0xfffffffffffffULL)==0);
                memset(slots.bytes,0xa5,sizeof(slots.bytes));
                memcpy(slots.bytes+4,&bits[i],8);memcpy(slots.bytes+20,&bits[j],8);
                memcpy(slots.bytes+36,&target,sizeof(target));memcpy(before,slots.bytes,sizeof(before));
                R.s=slots.bytes+4;R.m=slots.bytes+20;R.d=slots.bytes+36;R.PC=pc;
                feclearexcept(FE_ALL_EXCEPT);feraiseexcept(round==0?FE_INEXACT:FE_ALL_EXCEPT);flags=fetestexcept(FE_ALL_EXCEPT);
                if(!mode) {
                    completed=entry(&R);assert(completed==valid);
                    assert(fetestexcept(FE_ALL_EXCEPT)==flags && fegetround()==rounds[round]);
                    if(!completed){assert(R.PC==pc);assert(kprotect(dispatch,nil));}
                }else assert(kprotect(dispatch,nil));
                assert(R.PC==(take?target:pc) && !memcmp(before,slots.bytes,sizeof(before)));
                assert(fegetround()==rounds[round]);
                if(valid)assert(fetestexcept(FE_ALL_EXCEPT)==flags);
                if(!mode && (!valid || !take)) {
                    R.PC=pc;R.d=(void*)1;
                    feclearexcept(FE_ALL_EXCEPT);feraiseexcept(round==0?FE_INEXACT:FE_ALL_EXCEPT);flags=fetestexcept(FE_ALL_EXCEPT);
                    assert(entry(&R)==valid && R.PC==pc && fetestexcept(FE_ALL_EXCEPT)==flags);
                }
            }
        }
        m->jit->entry[activeop]=entry;
    }
    assert(fesetenv(&saved)==0);
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
        assert(m->jit->entry[activeop]);
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
    Module m={0}, *owned;Inst instructions[78]={{0}};
    WORD a=3,b=7,out=0;
    int i;
    instructions[0].op=ISUBW;instructions[1].op=IADDW;
    instructions[2].op=IMOVP;instructions[3].op=IADDW;
    instructions[4].op=IDIVW;instructions[5].op=IMODW;
    instructions[6].op=ISHLW;instructions[7].op=ISHRW;instructions[8].op=ILSRW;
    instructions[9].op=ILENA;instructions[10].op=IINDB;instructions[11].op=IINDW;
    instructions[12].op=IINDL;instructions[13].op=IINDF;instructions[14].op=INEGF;instructions[15].op=IMOVM;instructions[16].op=ILENC;
    instructions[17].op=IHEADB;instructions[18].op=IHEADW;instructions[19].op=IHEADL;instructions[20].op=IHEADF;instructions[21].op=ILENL;instructions[22].op=IINDC;instructions[23].op=IEXPW;instructions[24].op=IEXPL;instructions[25].op=IEXPF;instructions[26].op=IHEADM;
    instructions[27].op=IBEQF;instructions[28].op=IBNEF;instructions[29].op=IBLTF;
    instructions[30].op=IBLEF;instructions[31].op=IBGTF;instructions[32].op=IBGEF;
    instructions[33].op=IMULX;instructions[34].op=IDIVX;instructions[35].op=IINDX;instructions[36].op=ICALL;instructions[37].op=IMULX0;instructions[38].op=IDIVX0;instructions[39].op=ICVTXX0;instructions[40].op=IMULX1;instructions[41].op=IDIVX1;instructions[42].op=ICVTXX1;instructions[43].op=IMOVPC;instructions[44].op=ITCMP;instructions[45].op=IMULB;instructions[46].op=IMULW;instructions[47].op=IMULL;instructions[48].op=IBEQC;instructions[49].op=IBNEC;instructions[50].op=IBLTC;instructions[51].op=IBLEC;instructions[52].op=IBGTC;instructions[53].op=IBGEC;
    instructions[54].op=IADDF;instructions[55].op=ISUBF;instructions[56].op=IMULF;instructions[57].op=IDIVF;instructions[58].op=ICVTXF;instructions[59].op=ICVTFW;instructions[60].op=ICVTFL;instructions[61].op=ICVTFX;
    instructions[62].op=ICVTWF;instructions[63].op=ICVTLF;instructions[64].op=ICVTRF;instructions[65].op=ICVTFR;instructions[66].op=IGOTO;instructions[67].op=ICASE;instructions[68].op=ICASEL;instructions[69].op=ISELF;instructions[70].op=IHEADP;instructions[71].op=ITAIL;instructions[72].op=IMOVMP;instructions[73].op=IHEADMP;instructions[74].op=ISLICELA;instructions[75].op=ICVTCW;instructions[76].op=ICVTCL;instructions[77].op=IINSC;
    m.prog=instructions;m.nprog=78;
    for(i=1;i<=5;i++) {
        fail=i;assert(!compile(&m,78,nil));assert(!m.jit && !m.compiled && m.prog==instructions && mappings==0);
    }
    fail=0;
    assert(!compile(nil,36,nil));assert(!compile(&m,3,nil));
    assert(compile(&m,78,nil));assert(m.jit && !m.compiled && m.prog==instructions);
    assert(m.jit->entry[ICALL] && m.jit->entry[ITCMP]);
    assert(m.jit->entry[IMOVPC]);
    assert(m.jit->entry[IBEQC] && m.jit->entry[IBNEC] && m.jit->entry[IBLTC] && m.jit->entry[IBLEC] && m.jit->entry[IBGTC] && m.jit->entry[IBGEC]);
    assert(m.jit->entry[IMULB] && m.jit->entry[IMULW] && m.jit->entry[IMULL]);
    assert(m.jit->entry[IMULX1] && m.jit->entry[IDIVX1] && m.jit->entry[ICVTXX1]);
    assert(m.jit->entry[IMULX0] && m.jit->entry[IDIVX0] && m.jit->entry[ICVTXX0]);
    assert(m.jit->entry[IMULX] && m.jit->entry[IDIVX]);
    for(i=69;i<78;i++)assert(!!m.jit->entry[instructions[i].op]==REAL_LEAVES);
    assert(!!m.jit->entry[IMOVP]==REAL_LEAVES);
    assert(m.jit->entry[IINDX]);
    assert(!!m.jit->entry[IEXPF]==REAL_LEAVES);
    for(i=62;i<66;i++)assert(m.jit->entry[instructions[i].op]);
    assert(!!m.jit->entry[IGOTO]==REAL_LEAVES);
    assert(!!m.jit->entry[ICASE]==REAL_LEAVES && !!m.jit->entry[ICASEL]==REAL_LEAVES);
    for(i=54;i<62;i++)assert(!!m.jit->entry[instructions[i].op]==REAL_LEAVES);
    assert(protections==1 && mappings==1);
    R.s=&a;R.m=&b;R.d=&out;
#if EXECUTE_NATIVE
    assert(m.jit->entry[ISUBW](&R)==1 && out==4);
    R.d=&a;assert(m.jit->entry[IADDW](&R)==1 && a==10);
    descriptorindex();fixedframe();realarithmetic();integerreal();shiftdivision();integeralu();fixedscale();scalartransfers();arithmetic(&m);arrays(&m);realnegation(&m);records(&m);stringlengths(&m);listheads(&m);listlengths(&m);characters(&m);powers(&m);headrecords(&m);realbranches(&m);
#endif
    assert(compile(&m,78,nil));dropmodule(&m);
    for(i=0;i<78;i++)instructions[i].op=ISEND;
    assert(!compile(&m,78,nil) && !m.jit);
    owned=calloc(1,sizeof(*owned));assert(owned);
    owned->prog=calloc(1,sizeof(Inst));assert(owned->prog);
    owned->nprog=1;owned->prog[0].op=IADDW;
    assert(compile(owned,1,nil) && mappings==1);
    freemod(owned);assert(mappings==0);
    puts("PASS BACKEND compiler: RX publication, fallback table, metadata/failure/retry and unmap");
    return 0;
}
'''
unit=('#define REAL_LEAVES '+str(int(backend!='riscv64'))+'\n')+unit
unit=unit.replace('comp-amd64.c','comp-'+backend+'.c').replace('EXECUTE_NATIVE','1' if backend=='amd64' else '0').replace('BACKEND',backend)
if backend=='riscv64':
    profile=os.environ.get('RV_FEATURE_PROFILE','m')
    if profile not in ('i','zmmul','m'):raise ValueError(profile)
    definitions='#define RV_CODE_PAGES\n'
    if profile!='i':definitions+='#define __riscv_mul 1\n'
    if profile=='m':definitions+='#define __riscv_div 1\n'
    unit=definitions+unit
    unit=unit.replace('assert(m.jit->entry[IBEQC] && m.jit->entry[IBNEC] && m.jit->entry[IBLTC] && m.jit->entry[IBLEC] && m.jit->entry[IBGTC] && m.jit->entry[IBGEC]);', 'assert(!m.jit->entry[IBEQC] && !m.jit->entry[IBNEC] && !m.jit->entry[IBLTC] && !m.jit->entry[IBLEC] && !m.jit->entry[IBGTC] && !m.jit->entry[IBGEC]);')
    unit=unit.replace('assert(m.jit->entry[IMULX1] && m.jit->entry[IDIVX1] && m.jit->entry[ICVTXX1]);',
        'assert(!!m.jit->entry[IMULX1]=='+str(int(profile=='m'))+');assert(!!m.jit->entry[IDIVX1]=='+str(int(profile=='m'))+');assert(!!m.jit->entry[ICVTXX1]=='+str(int(profile=='m'))+');')
    unit=unit.replace('assert(m.jit->entry[IMULX0] && m.jit->entry[IDIVX0] && m.jit->entry[ICVTXX0]);',
        'assert(!!m.jit->entry[IMULX0]=='+str(int(profile=='m'))+');assert(!!m.jit->entry[IDIVX0]=='+str(int(profile=='m'))+');assert(!!m.jit->entry[ICVTXX0]=='+str(int(profile=='m'))+');')
    unit=unit.replace('assert(m.jit->entry[IMULX] && m.jit->entry[IDIVX]);',
        'assert(!m.jit->entry[IMOVP]);assert(!!m.jit->entry[IMULX]=='+str(1)+');'
        'assert(!!m.jit->entry[IDIVX]=='+str(int(profile=='m'))+');'
        'assert(!!m.jit->entry[IDIVW]=='+str(int(profile=='m'))+');'
        'assert(!!m.jit->entry[IMODW]=='+str(int(profile=='m'))+');')
    unit=unit.replace('assert(m.jit->entry[IINDX]);', 'assert(!!m.jit->entry[IINDX]=='+str(int(profile!='i'))+');')

load=(root/'libinterp/load.c').read_text()
a=load.index('void\nfreemod(');b=load.index('\nvoid\nunload(',a)
unit=unit.replace('FREEMOD',load[a:b])
xec=Path(os.environ.get('LEAF_VM_SOURCE',str(root/'libinterp/xec.c'))).read_text()
parts=[]
for signature in ('static WORD\nfixedword(', 'static LONG\nfixedshift(', 'static LONG\nfixeddivide(', 'OP(mulx)', 'OP(divx)', 'OP(addf)', 'OP(subf)', 'OP(mulf)', 'OP(divf)', 'OP(negf)', 'OP(cvtrf)', 'OP(cvtfr)', 'OP(cvtxf)', 'OP(cvtwf)', 'OP(cvtlf)', 'static WORD\nshiftcount(', 'OP(shlb)', 'OP(shrb)', 'OP(shll)', 'OP(shrl)', 'OP(lsrl)', 'OP(divb)', 'OP(modb)', 'OP(divl)', 'OP(modl)', 'OP(addb)', 'OP(subb)', 'OP(mulb)', 'OP(andb)', 'OP(orb)', 'OP(xorb)', 'OP(addw)', 'OP(subw)', 'OP(mulw)', 'OP(andw)', 'OP(orw)', 'OP(xorw)', 'OP(addl)', 'OP(subl)', 'OP(mull)', 'OP(andl)', 'OP(orl)', 'OP(xorl)', 'OP(cvtxx)', 'OP(movb)', 'OP(movw)', 'OP(movl)', 'OP(movf)', 'OP(cvtbw)', 'OP(cvtwb)', 'OP(cvtws)', 'OP(cvtsw)', 'OP(cvtwl)', 'OP(cvtlw)', 'static List*\nlisthead(', 'OP(lenl)', 'OP(headb)', 'OP(headw)', 'OP(headl)', 'OP(headf)', 'OP(headm)', 'static void\nbranchreal(', 'OP(beqf)', 'OP(bnef)', 'OP(bltf)', 'OP(blef)', 'OP(bgtf)', 'OP(bgef)', 'OP(iexpw)', 'OP(iexpl)', 'OP(iexpf)', 'OP(movm)', 'OP(divw)', 'OP(modw)', 'OP(shlw)', 'OP(shrw)', 'OP(lsrw)', 'static uchar*\narrayaddress(', 'static void\narrayindex(', 'OP(indx)', 'OP(indb)', 'OP(indw)', 'OP(indl)', 'OP(indf)', 'OP(lena)'):
    if signature.startswith('static ') and signature not in xec:continue
    start=xec.index(signature);brace=xec.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',xec[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:
                parts.append(xec[start:brace+token.end()]);break
    else:raise ValueError(signature)
string=(root/'libinterp/string.c').read_text()
for op in ('lenc','indc'):
    a=string.index('OP('+op+')');b=string.index('\n}',a)+2
    parts.append(string[a:b])
scalar=(root/'libinterp/tests/scalar-transfers.c').read_text()
scalar=scalar[scalar.index('/* Construct independent'):]
scalar=scalar.replace('int\nmain(void)', 'static void\nscalartransfers(void)')
scalar=scalar.replace('\tunsigned long checks=0;', '\tunsigned long checks=0;\n\tModule module;\n\tInst code[10];\n\tint ids[] = {IMOVB,IMOVW,IMOVL,IMOVF,ICVTBW,ICVTWB,ICVTWS,ICVTSW,ICVTWL,ICVTLW};\n\tmemset(&module,0,sizeof(module));\n\tmemset(code,0,sizeof(code));\n\tfor(k=0;k<10;k++) code[k].op=ids[k];\n\tmodule.prog=code;module.nprog=10;\n\tassert(compile(&module,10,nil));')
scalar=scalar.replace('\t\tops[op]();', '\t\tassert(module.jit->entry[ids[op]](&R)==1);\n\t\tassert(!memcmp(memory.bytes,expected,sizeof(expected)));\n\t\t/* Restore the source so the production C path sees identical input. */\n\t\tfor(k=0;k<sizeof(memory.bytes);k++) memory.bytes[k]=(uchar)(k*37+11);\n\t\tmemmove(memory.bytes+s,&patterns[i],8);\n\t\tops[op]();')
scalar=scalar.replace('\treturn 0;', '\tdropmodule(&module);')
unit=unit.replace('SCALAR_TRANSFERS',scalar)
fixed=(root/'libinterp/tests/fixed-scale.c').read_text()
fixed=fixed[fixed.index('/* Exercise fixed intermediates'):]
fixed=fixed.replace('int\nmain(void)', 'static void\nfixedscale(void)').replace('\treturn 0;', '')
unit=unit.replace('FIXED_SCALE','#define NATIVE_FIXED 1\n'+fixed)
alu=(root/'libinterp/tests/integer-alu.c').read_text()
alu=alu[alu.index('/* Compare modular'):]
alu=alu.replace('int\nmain(void)', 'static void\nintegeralu(void)')
alu=alu.replace('\tunsigned long checks=0;', '\tunsigned long checks=0;\n\tModule module;\n\tInst code[18];\n\tint ids[]={IADDB,ISUBB,IMULB,IANDB,IORB,IXORB,IADDW,ISUBW,IMULW,IANDW,IORW,IXORW,IADDL,ISUBL,IMULL,IANDL,IORL,IXORL};\n\tmemset(&module,0,sizeof(module));memset(code,0,sizeof(code));\n\tfor(k=0;k<18;k++) code[k].op=ids[k];\n\tmodule.prog=code;module.nprog=18;assert(compile(&module,18,nil));')
alu=alu.replace('\t\tops[op]();', '\t\tassert(module.jit->entry[ids[op]](&R)==1);\n\t\tassert(!memcmp(memory.bytes,expected,sizeof(expected)));\n\t\tassert(R.PC==&sentinel && R.s==memory.bytes+s && R.m==memory.bytes+m && R.d==memory.bytes+d);\n\t\tfor(k=0;k<sizeof(memory.bytes);k++) memory.bytes[k]=(uchar)(k*37+11);\n\t\tmemmove(memory.bytes+s,&patterns[i],8);memmove(memory.bytes+m,&patterns[j],8);\n\t\tops[op]();')
alu=alu.replace('\treturn 0;', '\tdropmodule(&module);')
unit=unit.replace('INTEGER_ALU',alu)
shiftdiv=(root/'libinterp/tests/shift-div.c').read_text()
shiftdiv=shiftdiv[shiftdiv.index('/* Test every operand'):]
shiftdiv=shiftdiv.replace('int\nmain(int argc, char **argv)', 'static void\nshiftdivision(void)').replace('\treturn 0;', '')
unit=unit.replace('SHIFT_DIV','#define NATIVE_SHIFT_DIV 1\n#define SHIFT_DIV_FIRST 0\n'+shiftdiv)
integerreal=(root/'libinterp/tests/integer-real.c').read_text()
integerreal=integerreal[integerreal.index('/* Force the host'):]
integerreal=integerreal.replace('int\nmain(void)', 'static void\nintegerreal(void)').replace('\treturn 0;', '')
unit=unit.replace('INTEGER_REAL','#define NATIVE_INTEGER_REAL 1\n'+integerreal)
realarith=(root/'libinterp/tests/real-arithmetic.c').read_text()
realarith=realarith[realarith.index('/* Use volatile scalar'):]
realarith=realarith.replace('int\nmain(void)', 'static void\nrealarithmetic(void)').replace('\treturn 0;', '')
unit=unit.replace('REAL_ARITHMETIC','#define NATIVE_REAL_ARITH 1\n'+realarith)
framefixed=(root/'libinterp/tests/fixed-frame.c').read_text()
framefixed=framefixed[framefixed.index('/* Compare frame-byte'):]
framefixed=framefixed.replace('int\nmain(void)', 'static void\nfixedframe(void)').replace('\treturn 0;', '')
unit=unit.replace('FIXED_FRAME','#define NATIVE_FIXED_FRAME 1\n'+framefixed)
descriptor=(root/'libinterp/tests/descriptor-index.c').read_text()
descriptor=descriptor[descriptor.index('/* Check exact byte effects'):]
descriptor=descriptor.replace('int\nmain(void)', 'static void\ndescriptorindex(void)').replace('\treturn 0;', '')
unit=unit.replace('DESCRIPTOR_INDEX','#define NATIVE_DESCRIPTOR 1\n'+descriptor)
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
    subprocess.run(cmd+[str(source),'-lm','-o',str(td/'runtime')],check=True)
    subprocess.run([str(td/'runtime')],check=True)
