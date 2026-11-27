#!/usr/bin/env python3
"""Independent full-pointer index results above4GiB on AMD64/ARM64/RV64."""
import argparse,ctypes as C,os,shlex,subprocess,tempfile,sys
from pathlib import Path
from unicorn import Uc,UC_ARCH_X86,UC_ARCH_ARM64,UC_ARCH_RISCV,UC_MODE_64,UC_MODE_RISCV64
from unicorn.x86_const import UC_X86_REG_RDI,UC_X86_REG_RAX,UC_X86_REG_RSP
from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_LR
from unicorn.riscv_const import UC_RISCV_REG_A0,UC_RISCV_REG_RA
ROOT=Path(__file__).resolve().parents[2];BASE=0x100000000;CODE=0x10000;STOP=CODE+0x1000
OPS=['IINDB','IINDW','IINDL','IINDF','IINDX']
def require(condition,detail):
 if not condition:raise RuntimeError(detail)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--control',choices=('amd64','arm64','riscv64'));a=p.parse_args();total=0
 with tempfile.TemporaryDirectory(prefix='array-pointer-width-')as td:
  w=Path(td)
  for arch in ([a.control]if a.control else ['amd64','arm64','riscv64']):
   header='riscv-emit.h'if arch=='riscv64'else arch+'-emit.h';text=(ROOT/'libinterp'/header).read_text()
   if a.control:
    if arch=='amd64':old='x64ptr(&b, 2, abi->m); x64store(&b, 0, 8);';new=old.replace('store(&b, 0, 8)','store(&b, 0, abi->wordbytes)')
    elif arch=='arm64':old='a64ptr(&b, 11, abi->m); a64store(&b, 14, 8);';new=old.replace('store(&b, 14, 8)','store(&b, 14, abi->wordbytes)')
    else:old='rvptr(&b, Rvt2, abi->m); rvstore(&b, Rvt2, Rvt5, 8);';new=old.replace('Rvt5, 8)','Rvt5, abi->wordbytes)')
    require(text.count(old)>=2,"control store missing");text=text.replace(old,new)
   (w/header).write_text(text)
   layout='RvLayout'if arch=='riscv64'else'LeafLayout';emit='rvemit'if arch=='riscv64'else'leafemit'
   source='#include <stdint.h>\n#include <string.h>\ntypedef uint32_t u32int;typedef unsigned char uchar;\n#define nil 0\n#include "isa.h"\n#include "'+header+'"\nint emit(u32int*out,int cap,int wb,int op){'+layout+' a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;a.arraylen=0;a.arraytype=8;a.arraydata=24;a.typesize=24;'+('a.xlen=64;a.hasm=1;a.hasdiv=1;'if arch=='riscv64'else'')+'int ops[]={'+','.join(OPS)+'};return '+emit+'(out,cap,&a,ops[op]);}\n'
   (w/'emit.c').write_text(source);library=w/(arch+'.so');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-shared','-fPIC','-I'+str(w),'-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(w/'emit.c'),'-o',str(library)],check=True)
   lib=C.CDLL(str(library));lib.emit.argtypes=[C.POINTER(C.c_uint32),C.c_int,C.c_int,C.c_int]
   machine=Uc(UC_ARCH_X86 if arch=='amd64'else UC_ARCH_ARM64 if arch=='arm64'else UC_ARCH_RISCV,UC_MODE_64 if arch=='amd64'else UC_MODE_RISCV64 if arch=='riscv64'else 0);machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000);checks=0;detected=False
   def put(data,offset,value,width=8):data[offset:offset+width]=value.to_bytes(width,'little')
   for wb in ([4]if a.control else[4,8]):
    for op in ([0]if a.control else range(5)):
     code=(C.c_uint32*256)();n=lib.emit(code,256,wb,op);require(0<n<=128,"leaf size");machine.mem_write(CODE,bytes(code)[:n*4]);machine.ctl_remove_cache(CODE,CODE+1024)
     for stride in ([0,1,3,8,16]if op==4 else[1 if op==0 else wb if op==1 else 8]):
      for sa in ([0]if a.control else[0,1,4,7]):
       for ia in ([0]if a.control else[0,1,4,7]):
        for oa in ([0]if a.control else[0,1,4,7]):
         for alias in ([0]if a.control else range(4)):
          for index in ([1]if a.control else[0,1,7]):
           data=bytearray(b'\xa5'*0x4000);s=0x100+sa;d=0x200+ia;out=[0x300+oa,s,d,0x418][alias];backing=BASE+0x1000
           put(data,0x400,8,wb);put(data,0x408,BASE+0x600);put(data,0x418,backing);put(data,0x618,stride,4);put(data,s,BASE+0x400);put(data,d,index,wb);put(data,0,CODE+0x2800);put(data,8,BASE+s);put(data,16,BASE+d);put(data,24,BASE+out)
           expected=bytearray(data);put(expected,out,backing+index*stride);machine.mem_write(BASE,bytes(data))
           if arch=='amd64':machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2000);machine.mem_write(CODE+0x2000,STOP.to_bytes(8,'little'))
           elif arch=='arm64':machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
           else:machine.reg_write(UC_RISCV_REG_A0,BASE);machine.reg_write(UC_RISCV_REG_RA,STOP)
           machine.emu_start(CODE,STOP,count=2000);result=machine.reg_read(UC_X86_REG_RAX if arch=='amd64'else UC_ARM64_REG_X0 if arch=='arm64'else UC_RISCV_REG_A0);actual=bytes(machine.mem_read(BASE,0x4000));require(result==1,(arch,wb,OPS[op],'unexpected decline'))
           if a.control:require(actual!=expected,'narrowing control escaped');detected=True
           else:require(actual==expected,(arch,wb,OPS[op],stride,sa,ia,oa,alias,index,'pointer/canary/alias mismatch'))
           checks+=1
   require(not a.control or detected,"control did not run");total+=checks;print('PASS pointer-width',arch,'narrowing control'if a.control else str(checks)+' high-address cases',flush=True)
 print('PASS native array pointer width:',total,'cases',flush=True)
 if not a.control:
  for arch in ('amd64','arm64','riscv64'):subprocess.run([sys.executable,str(Path(__file__).resolve()),'--control',arch],check=True)
  print('PASS all three narrowing omission controls',flush=True)
if __name__=='__main__':main()
