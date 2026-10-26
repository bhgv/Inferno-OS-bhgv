#!/usr/bin/env python3
"""Probe staged mapped-copy emission bounds under sanitizer-instrumented C."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='mapped-copy-layouts-') as directory:
    td=Path(directory)
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "'''+os.environ.get('MAPPED_'+arch.upper()+'_EMITTER',arch+'-emit.h')+'''"
/* Keep every publication/canary check active under NDEBUG. */
static void
mappedcheck(int condition)
{
 if(!condition)
  abort();
}


/* Retain sanitizer instrumentation without unavailable leak-process inspection. */
int __lsan_is_turned_off(void){return 1;}
/* Vary native offsets and capacities independently of operation execution fixtures. */
int main(void){
 LeafLayout a;u32int out[130],before[130];
 int offsets[]={0,1,4,8,24,28,120,128,248,255,256,2040,2044,2047,2048,32760};
 int wb,op,field,oi,cap,n,m,i;unsigned long checks=0;
 for(wb=4;wb<=8;wb+=4)for(op=0;op<3;op++)for(field=0;field<17;field++)for(oi=0;oi<16;oi++){
  memset(&a,0,sizeof(a));a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;a.module=40;
  a.linkmodule=8;a.moduletypes=32;a.modulenty=40;a.typesize=24;a.typenp=28;
  a.listdata=24;a.heapref=24;a.refbytes=8;a.arraylen=8;a.arraytype=16;a.arraydata=32;
  switch(field){
  case 0:a.arraylen=a.arraytype=a.arraydata=a.module=a.linkmodule=a.moduletypes=a.modulenty=a.typesize=a.typenp=a.listdata=offsets[oi];a.s=a.d=a.m=a.pc=32760;break;
  case 1:a.module=offsets[oi];break;case 2:a.linkmodule=offsets[oi];break;
  case 3:a.moduletypes=offsets[oi];break;case 4:a.modulenty=offsets[oi];break;
  case 5:a.typesize=offsets[oi];break;case 6:a.typenp=offsets[oi];break;
  case 7:a.listdata=offsets[oi];break;case 8:a.heapref=offsets[oi];break;
  case 9:a.refbytes=offsets[oi];break;case 10:a.s=offsets[oi];break;
  case 11:a.m=offsets[oi];break;case 12:a.d=offsets[oi];break;case 13:a.pc=offsets[oi];break;
  case 14:a.arraylen=offsets[oi];break;case 15:a.arraytype=offsets[oi];break;case 16:a.arraydata=offsets[oi];break;}
  n=leafemit(nil,128,&a,op==2 ? ISLICELA : op ? IHEADMP : IMOVMP);mappedcheck(n>=0 && n<=128);
  for(cap=0;cap<=128;cap++){
   memset(out,0xa5,sizeof(out));memmove(before,out,sizeof(out));
   m=leafemit(out+1,cap,&a,op==2 ? ISLICELA : op ? IHEADMP : IMOVMP);
   mappedcheck(m==(n && cap>=n ? n : 0));
   if(!m)mappedcheck(!memcmp(out,before,sizeof(out)));
   else{mappedcheck(out[0]==before[0]);for(i=m+1;i<130;i++)mappedcheck(out[i]==before[i]);}
   checks++;
  }
 }
 printf("PASS '''+arch+''' mapped layout/capacity checks: %lu\\n",checks);return 0;
}
''')
        target=td/arch
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(target)],check=True)
        subprocess.run([str(target)],check=True)
