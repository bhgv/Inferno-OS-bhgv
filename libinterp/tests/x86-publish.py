#!/usr/bin/env python3
"""x86 publication/rollback with production driver, type/preamble and relocators.

Host scalars model IA-32. Integer emission and scheduler/OS are boundary stubs;
x86-integer-jit.py and x86-types.py execute native encoders in Unicorn.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'libinterp/comp-386.c').read_text()
def function(name,result):
    start=source.index(result+'\n'+name+'(');brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(name)
unit=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
static jmp_buf errorframe;
static int errdepth, throw_after=-1, events;
#define waserror() (errdepth++, setjmp(errorframe))
#define poperror() (errdepth--)
static void event(void) {
 events++;
 if(throw_after==0){errdepth--;longjmp(errorframe,1);}
 if(throw_after>0)throw_after--;
}
RUNTIMEGUARD

#define ulong uint32_t
#define nil NULL
#define H ((void*)(uintptr_t)0xffffffff)
#define nelem(a) (sizeof(a)/sizeof((a)[0]))
#include "isa.h"
typedef int32_t WORD;
typedef uint8_t uchar;
typedef uintptr_t uintptr;
typedef uint64_t uvlong;
typedef struct Inst Inst;
typedef struct Type Type;
typedef struct Module Module;
typedef struct Link Link;
typedef struct Modl Modl;
typedef union Adr {int32_t ind,imm;Inst *ins;} Adr;
struct Inst {uint8_t op,add;Adr s,d;};
struct Type {int size,np;void *initialize,*destroy;};
struct Link {char *name;union {Inst *pc;} u;Type *frame;};
struct Modl {union {Inst *pc;} u;Type *frame;};
typedef struct Modlink {int nlinks;Modl links[2];} Modlink;
typedef struct Except {char *s;int pc;} Except;
typedef struct Handler {int pc1,pc2;Except *etab;} Handler;
struct Module {Inst *prog,*entry;int compiled,nprog,ntype;Type **type;Link *ext;char *name;uint8_t *origmp;Handler *htab;};
int __lsan_is_turned_off(void) {return 1;}
static int live,fail_after=-1,flushes,fail_flush,phase_error;
static void *allocations[256];
static int frees;
static void *testmalloc(size_t n) {
    void *p;int i;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    if(n==0)return nil;
    p=malloc(n);if(!p)return nil;memset(p,0xa5,n);
    for(i=0;i<256;i++)if(!allocations[i]){allocations[i]=p;live++;return p;}
    assert(0);return nil;
}
static void *testmallocz(size_t n,int zero) {void *p=testmalloc(n);if(p && zero)memset(p,0,n);return p;}
static void testfree(void *p) {
    int i;if(!p)return;
    for(i=0;i<256;i++)if(allocations[i]==p){allocations[i]=nil;live--;frees++;free(p);return;}
    assert(0);
}
#define malloc testmalloc
#define mallocz testmallocz
#define free testfree
static uchar *x86helpertemp,*x86helpercode;
static uchar *code,*base;
static ulong *patch,*litpool;
static uint8_t *tinit;
static Module *mod;
static uchar *x86frp, *x86ret;
static int pass,nlit,cflag;
static int use_literals,literal_phase;
static ulong macro[1];
void (*comvec)(void);
#define RELPC(pc) ((ulong)(uintptr_t)(base+(pc)))
#define O(t,f) 0
#define REG int
#define RAX 0
#define RBX 3
#define RCX 1
#define RDX 2
#define RSI 6
#define RDI 7
#define RFP RSI
#define RMP RDI
#define RTMP RBX
#define Olea 0
#define Oldw 1
#define Ojmprm 2
#define Opushl 0x50
#define MacCASE 0
#define Ojmp 0xe9
static int R;
static void genb(uchar v) {event();*code++=v;}
static void gen2(uchar a,uchar b) {genb(a);genb(b);}
static void con(ulong x,int r) {event();(void)x;(void)r;*code++=1;}
static void modrm(int op,ulong off,int a,int b) {event();(void)op;(void)off;(void)a;(void)b;*code++=2;}
static void opwld(Inst *i,int op,int reg) {event();(void)i;(void)op;(void)reg;*code++=3;}
static void opwst(Inst *i,int op,int reg) {event();(void)i;(void)op;(void)reg;*code++=4;}
static void rbra(ulong n,int op) {event();(void)n;(void)op;*code++=5;}
static void macgen(void) {event();*code++=7;}
static struct {int idx;void (*gen)(void);} mactab[]={{0,macgen}};
static void print(char *s,...) {event();(void)s;}
static void das(uchar *p,int n) {event();(void)p;(void)n;}
static Module *active;
static Inst *original;
static int segflush(void *p,ulong n) { event();
    assert(p && n);flushes++;
    if(p==base)assert(n==(((code-base+3)&~3)+nlit*sizeof(ulong)));
    assert(active && !active->compiled && active->prog==original);
    assert(active->ext[0].u.pc==original+1);
    if(active->origmp!=H)
        assert(((WORD*)active->origmp)[0]==1 && ((WORD*)active->origmp)[8]==2 && ((WORD*)active->origmp)[16]==1);
    return flushes==fail_flush ? -1 : 0;
}
static void comi(Type *t) { event();(void)t;*code++=8;}
static void comd(Type *t) { event();(void)t;*code++=9;}
RELOCATORS
static void comp(Inst *i) { event();
    if(use_literals && !(literal_phase && pass)) {
        nlit++;
        if(pass)*litpool++=0x12345678;
    }
    if(i->op==ICASE)comcase(i,0);
    if(i->op==ICASEL)comcasel(i);
    if(i->op==IGOTO)comgoto(i);
    if(mod->ntype>1)tinit[1]=1;
    if(!(phase_error && pass && i==mod->prog))*code++=10;
}
static void macfrp(void) {event();*code++=11;}
static void retbody(void) {event();*code++=12;}
PREAMBLE
TYPECOM
PATCHEX
#include "jit-validate.h"
COMPILE
static Module m;
static Modlink ml;
static Type data,a,b,old,*types[2];
static Link links[3];
static WORD mp[32],before[32];
static Handler handlers[2];
static Except exceptions[2];
static void fixture(void) {
    int i;
    memset(&m,0,sizeof(m));memset(&ml,0,sizeof(ml));
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&old,0,sizeof(old));
    memset(&data,0,sizeof(data));memset(links,0,sizeof(links));memset(mp,0,sizeof(mp));
    original=malloc(4*sizeof(Inst));assert(original);memset(original,0,4*sizeof(Inst));
    original[0].op=ICASE;original[1].op=ICASE;
    original[2].op=IGOTO;original[2].d.ind=9*4;
    original[3].op=ICASEL;original[3].d.ind=16*4;
    mp[0]=1;mp[1]=0;mp[2]=10;mp[3]=1;mp[4]=2;
    mp[8]=2;mp[9]=1;mp[10]=3;
    mp[16]=1;mp[22]=0;mp[24]=2;
    memcpy(before,mp,sizeof(mp));
    data.size=sizeof(mp);types[0]=&data;types[1]=&b;
    old.initialize=malloc(8);assert(old.initialize);old.destroy=(char*)old.initialize+4;
    links[0].name="a";links[0].u.pc=original+1;links[0].frame=&a;
    links[1].name="old";links[1].u.pc=original+2;links[1].frame=&old;
    ml.nlinks=2;ml.links[0].u.pc=original;ml.links[0].frame=&a;
    ml.links[1].u.pc=original+3;ml.links[1].frame=&b;
    exceptions[0].s="error";exceptions[0].pc=1;exceptions[1].s=nil;exceptions[1].pc=3;
    memset(handlers,0,sizeof(handlers));handlers[0].pc1=1;handlers[0].pc2=4;handlers[0].etab=exceptions;
    m.prog=original;m.entry=original+3;m.nprog=4;m.ntype=2;m.type=types;
    m.origmp=(uint8_t*)mp;m.ext=links;m.htab=handlers;m.name="fixture";
    active=&m;flushes=0;
    for(i=0;i<1;i++)macro[i]=0;
}
static void unchanged(void) {
    assert(!m.compiled && m.prog==original && m.entry==original+3);
    assert(memcmp(mp,before,sizeof(mp))==0 && links[0].u.pc==original+1);
    assert(ml.links[1].u.pc==original+3 && !a.initialize && !a.destroy && !b.initialize && !b.destroy);
    assert(old.initialize && old.destroy==(char*)old.initialize+4);
    assert(handlers[0].pc1==1 && handlers[0].pc2==4 && exceptions[0].pc==1);
    assert(live==2+(comvec!=nil));
    assert((comvec!=nil)==(x86frp!=nil));assert((comvec!=nil)==(x86ret!=nil));
}
static void cleanup(void) {
    free(m.prog);free(a.initialize);free(b.initialize);free(old.initialize);free((void*)comvec);comvec=nil;x86frp=nil;x86ret=nil;
    assert(live==0);
}
int main(void) {
    int i,failures=0,successes=0, count;
    fixture();cflag=5;events=0;assert(compile(&m,4,&ml));count=events;cleanup();
    for(i=0;i<count;i++) {
        fixture();cflag=5;throw_after=i;
        assert(!compile(&m,4,&ml));assert(errdepth==0);unchanged();
        throw_after=-1;assert(compile(&m,4,&ml));cleanup();
    }
    cflag=0;
    for(i=0;i<25;i++) {
        fixture();fail_after=i;
        if(compile(&m,4,&ml))successes++;
        else {failures++;unchanged();}
        fail_after=-1;cleanup();
    }
    assert(failures && successes);
    for(i=1;i<=4;i++) {
        fixture();fail_flush=i;
        assert(!compile(&m,4,&ml));unchanged();
        fail_flush=0;flushes=0;assert(compile(&m,4,&ml));
        assert(m.compiled && m.prog!=original && a.initialize && b.initialize);
        assert(mp[0]==1 && (uint32_t)mp[3]==(uint32_t)(uintptr)links[0].u.pc && (uint32_t)mp[4]==(uint32_t)(uintptr)links[1].u.pc);
        assert(mp[8]==0 && (uint32_t)mp[9]==(uint32_t)(uintptr)links[0].u.pc && (uint32_t)mp[10]==(uint32_t)(uintptr)ml.links[1].u.pc);
        assert(mp[16]==1 && (uint32_t)mp[22]==(uint32_t)(uintptr)m.prog && (uint32_t)mp[24]==(uint32_t)(uintptr)links[1].u.pc);
        assert(handlers[0].pc1==(char*)links[0].u.pc-(char*)m.prog && handlers[0].pc2>(char*)ml.links[1].u.pc-(char*)m.prog);
        cleanup();
    }
    fixture();m.ntype=0;m.type=nil;m.origmp=H;ml.nlinks=0;
    links[0].frame=nil;links[1].frame=nil;
    for(i=0;i<4;i++)original[i].op=IRUNT;
    assert(compile(&m,4,&ml));cleanup();
    fixture();phase_error=1;assert(!compile(&m,4,&ml));phase_error=0;unchanged();cleanup();
    fixture();assert(!compile(&m,3,&ml));unchanged();cleanup();
    fixture();m.entry=original+4;assert(!compile(&m,4,&ml));m.entry=original+3;unchanged();cleanup();
    fixture();links[0].u.pc=(Inst*)((char*)original+1);assert(!compile(&m,4,&ml));links[0].u.pc=original+1;unchanged();cleanup();
    fixture();handlers[0].pc2=5;assert(!compile(&m,4,&ml));handlers[0].pc2=4;unchanged();cleanup();
    fixture();exceptions[0].pc=-2;assert(!compile(&m,4,&ml));exceptions[0].pc=1;unchanged();cleanup();
    {
        int slots[]={0,0,3,4,8,8,9,16,16,22,24};
        WORD bad[]={-1,0x7fffffff,-1,4,-1,0x7fffffff,4,-1,0x7fffffff,4,-1};
        WORD saved;
        for(i=0;i<11;i++) {
            fixture();saved=mp[slots[i]];mp[slots[i]]=bad[i];
            assert(!compile(&m,4,&ml));mp[slots[i]]=saved;unchanged();cleanup();
        }
        for(i=0;i<3;i++) {
            fixture();original[0].d.ind=i==0?-4:i==1?2:124;
            assert(!compile(&m,4,&ml));original[0].d.ind=0;unchanged();cleanup();
        }
        fixture();original[2].d.ind=4; /* Valid goto inside the case table: reject overlap. */
        assert(!compile(&m,4,&ml));original[2].d.ind=9*4;unchanged();cleanup();
        fixture();original[0].op=IFRAME;original[0].add=SRC(AIMM);original[0].s.imm=-1;
        assert(!compile(&m,4,&ml));unchanged();cleanup();
        fixture();original[0].op=IFRAME;original[0].add=SRC(AIMM);original[0].s.imm=2;
        assert(!compile(&m,4,&ml));unchanged();cleanup();
        fixture();original[0].op=IJMP;original[0].d.ins=original+4;
        assert(!compile(&m,4,&ml));unchanged();cleanup();
        fixture();original[0].op=IJMP;original[0].d.ins=(Inst*)((char*)original+1);
        assert(!compile(&m,4,&ml));unchanged();cleanup();
        fixture();original[0].add=DST(AFP);
        assert(!compile(&m,4,&ml));unchanged();cleanup();
        fixture();m.origmp=nil;assert(!compile(&m,4,&ml));m.origmp=(uchar*)mp;unchanged();cleanup();
    }
    {
        unsigned char *raw=(unsigned char*)mp;
        unsigned char value;
        unsigned int byte,mutation;
        fixture();
        for(byte=0;byte<sizeof(mp);byte++)for(mutation=0;mutation<256;mutation++) {
            value=raw[byte];raw[byte]=mutation;
            (void)jitvalidprogram(&m,4);
            assert(raw[byte]==mutation);raw[byte]=value;
            assert(!memcmp(mp,before,sizeof(mp)));
        }
        for(i=0;i<=128;i++){data.size=i;(void)jitvalidprogram(&m,4);}
        data.size=sizeof(mp);unchanged();cleanup();
    }
    fixture();use_literals=1;assert(compile(&m,4,&ml));
    {int savedlive=live;Inst *published=m.prog;
     assert(!compile(&m,4,&ml) && m.prog==published && live==savedlive);}
    use_literals=0;cleanup();
    fixture();use_literals=1;literal_phase=1;assert(!compile(&m,4,&ml));
    use_literals=literal_phase=0;unchanged();cleanup();
    printf("PASS %d VM error injection points; 32768 table mutations and 129 data extents\n",count);
    puts("PASS x86 publication: VM error unwinds, allocation/cache failures, table/Type rollback, retry, aliases, shared helpers, phase mismatch and end-PC exception ranges");
    return 0;
}
'''
runtime=(root/'emu/port/dis.c').read_text()
guard=runtime[runtime.index('int\nkprotect('):runtime.index('\nvoid\nkrethrow(')]
native=(root/'os/port/dis.c').read_text()
assert guard == native[native.index('int\nkprotect('):native.index('\nvoid\nkrethrow(')]
unit=unit.replace('RUNTIMEGUARD',guard)
relocators='\n'.join(function(n,'static void') for n in ['comcase','comcasel','comgoto'])
unit=unit.replace('RELOCATORS',relocators).replace('PREAMBLE',function('preamble','static int'))
unit=unit.replace('TYPECOM',function('typecom','void')).replace('PATCHEX',function('patchex','static void'))
unit=unit.replace('COMPILE',source[source.index('/* Roll back only helpers'):])
with tempfile.TemporaryDirectory(prefix='x86-publish-') as directory:
    td=Path(directory);path=td/'publish.c';path.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    command+=['-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-I'+str(root/'include'),'-I'+str(root/'libinterp'),str(path),'-o',str(td/'publish')]
    subprocess.run(command,check=True)
    environment=dict(os.environ);environment.setdefault('UBSAN_OPTIONS','halt_on_error=1')
    subprocess.run([str(td/'publish')],check=True,env=environment)
