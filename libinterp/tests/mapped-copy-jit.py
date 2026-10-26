#!/usr/bin/env python3
"""Execute mapped-copy leaves against complete memory images and metadata fault oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR
ROOT=Path(__file__).resolve().parents[2]
BASE=0x100000000;CODE=0x10000;STOP=0x11000;MASK=(1<<64)-1


def put(image, offset, value, size=8):
    """Store independently modelled little-endian fields at any byte alignment."""
    image[offset:offset+size]=(value & ((1<<(8*size))-1)).to_bytes(size,'little')


# Keep every evaluated oracle active under Python -O.
def require(condition, detail='mapped-copy invariant'):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='mapped-copy-jit-') as directory:
    td=Path(directory);checks=0
    patterns={}  # Immutable per-length images avoid rebuilding identical byte patterns.
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "'''+arch+'''-emit.h"
/* Use native pointers and independent toy metadata, with hostile layout probes. */
int emit(u32int *out,int cap,int wb,int op,int field,int value){
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;a.module=40;
 a.linkmodule=8;a.moduletypes=32;a.modulenty=40;a.typesize=24;a.typenp=28;
 a.listdata=24;a.heapref=24;a.refbytes=8;
 switch(field){case 1:a.module=value;break;case 2:a.linkmodule=value;break;
 case 3:a.moduletypes=value;break;case 4:a.modulenty=value;break;
 case 5:a.typesize=value;break;case 6:a.typenp=value;break;
 case 7:a.listdata=value;break;case 8:a.heapref=value;break;case 9:a.refbytes=value;break;}
 return leafemit(out,cap,&a,op ? IHEADMP : IMOVMP);
}
''')
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*5
        dis=Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[]
        def write(machine,access,address,size,value,context):
            """Reject even same-byte stores from paths that promise to decline cleanly."""
            writes.append((address,size))
        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        for wb in (4,8):
            for op in (0,1):
                out=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                n=lib.emit(out,128,wb,op,0,0)
                require(0<n<=128 and lib.emit(None,128,wb,op,0,0)==n,(arch,wb,op,n))
                blob=bytes(out)[:n*4]
                exact=(C.c_uint32*(n+1))(*([0xa5a5a5a5]*(n+1)))
                require(lib.emit(exact,n,wb,op,0,0)==n and bytes(exact)[:n*4]==blob and exact[n]==0xa5a5a5a5,'exact capacity/canary')
                require(sum(i.size for i in dis.disasm(blob,CODE))==n*4)
                for cap in range(n):
                    guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    require(lib.emit(guard,cap,wb,op,0,0)==0 and list(guard)==[0xa5a5a5a5]*128)
                for field in range(1,10 if op else 7):
                    values=(-1,1,2048) if field<7 else (-1,2048) if field==7 else (0,7,256) if field==8 else (0,4,16)
                    for value in values:
                        guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                        require(lib.emit(guard,128,wb,op,field,value)==0 and list(guard)==[0xa5a5a5a5]*128,(arch,op,field,value))
                        checks+=1
                machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+512)

                def run(length,sa,da,delta,fault=0,np=0):
                    """Model one decoded invocation without sharing emitter arithmetic or metadata lookup."""
                    if length not in patterns:
                        patterns[length]=bytes((i*37+length*11)&255 for i in range(0x4000))
                    image=bytearray(patterns[length])
                    ma=(sa^da)&7;src=0x1018 if op else 0x1810+sa
                    dest=src+delta+da if delta not in (999,1000) else 0x200+ma if delta==999 else 0x100+sa if op else src
                    sp=BASE+0x100+sa if op else (0 if fault==23 else MASK if fault==24 else BASE+src)
                    index=1;count=2;owner=BASE+0x500;module=BASE+0x700;table=BASE+0x900;descriptor=BASE+0xb00;listowner=BASE+0x1000;refs=10
                    if fault==1:owner=0
                    if fault==2:owner=MASK
                    if fault==3:module=0
                    if fault==4:module=MASK
                    if fault==5:table=0
                    if fault==6:table=MASK
                    if fault==7:count=0
                    if fault==8:count=-1
                    if fault==9:index=-1
                    if fault==10:index=2
                    if fault==11:descriptor=0
                    if fault==12:descriptor=MASK
                    if fault==13:length=-1
                    if fault==14:length=257
                    if fault==15:np=1
                    if fault==16:np=-1
                    if fault==17:listowner=0
                    if fault==18:listowner=MASK
                    if fault==19:refs=0
                    if fault==20:refs=MASK
                    if fault==21:table=MASK-7
                    if fault==22:count=0x7fffffff
                    put(image,0,BASE+0x3000);put(image,8,sp);put(image,16,0 if fault==25 else MASK if fault==26 else BASE+dest);put(image,24,BASE+0x200+ma);put(image,40,owner)
                    put(image,0x100+sa,listowner);put(image,0x200+ma,index,wb)
                    put(image,0x500+8,module);put(image,0x500+24,BASE+0xd00)
                    put(image,0x700+32,table);put(image,0x700+40,count,4)
                    put(image,0x908,descriptor);put(image,0xb00+24,length,4);put(image,0xb00+28,np,4)
                    put(image,0x1000-24,refs)
                    validmeta=fault not in tuple(range(1,14))+(21,) and (not op or fault not in (17,18))
                    accepted=validmeta and 0<=length<=256 and (length==0 or (np==0 and (not op or refs not in (0,MASK))))
                    expected=bytearray(image)
                    if accepted and length:
                        expected[dest:dest+length]=bytes(image[src:src+length])
                        if op:put(expected,8,BASE+src)
                    if arch=='amd64':
                        put(image,0x3f00,STOP);put(expected,0x3f00,STOP)
                        machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,BASE+0x3f00);machine.reg_write(UC_X86_REG_MXCSR,0x5fa5)
                    else:
                        machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                        machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x95)
                    machine.mem_write(BASE,bytes(image));writes.clear();machine.emu_start(CODE,STOP,count=2400)
                    status=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                    require(status==int(accepted),(arch,wb,op,length,sa,da,delta,fault,np,status,accepted))
                    require(bytes(machine.mem_read(BASE,0x4000))==expected,(arch,wb,op,length,delta,fault,np))
                    expectedwrites=[]
                    if accepted and length:
                        if op:
                            expectedwrites.append((BASE+8,8))
                        if dest>src:
                            left=length
                            while left>=8:
                                left-=8
                                expectedwrites.append((BASE+dest+left,8))
                            expectedwrites += [(BASE+dest+i,1) for i in range(left-1,-1,-1)]
                        else:
                            whole=length//8*8
                            expectedwrites += [(BASE+dest+i,8) for i in range(0,whole,8)]
                            expectedwrites += [(BASE+dest+i,1) for i in range(whole,length)]
                    require(writes==expectedwrites,(arch,wb,op,length,delta,fault,'exact store sequence'))
                    if arch=='amd64':require(machine.reg_read(UC_X86_REG_MXCSR)==0x5fa5)
                    else:require(machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x95)

                for length in (0,1,2,7,8,15,16,31,32,63,64,127,128,129,255,256,257):
                    for sa in range(8):
                        for da in range(8):
                            for delta in (-8,-1,0,1,7,8,31,63,127,128,256,999,1000):
                                run(length,sa,da,delta);checks+=1
                for fault in range(23):
                    for length in (0,1,128,256):
                        for shift in range(8):
                            run(length,shift,shift,256,fault);checks+=1
                for fault in (25,26) if op else (23,24,25,26):
                    run(0,0,0,256,fault);checks+=1
        print('PASS',arch,'mapped copies, cumulative metadata/overlap/byte/FP checks',checks)
