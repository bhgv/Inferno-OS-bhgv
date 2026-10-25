#!/usr/bin/env python3
"""Probe bounded RET publication with native offsets and extended module registers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

with tempfile.TemporaryDirectory(prefix='return-layouts-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null staging buffer requests sizing only.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Preserve address/undefined checks without sandbox leak-process inspection. */
int
__lsan_is_turned_off(void)
{
 return 1;
}


/* Every rejected capacity/layout must preserve both output and outer canaries. */
int
main(void)
{
 int offsets[]={-1,0,1,4,8,24,28,120,128,248,255,256,2040,2044,2047,2048};
 LeafLayout a;
 u32int out[66],before[66];
 int wb,field,oi,cap,n,m,i;
 ulong_unused: ;
 unsigned long checks=0;

 for(wb=4;wb<=8;wb+=4)for(field=0;field<14;field++)for(oi=0;oi<16;oi++){
  memset(&a,0,sizeof(a));a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
  a.fp=32;a.sp=48;a.module=40;a.regmp=56;a.linkcompiled=8;a.linkmp=16;
  a.frametype=16;a.framemr=24;a.framefp=8;a.framelr=0;a.typenp=28;a.heapref=24;a.refbytes=8;
  switch(field){
  case 0:a.fp=offsets[oi];break;case 1:a.sp=offsets[oi];break;
  case 2:a.pc=offsets[oi];break;case 3:a.frametype=offsets[oi];break;
  case 4:a.framemr=offsets[oi];break;case 5:a.framefp=offsets[oi];break;
  case 6:a.framelr=offsets[oi];break;case 7:a.typenp=offsets[oi];break;
  case 8:a.module=offsets[oi];break;case 9:a.regmp=offsets[oi];break;
  case 10:a.linkmp=offsets[oi];break;case 11:a.linkcompiled=offsets[oi];break;
  case 12:a.heapref=offsets[oi];break;case 13:a.refbytes=offsets[oi];break;}
  n=leafemit(nil,64,&a,IRET);
  if(n<0 || n>64)abort();
  for(cap=0;cap<=64;cap++){
   memset(out,0xa5,sizeof(out));memmove(before,out,sizeof(out));
   m=leafemit(out+1,cap,&a,IRET);
   if(m!=(n && cap>=n ? n : 0))abort();
   if(!m){if(memcmp(out,before,sizeof(out)))abort();}
   else{
    if(out[0]!=before[0])abort();
    for(i=m+1;i<66;i++)if(out[i]!=before[i])abort();
   }
   checks++;
  }
 }
 printf("PASS '''+arch+''' RET layout/capacity: %lu\\n",checks);
 return 0;
}
'''.replace(' ulong_unused: ;\n',''))
        binary = td/arch
        command = shlex.split(os.environ.get('CC','cc'))
        command += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
