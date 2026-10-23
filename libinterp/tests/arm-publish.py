#!/usr/bin/env python3
"""ARM publication/rollback with production driver, type/preamble and relocators.

Host scalars model ARM32. Integer emission and scheduler/OS are boundary stubs;
arm-vfp.py separately executes the production VFP encoder in Unicorn.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=Path(os.environ.get('ARM_PUBLISH_SOURCE', root/'libinterp/comp-arm.c')).read_text()
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
typedef struct Inst Inst;
typedef struct Type Type;
typedef struct Module Module;
typedef struct Link Link;
typedef struct Modl Modl;
typedef union Adr {int32_t ind;Inst *ins;} Adr;
struct Inst {uint8_t op;Adr d;};
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
static size_t allocationbytes[256];
static int frees;
static void *testmalloc(size_t n) {
    void *p;int i;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    if(n==0)return nil;
    p=calloc(1,n);if(!p)return nil;
    for(i=0;i<256;i++)if(!allocations[i]){allocations[i]=p;allocationbytes[i]=n;live++;return p;}
    assert(0);return nil;
}
static void *testmallocz(size_t n,int zero) {(void)zero;return testmalloc(n);}
static void testfree(void *p) {
    int i;if(!p)return;
    for(i=0;i<256;i++)if(allocations[i]==p){allocations[i]=nil;live--;frees++;free(p);return;}
    assert(0);
}
#define malloc testmalloc
#define mallocz testmallocz
#define free testfree
static ulong *armhelpertemp,*armhelpercode;
static ulong *code,*base,*patch,*litpool;
static uint8_t *tinit;
static Module *mod;
static int pass,nlit,codeoff,cflag;
static ulong macro[1];
static struct {int ptr;} rcon;
void (*comvec)(void);
#define RELPC(pc) ((ulong)(uintptr_t)(base+(pc)))
#define RREG 0
#define RLINK 1
#define RFP 2
#define RMP 3
#define R15 15
#define REG int
#define O(t,f) 0
#define Stw 0
#define Ldw 1
#define Lea 2
#define AL 14
#define RA0 0
#define RA1 1
#define RA3 3
#define MacCASE 0
static int R;
static void con(ulong x,int r,int opt) { event();(void)x;(void)r;(void)opt;*code++=1;}
static void mem(int op,int off,int a,int b) { event();(void)op;(void)off;(void)a;(void)b;*code++=2;}
static void flushcon(int x) { event();(void)x;}
static void opwld(Inst *i,int op,int reg) { event();(void)i;(void)op;(void)reg;*code++=3;}
static void opwst(Inst *i,int op,int reg) { event();(void)i;(void)op;(void)reg;*code++=4;}
#define BRAMAC(...) (*code++=5)
#define LDRW(...) (*code++=6)
static void macgen(void) { event();*code++=7;}
static struct {int idx;void (*gen)(void);char *name;} mactab[]={{0,macgen,"macro"}};
static void print(char *s,...) { event();(void)s;}
static void das(ulong *p,int n) { event();(void)p;(void)n;}
static Module *active;
static Inst *original;
static int segflush(void *p,ulong n) { event();
    assert(p && n);flushes++;
    int slot;
    for(slot=0;slot<256 && allocations[slot]!=p;slot++);
    assert(slot<256 && n<=allocationbytes[slot]);
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
    if(i->op==ICASE)comcase(i,0);
    if(i->op==ICASEL)comcasel(i);
    if(i->op==IGOTO)comgoto(i);
    if(mod->ntype>1)tinit[1]=1;
    if(!(phase_error && pass && i==mod->prog))*code++=10;
}
PREAMBLE
TYPECOM
PATCHEX
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
    original=malloc(4*sizeof(Inst));assert(original);
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
}
static void cleanup(void) {
    free(m.prog);free(a.initialize);free(b.initialize);free(old.initialize);free((void*)comvec);comvec=nil;
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
        assert(mp[0]==1 && mp[3]==RELPC(1) && mp[4]==RELPC(2));
        assert(mp[8]==0 && mp[9]==RELPC(1) && mp[10]==RELPC(6));
        assert(mp[16]==1 && mp[22]==RELPC(0) && mp[24]==RELPC(2));
        assert(handlers[0].pc1==1 && handlers[0].pc2==7);
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
    /* Invalid metadata must fail before any helper or PC is published. */
    for(i=0;i<14;i++) {
        fixture();
        if(i==0)types[1]=H;
        else if(i==1)links[0].frame=H;
        else if(i==2)ml.links[1].frame=H;
        else if(i==3)b.size=-1;
        else if(i==4)b.np=-1;
        else if(i==5)m.origmp=nil;
        else if(i==6)a.np=-1;
        else if(i==7)old.size=-1;
        else if(i==10)m.prog=H;
        else if(i==11)m.ext=H;
        else if(i==12)m.type=H;
        else if(i==13)m.htab=H;
        if(i==8)assert(!compile((Module*)H,4,&ml));
        else if(i==9)assert(!compile(&m,4,(Modlink*)H));
        else assert(!compile(&m,4,&ml));
        types[1]=&b;links[0].frame=&a;ml.links[1].frame=&b;
        b.size=b.np=a.np=old.size=0;m.origmp=(uint8_t*)mp;
        m.prog=original;m.ext=links;m.type=types;m.htab=handlers;
        unchanged();cleanup();
    }
    printf("PASS %d VM error injection points\n",count);
    puts("PASS ARM publication: VM error unwinds, allocation/cache failures, table/Type rollback, retry, aliases, shared helpers, phase mismatch and end-PC exception ranges");
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
with tempfile.TemporaryDirectory(prefix='arm-publish-') as directory:
    td=Path(directory);path=td/'publish.c';path.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g'))
    command+=['-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-I'+str(root/'include'),str(path),'-o',str(td/'publish')]
    subprocess.run(command,check=True)
    environment=dict(os.environ);environment.setdefault('UBSAN_OPTIONS','halt_on_error=1')
    subprocess.run([str(td/'publish')],check=True,env=environment)
