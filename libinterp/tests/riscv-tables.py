#!/usr/bin/env python3
"""Execute production RV64 case/goto leaves in Unicorn, independently of host pointer width."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc,UC_ARCH_RISCV,UC_MODE_RISCV64,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
import unicorn.riscv_const as regs

ROOT=Path(__file__).resolve().parents[2]
CODE=0x10000;BASE=0x100000000;MASK=(1<<64)-1

def require(ok,detail):
    if not ok:raise RuntimeError(detail)

def put(data,at,value,width=8):data[at:at+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')

def signed(value,width):
    value&=(1<<(width*8))-1
    return value-(1<<(width*8)) if value>>(width*8-1) else value

def main():
    checks=0
    with tempfile.TemporaryDirectory(prefix='rv-tables-') as td:
        work=Path(td);source=work/'test.c';binary=work/'test.so'
        source.write_text('''#include <stdint.h>
typedef uint32_t u32int;
#define nil 0
#include "isa.h"
#include "riscv-emit.h"
int emit(u32int*out,int cap,int wb,int op,int m,int large,int stride,int bad){
 RvLayout a={0};int compiled=large?512:16;int ops[]={ICASE,ICASEL,IGOTO};
 a.xlen=64;a.wordbytes=wb;a.hasm=m;a.pc=0;a.s=8;a.d=16;a.m=24;a.module=40;
 a.linkmodule=large?256:8;a.moduleprog=large?256:16;a.modulenprog=large?512:24;a.instbytes=stride;
 if(bad==1)compiled=-1;if(bad==2)compiled=1;if(bad==3)compiled=2048;
 if(bad==4)a.moduleprog=1;if(bad==5)a.instbytes=0;if(bad==6)a.xlen=32;
 if(bad==7)a.s=1;if(bad==8)a.linkmodule=-1;if(bad==9)a.modulenprog=2048;
 return rvtableemit(out,cap,&a,ops[op],compiled);
}
''')
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+
                       ['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*7
        for wb in (4,8):
            for op in range(3):
                for hasm in (0,1):
                    for large in (0,1):
                        for stride in (1,40,2047):
                            out=(C.c_uint32*386)(*([0xdeadbeef]*386))
                            args=(wb,op,hasm,large,stride)
                            n=lib.emit(out,384,*args,0);require(0<n<=384,('emit',args,n))
                            require(lib.emit(None,384,*args,0)==n,'size mismatch')
                            require(list(out)[n:]==[0xdeadbeef]*(386-n),'code suffix')
                            if not hasm:require(all(not (v&127==0x33 and v>>25==1) for v in list(out)[:n]),'M opcode in RV64I leaf')
                            for cap in (-1,0,n-1):
                                guard=(C.c_uint32*386)(*([0xa5a5a5a5]*386))
                                require(lib.emit(guard,cap,*args,0)==0 and list(guard)==[0xa5a5a5a5]*386,'partial publication')
                            for bad in range(1,10):
                                guard=(C.c_uint32*386)(*([0xa5a5a5a5]*386))
                                require(lib.emit(guard,384,*args,bad)==0 and list(guard)==[0xa5a5a5a5]*386,('layout refusal',bad));checks+=1
                            blob=bytes(out)[:n*4]

                            def execute(count,value,sa=0,ta=0,bad=None):
                                nonlocal checks
                                image=bytearray(0x40000);link=0x1000;module=0x2000;source=0x3000+sa;table=0x4000+ta
                                lm=256 if large else 8;lc=512 if large else 16;mp=256 if large else 16;np=512 if large else 24
                                program=BASE+0x30000;total=32;fallback=31
                                put(image,0,0x123456789abcdef);put(image,8,BASE+source);put(image,16,BASE+table+(4 if op==2 else 0));put(image,40,BASE+link)
                                put(image,link+lm,BASE+module);put(image,link+lc,0,4);put(image,module+mp,program);put(image,module+np,total,4)
                                width=8 if op==1 else wb;put(image,source,value,width);value=signed(value,width)
                                put(image,table,count,4);header=8 if op==1 else 4;rowbytes=4 if op==2 else 24 if op==1 else 12
                                destination=fallback
                                if op==2:
                                    for i in range(count):put(image,table+4+4*i,i%30+1,4)
                                    success=0<=value<count and 0<count<=4095
                                    if success:destination=value%30+1
                                else:
                                    success=0<=count<=4095
                                    for i in range(count):
                                        low=-100+i*7;high=low+3;at=table+header+rowbytes*i
                                        put(image,at,low,8 if op==1 else 4);put(image,at+(8 if op==1 else 4),high,8 if op==1 else 4);put(image,at+(16 if op==1 else 8),i%30+1,4)
                                        if low<=value<high:destination=i%30+1
                                    put(image,table+header+rowbytes*count,fallback,4)
                                if bad:
                                    success=False
                                    if bad.startswith('source-'):put(image,8,{'nil':0,'H':MASK,'wrap':MASK-2}[bad[7:]])
                                    elif bad.startswith('table-'):put(image,16,{'nil':0,'H':MASK,'wrap':MASK-2}[bad[6:]])
                                    elif bad.startswith('link-'):put(image,40,{'nil':0,'H':MASK,'wrap':MASK-2}[bad[5:]])
                                    elif bad.startswith('module-'):put(image,link+lm,{'nil':0,'H':MASK,'wrap':MASK-2}[bad[7:]])
                                    elif bad=='program-empty':put(image,module+np,0,4)
                                    elif bad.startswith('program-'):put(image,module+mp,{'nil':0,'H':MASK,'wrap':MASK-1}[bad[8:]])
                                    elif bad=='compiled':put(image,link+lc,1,4)
                                    elif bad=='negative-count':put(image,table,-1,4)
                                    elif bad=='target-outside':
                                        if op==2:put(image,table+4,total,4)
                                        else:put(image,table+header+(16 if op==1 else 8),total,4)
                                    else:raise RuntimeError(bad)
                                before=bytes(image);u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64);u.mem_map(CODE,0x4000);u.mem_map(BASE,len(image))
                                u.mem_write(CODE,blob);u.mem_write(BASE,before);u.reg_write(regs.UC_RISCV_REG_A0,BASE);u.reg_write(regs.UC_RISCV_REG_RA,CODE+len(blob))
                                preserved=[getattr(regs,'UC_RISCV_REG_'+r) for r in ['SP','GP','TP']+['S'+str(i) for i in range(12)]]
                                for r in preserved:u.reg_write(r,0xabc000+r*16)
                                def aligned(uc,access,address,size,value,user):require(address%size==0,('unaligned native load',hex(address),size))
                                u.hook_add(UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,aligned)
                                u.emu_start(CODE,CODE+len(blob),count=10000)
                                require(u.reg_read(regs.UC_RISCV_REG_PC)==CODE+len(blob),('did not return',args,count,value,bad))
                                require(u.reg_read(regs.UC_RISCV_REG_A0)==int(success),('result',args,count,value,bad,u.reg_read(regs.UC_RISCV_REG_A0)))
                                for r in preserved:require(u.reg_read(r)==0xabc000+r*16,('callee register',r))
                                expected=bytearray(before)
                                if success:put(expected,0,program+destination*stride)
                                require(bytes(u.mem_read(BASE,len(image)))==bytes(expected),('VM/table publication',args,count,value,bad));checks+=1

                            for count in (0,1,2,5,31,4095,4096):
                                values=(-101,-100,-99,-97,0,1,count-1,count,(-100+(count-1)*7),1<<31,1<<40)
                                for value in values:
                                    for sa,ta in ((0,0),(1,7),(7,1)):execute(count,value,sa,ta)
                            for bad in ('source-nil','source-H','source-wrap','table-nil','table-H','table-wrap','link-nil','link-H','link-wrap',
                                        'module-nil','module-H','module-wrap','program-nil','program-H','program-wrap','compiled','negative-count','target-outside','program-empty'):
                                execute(2,0 if op==2 else -100,1,7,bad)
        print('PASS RV64 numeric table emitter:',checks,'independent native executions/layout refusals; RV64I/M, Word32/64, packed cells, high pointers, exact publication and preserved ABI')

if __name__=='__main__':main()
