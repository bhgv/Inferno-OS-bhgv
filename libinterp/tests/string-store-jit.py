#!/usr/bin/env python3
"""Execute AMD64/ARM64 INSC leaves against independent owner/byte-write oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR
ROOT=Path(__file__).resolve().parents[2]
BASE=0x100000000;CODE=0x10000;STOP=0x11000;MASK=(1<<64)-1


def put(image, offset, value, size=8):
    """Write little-endian native/VM fields without reusing emitter calculations."""
    image[offset:offset+size]=(value & ((1<<(size*8))-1)).to_bytes(size,'little')


with tempfile.TemporaryDirectory(prefix='string-store-jit-') as directory:
    td=Path(directory);checks=0
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "'''+arch+'''-emit.h"
/* Stage the actual store with independent string/heap layouts and malformed variants. */
int emit(u32int *out,int cap,int wb,int field,int value){
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.stringlen=0;a.stringmax=4;a.stringdata=24;a.heapref=24;a.refbytes=8;
 switch(field){case 1:a.stringlen=value;break;case 2:a.stringmax=value;break;
 case 3:a.stringdata=value;break;case 4:a.heapref=value;break;case 5:a.refbytes=value;break;}
 return leafemit(out,cap,&a,IINSC);
}
''')
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*4
        dis=Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[]
        # Detect partial publication and same-value stores on every declined path.
        def write(machine,access,address,size,value,context):
            writes.append((address,size))
        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        for wb in (4,8):
            out=(C.c_uint32*64)(*[0xa5a5a5a5]*64);n=lib.emit(out,64,wb,0,0)
            assert 0<n<=64 and lib.emit(None,64,wb,0,0)==n
            blob=bytes(out)[:n*4];assert sum(i.size for i in dis.disasm(blob,CODE))==n*4
            for cap in range(n):
                guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                assert lib.emit(guard,cap,wb,0,0)==0 and list(guard)==[0xa5a5a5a5]*64;checks+=1
            for field,values in ((1,(-1,1,2048)),(2,(-1,1,2048)),(3,(-1,2048)),(4,(-1,0,1,256)),(5,(0,4,16))):
                for value in values:
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,wb,field,value)==0 and list(guard)==[0xa5a5a5a5]*64;checks+=1
            machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+256)
            for wide in (0,1):
                for profile in range(16):
                    for value in (0,65,127,128,0x20ac,0xffffffff,0x100000041,0x100000080):
                        for sa in range(8):
                            for mi in range(4):
                                for di in range(4):
                                    for alias in (0,1):
                                        ma=mi*2+(sa&1);da=di*2+((sa>>1)&1)
                                        image=bytearray(b'\xa5'*0x4000);src=0x100+sa;middle=0x200+ma;dst=0x300+da
                                        obj=0x800+da;owner=BASE+obj;ref=1;length=-8 if wide else 8;capacity=16;index=7 if profile==1 else 0
                                        if profile==2:index=8
                                        if profile==3:index=-1
                                        if profile==4:index=1<<32 if wb==8 else -1
                                        if profile==5:capacity=7
                                        if profile==6:length=-(1<<31)
                                        if profile==7:owner=MASK
                                        if profile==8:owner=0
                                        if profile==9:ref=0
                                        if profile==10:ref=2
                                        if profile==11:ref=MASK
                                        if profile==12:capacity=-1
                                        if profile==13:length=(1<<31)-1
                                        if profile==14:index=(1<<31)-1
                                        if profile==15:capacity=0
                                        put(image,obj-24,ref);put(image,obj,length,4);put(image,obj+4,capacity,4)
                                        for i in range(8):put(image,obj+24+i*(4 if wide else 1),0x400+i if wide else 65+i,4 if wide else 1)
                                        put(image,src,value,wb);put(image,middle,index,wb);put(image,dst,owner)
                                        if alias:src=obj+24+(index if 0<=index<8 else 0)*(4 if wide else 1)
                                        put(image,8,BASE+src);put(image,24,BASE+middle);put(image,16,BASE+dst)
                                        rune=int.from_bytes(image[src:src+wb],'little')&0xffffffff
                                        valid=owner==BASE+obj and ref==1 and length!=-(1<<31) and abs(length)<=capacity and 0<=index<abs(length) and (length<0 or rune<128)
                                        expected=bytearray(image);target=obj+24+index*(4 if wide else 1)
                                        if valid:put(expected,target,rune,4 if wide else 1)
                                        machine.mem_write(BASE,bytes(image));writes.clear()
                                        if arch=='amd64':
                                            machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                            machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,0x3f80)
                                        else:
                                            machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                            machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x11)
                                        machine.emu_start(CODE,STOP,count=300)
                                        result=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                                        assert result==valid,(arch,wb,wide,profile,value,sa,ma,da,alias,result)
                                        assert bytes(machine.mem_read(BASE,0x4000))==bytes(expected),(arch,wb,wide,profile,value,sa,ma,da,alias)
                                        assert writes==([(BASE+target,4 if wide else 1)] if valid else [])
                                        if arch=='amd64':assert machine.reg_read(UC_X86_REG_MXCSR)==0x3f80
                                        else:assert machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x11
                                        checks+=1
        print('PASS',arch,'string store owner/capacity/alias/FP checks, cumulative',checks)
