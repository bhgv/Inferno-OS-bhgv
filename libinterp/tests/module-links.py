#!/usr/bin/env python3
"""Production module lookup, VM mcall/mframe and linker checks with Proc unwinds.

Host layout and target WORD32/64 operand models; allocation/runtime-module
bodies are recording stubs. No native target ABI or loader boot is executed.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
def function(file,signature):
 s=(root/file).read_text();a=s.index(signature);brace=s.index('{',a);depth=0
 for t in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',s[brace:]):
  if t.group()=='{':depth+=1
  elif t.group()=='}':
   depth-=1
   if not depth:return s[a:brace+t.end()]
 raise ValueError(signature)
unit=r'''
#include "dat.h"
#include "fns.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include "kernel.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#if VM_WORD_BITS == 32
#define WORD int32_t
#define UWORD uint32_t
#else
#define WORD int64_t
#define UWORD uint64_t
#endif
#define OP(n) void n(void)
#define W(r) (*(WORD*)R.r)
#define T(r) (*(void**)R.r)
static Proc worker;
static char *last_error;
Proc *getup(void){return &worker;}
void nexterror(void){assert(worker.nerr>0);siglongjmp(worker.estack[--worker.nerr],1);}
void error(char *s){last_error=s;nexterror();}
char exModule[]="module",exSpawn[]="spawn";
REG R;
static Prog running;
Prog *currun(void){return &running;}
static int extended,initialized,freed,called,children,stacks,unframes,releasechild,destroyed;
static Modlink *owned;
static Prog child;
Prog *newprog(Prog *parent,Modlink *m){assert(parent==currun());memset(&child,0,sizeof child);child.R.M=m;children++;return &child;}
void newstack(Prog *p){assert(p==&child);stacks++;}
void unframe(void){unframes++;}
static Frame *expected_frame;
void extend(void){extended++;R.s=expected_frame;}
void initmem(Type *t,void *p){(void)t;(void)p;initialized++;}
void freeptrs(void *p,Type *t){(void)t;assert(p==expected_frame);freed++;if(releasechild){assert(D2H(owned)->ref>1);D2H(owned)->ref--;}}
void destroy(void *p){assert(p==owned);if(releasechild)assert(freed==1);assert(D2H(owned)->ref>0);if(--D2H(owned)->ref==0)destroyed++;}
void unextend(Frame *f){assert(f==expected_frame);freed++;}
static void runtimecall(void *f){assert(f==expected_frame);called++;}
static char link_error[256];
void kwerrstr(char *fmt,...){va_list ap;va_start(ap,fmt);vsnprintf(link_error,sizeof link_error,fmt,ap);va_end(ap);}
int print(char *fmt,...){(void)fmt;return 0;}
int snprint(char *out,int n,char *fmt,...){int r;va_list ap;va_start(ap,fmt);r=vsnprintf(out,n,fmt,ap);va_end(ap);return r;}
GUARD
LOOKUP
MFRAME
MCALL
MSPAWN
RET
LINKM
DESTROYLINKS
static union {Heap align;uchar bytes[sizeof(Heap)+sizeof(Modlink)+2*sizeof(Modl)];} allocation;
static Module module;
static Modlink caller,*ml,*operand;
static Link exports[3];
static Type types[2];
static Inst code[8];
static Frame frame,*fp,*destination;
static uchar vmstack[256],data[32];
static WORD indexvalue;
static int operation;
static unsigned long checks;
static void setup(WORD index){
 memset(&R,0,sizeof R);memset(&frame,0xa5,sizeof frame);memset(vmstack,0xa5,sizeof vmstack);
 memset(&allocation,0,sizeof allocation);memset(&module,0,sizeof module);memset(&caller,0,sizeof caller);memset(exports,0,sizeof exports);memset(types,0,sizeof types);
 ml=(Modlink*)(allocation.bytes+sizeof(Heap));D2H(ml)->ref=7;ml->m=&module;ml->nlinks=2;ml->compiled=1;ml->prog=code;ml->MP=data;
 module.ext=exports;module.name="target";operand=ml;
 for(int j=0;j<2;j++){types[j].size=sizeof(Frame)+16*j;types[j].np=j;exports[j].name=j?"second":"first";exports[j].sig=100+j;exports[j].u.pc=&code[j+2];exports[j].frame=&types[j];ml->links[j].u=exports[j].u;ml->links[j].frame=&types[j];}
 fp=&frame;destination=(Frame*)H;expected_frame=&frame;indexvalue=index;
 R.PC=&code[1];R.FP=vmstack+128;R.SP=vmstack;R.TS=vmstack+sizeof vmstack;R.M=&caller;R.MP=vmstack;R.m=&indexvalue;R.t=37;R.IC=99;caller.compiled=1;
 if(operation!=1){R.s=&fp;R.d=&operand;}else{R.s=&operand;R.d=&destination;}
 frame.t=&types[0];running.kill=nil;last_error=nil;extended=initialized=freed=called=children=stacks=unframes=releasechild=destroyed=0;owned=ml;
}
static int invoke(void *arg){(void)arg;if(operation==0)mcall();else if(operation==1)mframe();else if(operation==2)mspawn();else ret();return 1;}
static void expect_error(char *message){
 REG saved=R;Frame before=frame;ulong refs=D2H(ml)->ref;uchar bytes[sizeof vmstack];memcpy(bytes,vmstack,sizeof vmstack);
 assert(!kprotect(invoke,nil));assert(strcmp(last_error,message)==0);
 assert(memcmp(&saved,&R,sizeof R)==0&&memcmp(&before,&frame,sizeof frame)==0);
 assert(D2H(ml)->ref==refs&&memcmp(bytes,vmstack,sizeof vmstack)==0&&destination==(Frame*)H);
 assert(!extended&&!initialized&&!called&&!freed&&!children&&!stacks&&!unframes&&worker.nerr==0);checks++;
}
int main(void){
 for(operation=0;operation<3;operation++){
  for(int n=-32768;n<=32767;n++){
   if(n==0||n==1||n==-1||n==-2)continue;
   setup(n);expect_error(operation==1?"invalid mframe":operation==2?"invalid mspawn":"invalid mcall");
  }
  WORD extremes[]={ (WORD)((UWORD)1<<(VM_WORD_BITS-1)),(WORD)(((UWORD)1<<(VM_WORD_BITS-1))-1),VM_WORD_BITS==64?(WORD)0x100000000LL:32768,VM_WORD_BITS==64?(WORD)-0x100000001LL:-32769};
  for(unsigned j=0;j<sizeof extremes/sizeof extremes[0];j++){setup(extremes[j]);expect_error(operation==1?"invalid mframe":operation==2?"invalid mspawn":"invalid mcall");}
  for(int n=-2;n<=1;n++){
   int slot=n<0?-n-1:n;setup(n);assert(kprotect(invoke,nil));
   if(operation==0){assert(R.PC==&code[slot+2]&&R.FP==(uchar*)&frame&&R.M==ml&&R.MP==data&&R.t==1);assert(frame.lr==&code[1]&&frame.fp==vmstack+128&&frame.mr==&caller&&D2H(ml)->ref==8);}
   else if(operation==1){assert(destination==(Frame*)vmstack&&R.SP==vmstack+types[slot].size);assert(destination->t==&types[slot]&&destination->mr==nil&&initialized==slot);}
   else {assert(child.R.PC==&code[slot+2]&&children==1&&stacks==1&&unframes==1&&R.FP==vmstack+128&&D2H(ml)->ref==7);}
   checks++;
  }
  setup(0);operand=(Modlink*)H;expect_error(exModule);
  setup(-1);module.ext=nil;expect_error(operation==1?"invalid mframe":operation==2?"invalid mspawn":"invalid mcall");
  setup(-1);ml->m=nil;expect_error(operation==1?"invalid mframe":operation==2?"invalid mspawn":"invalid mcall");
  setup(0);ml->nlinks=-1;expect_error(operation==1?"invalid mframe":operation==2?"invalid mspawn":"invalid mcall");
  for(int n=0;n<2;n++){
   setup(n);if(operation==1)ml->links[n].frame=nil;else ml->links[n].u.pc=nil;expect_error(operation==1?"invalid mframe":operation==2?"invalid mspawn":"invalid mcall");
   setup(-n-1);if(operation==1)exports[n].frame=(Type*)H;else exports[n].u.pc=(Inst*)H;expect_error(operation==1?"invalid mframe":operation==2?"invalid mspawn":"invalid mcall");
  }
 }
 operation=0;
 for(int n=-2;n<=1;n++){
  setup(n);ml->prog=nil;frame.t=&types[1];
  for(int j=0;j<2;j++){ml->links[j].u.runt=runtimecall;exports[j].u.runt=runtimecall;}
  assert(kprotect(invoke,nil));assert(called==1&&freed==1&&D2H(ml)->ref==7&&R.M==&caller&&R.FP==vmstack+128&&R.t==0);checks++;
 }
 operation=2;setup(0);ml->prog=nil;expect_error(exSpawn);
 operation=1;setup(-1);R.TS=R.SP+types[0].size;assert(kprotect(invoke,nil));assert(extended==1&&destination==expected_frame);checks++;
 operation=3;
 for(int compiled=0;compiled<2;compiled++){
  setup(0);R.FP=(uchar*)&frame;R.M=ml;R.t=0;frame.fp=vmstack+128;frame.lr=&code[3];frame.mr=&caller;frame.t=&types[1];
  caller.compiled=compiled;caller.MP=vmstack+64;D2H(ml)->ref=2;releasechild=1;
  assert(kprotect(invoke,nil));assert(freed==1&&destroyed==1&&D2H(ml)->ref==0);
  assert(R.M==&caller&&R.MP==vmstack+64&&R.PC==&code[3]&&R.FP==vmstack+128&&R.SP==(uchar*)&frame);
  assert(R.t==(compiled?0:1));checks++;
 }
 setup(0);R.FP=(uchar*)&frame;frame.fp=nil;expect_error("");
 Import imp={0,"missing"};
 for(int sig=0;sig<3;sig++){
  setup(0);imp.sig=sig;Modl before=ml->links[0];assert(linkm(&module,ml,0,&imp)==-1);assert(memcmp(&before,&ml->links[0],sizeof before)==0);assert(strstr(link_error,"not implemented"));checks++;
 }
 setup(0);module.ext=nil;assert(linkm(&module,ml,0,&imp)==-1);checks++;
 setup(0);imp.name="first";imp.sig=999;assert(linkm(&module,ml,0,&imp)==-1);checks++;
 setup(0);imp.sig=100;assert(linkm(&module,ml,1,&imp)==0&&ml->links[1].u.pc==exports[0].u.pc&&ml->links[1].frame==exports[0].frame);checks++;
 setup(0);module.ext=nil;destroylinks(&module);assert(module.ext==nil);checks++;
 module.ext=calloc(3,sizeof(Link));assert(module.ext);
 module.ext[0].name=strdup("one");module.ext[1].name=strdup("two");assert(module.ext[0].name&&module.ext[1].name);
 destroylinks(&module);assert(module.ext==nil);checks++;
 destroylinks(&module);assert(module.ext==nil);checks++;
 printf("PASS module links WORD%d: %lu lookup/VM state/ownership/linker cases with Proc unwinds\n",VM_WORD_BITS,checks);
 return 0;
}
'''
for label,file,sig in [('GUARD','emu/port/dis.c','int\nkprotect('),('LOOKUP','libinterp/xec.c','static Linkpc*\nmodulelink('),('MFRAME','libinterp/xec.c','OP(mframe)'),('MCALL','libinterp/xec.c','OP(mcall)'),('MSPAWN','libinterp/xec.c','OP(mspawn)'),('RET','libinterp/xec.c','OP(ret)'),('LINKM','libinterp/link.c','static int\nlinkm('),('DESTROYLINKS','libinterp/link.c','void\ndestroylinks(')]:unit=unit.replace(label,function(file,sig))
with tempfile.TemporaryDirectory(prefix='module-links-') as td:
 td=Path(td);(td/'links.c').write_text(unit)
 for bits in [32,64]:
  binary=td/f'links{bits}'
  command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+[f'-DVM_WORD_BITS={bits}']+['-I'+str(root/p) for p in ['Linux/arm/include','emu/port','include','libinterp']]
  subprocess.run(command+[str(td/'links.c'),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
