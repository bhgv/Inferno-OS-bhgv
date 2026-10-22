#!/usr/bin/env python3
"""Check actual JIT module-op selectors for every signed 16-bit immediate.

Native compiler and punt calls are recording stubs. jit-handoff.py separately
executes the production NEWPC/TCHECK bridge; this is not module runtime/ABI code.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
def cases(arch):
 s=(root/f'libinterp/comp-{arch}.c').read_text();out=[]
 for op in ['IMCALL','IMFRAME']:
  a=s.index('\tcase '+op+':',s.index('comp(Inst *i)'));b=s.index('\t\tbreak;',a)+len('\t\tbreak;');out.append(s[a:b])
 return '\n'.join(out)
unit=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "isa.h"
typedef struct Inst {int op,add;uint16_t reg;} Inst;
enum {SRCOP=1,DSTOP=2,WRTPC=4,TCHECK=8,NEWPC=16,DBRAN=32,THREOP=64};
static int fast,mask;
static void (*optab[256])(void);
static void commcall(Inst *i){(void)i;fast=1;}
static void commframe(Inst *i){(void)i;fast=2;}
static void punt(Inst *i,int m,void(*fn)(void)){assert(fn==optab[i->op]);mask=m;}
static void route(int arch,Inst *i){
 fast=mask=0;
 if(arch==0){switch(i->op){X86 default:assert(0);}}
 else {switch(i->op){ARM32 default:assert(0);}}
}
int main(void){
 unsigned reg;int arch,op,mode;unsigned long count=0;
 int modes[]={AXIMM,AXINF,AXINM,AXNON};
 for(arch=0;arch<2;arch++)for(op=0;op<2;op++)for(mode=0;mode<4;mode++)for(reg=0;reg<65536;reg++){
  Inst i={op?IMFRAME:IMCALL,modes[mode],reg};route(arch,&i);
  if(mode==0&&reg<32768){assert(fast==(op?2:1));assert(mask==0);}
  else {assert(fast==0);assert(mask==(op?67:95));}
  count++;
 }
 printf("PASS JIT module dispatch: %lu x86/ARM signed link selectors, native eligibility and helper exit masks\n",count);
 return 0;
}
'''.replace('X86',cases('386')).replace('ARM32',cases('arm'))
with tempfile.TemporaryDirectory(prefix='jit-module-dispatch-') as td:
 td=Path(td);(td/'dispatch.c').write_text(unit)
 subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+['-I'+str(root/'include'),str(td/'dispatch.c'),'-o',str(td/'dispatch')],check=True)
 subprocess.run([str(td/'dispatch')],check=True)
