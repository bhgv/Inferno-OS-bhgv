#!/usr/bin/env python3
"""Check MFRAME leaves against complete independent VM images and write traces."""
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
ROOT = Path(__file__).resolve().parents[2]
BASE = 0x100000000
CODE = 0x10000
STOP = 0x11000
MASK = (1 << 64)-1


# Encode VM fields independently of emitter instruction choices.
def put(image, offset, value, size=8):
    image[offset:offset+size] = (value & ((1 << (size*8))-1)).to_bytes(size, 'little')


# Fail explicitly so optimized Python runs retain every regression check.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='mframe-jit-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0 // Null pointer in the standalone emitter fixture.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Emit through the production classifier with independent frame metadata. */
int emit(u32int *out, int cap, int wb, int bad)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.module=40;a.linkmodule=8;a.moduletypes=32;a.modulenty=40;
 a.typesize=24;a.typenp=28;a.sp=48;a.ts=56;
 a.frametype=16;a.framemr=24;a.framesize=32;
 a.linknlinks=48;a.linklinks=56;a.modlbytes=16;a.modlframe=8;
 switch(bad){case 1:a.sp=-1;break;case 2:a.ts=3;break;
 case 3:a.frametype=32;break;case 4:a.framemr=16;break;
 case 5:a.framesize=65536;break;case 6:a.typesize=1;break;
 case 7:a.typenp=-1;break;case 8:a.moduletypes=2048;break;case 9:a.modlbytes=8;break;
 case 10:a.linknlinks=-1;break;case 11:a.linklinks=3;break;case 12:a.modlframe=16;break;}
 return leafemit(out,cap,&a,IMFRAME);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib = C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*3
        dis = Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine = Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[]


        # Observe stores even when the value would leave the final image unchanged.
        def write(machine,access,address,size,value,context):
            writes.append((address,size))


        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        checks=0
        for wb in (4,8):
            out=(C.c_uint32*128)();n=lib.emit(out,128,wb,0)
            require(0<n<=128 and lib.emit(None,128,wb,0)==n,(arch,wb,'sizing'))
            blob=bytes(out)[:n*4]
            require(sum(i.size for i in dis.disasm(blob,CODE))==n*4,'decoding')
            for cap in range(n):
                guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                require(lib.emit(guard,cap,wb,0)==0 and list(guard)==[0xa5a5a5a5]*128,'partial publication')
            for bad in range(1,13):
                guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                require(lib.emit(guard,128,wb,bad)==0 and list(guard)==[0xa5a5a5a5]*128,(arch,bad))
            machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+512)
            for profile in range(36):
                for sa in range(8):
                    for da in range(8):
                        for alias in range(3):
                            image=bytearray(b'\xa5'*0x4000)
                            src=0x100+sa;dst=0x200+da;mid=0x300+(sa+da)%8
                            link=BASE+0x500;module=BASE+0x600;table=BASE+0x700;descriptor=BASE+0x800
                            count=1;index=0;size=64;np=0;sp=BASE+0x1800;ts=BASE+0x1c00
                            if profile==1:link=0
                            if profile==2:link=MASK
                            if profile==3:module=0
                            if profile==4:module=MASK
                            if profile==5:table=0
                            if profile==6:table=MASK
                            if profile==7:descriptor=0
                            if profile==8:descriptor=MASK
                            if profile==9:count=0
                            if profile==10:count=-1
                            if profile==11:index=-1
                            if profile==12:index=1
                            if profile==13:index=1<<(wb*8-2)
                            if profile==14:size=-1
                            if profile==15:size=31
                            if profile==16:size=32
                            if profile==17:np=1
                            if profile==18:np=-1
                            if profile==19:sp=0
                            if profile==20:ts=0
                            if profile==21:ts=MASK
                            if profile==22:ts=sp
                            if profile==23:ts=sp+size
                            if profile==24:ts=sp+size+1
                            if profile==25:ts=sp-1
                            if profile==26:size=(1<<31)-1
                            if profile==27:size=1023
                            if profile==28:count=(1<<31)-1
                            if profile==29:src=0x1800+sa
                            if profile==30:dst=0x1800+16
                            if profile==31:dst=0x1800+24
                            if profile==32:count=2;index=1
                            if profile==33:count=3;index=2
                            if profile==34:count=2;index=1;descriptor=MASK
                            if profile==35:count=2;index=1;mid=0x1800+16
                            if alias==1:dst=src
                            if alias==2:src=0x1800+24
                            put(image,src,link);put(image,mid,index,wb);put(image,24,BASE+mid);put(image,8,BASE+src);put(image,16,BASE+dst)
                            if profile in (1,2):put(image,24,BASE+0x6000)
                            put(image,40,link);put(image,0x508,module);put(image,0x620,table);put(image,0x628,count,4)
                            put(image,0x530,count,4);put(image,0x540,MASK);
                            if 0<=index<=2:put(image,0x540+index*16,descriptor)
                            put(image,0x700,descriptor);put(image,0x818,size,4);put(image,0x81c,np,4)
                            put(image,48,sp);put(image,56,ts)
                            valid=(link not in (0,MASK) and descriptor not in (0,MASK) and count>0 and 0<=index<count and size>=32 and np==0 and sp not in (0,MASK) and ts not in (0,MASK) and sp<=ts and size<ts-sp)
                            expected=bytearray(image);expectedwrites=[]
                            if valid:
                                put(expected,48,sp+size);put(expected,0x1810,descriptor);put(expected,0x1818,0);put(expected,dst,sp)
                                expectedwrites=[(BASE+48,8),(BASE+0x1810,8),(BASE+0x1818,8),(BASE+dst,8)]
                            machine.mem_write(BASE,bytes(image));writes.clear()
                            if arch=='amd64':
                                machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,0x3f80)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x11)
                            machine.emu_start(CODE,STOP,count=300)
                            result=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                            detail=(arch,wb,profile,sa,da,alias)
                            require(result==valid,(detail,'return',result,valid))
                            require(bytes(machine.mem_read(BASE,0x4000))==expected,(detail,'image'))
                            require(writes==expectedwrites,(detail,'writes',writes))
                            if arch=='amd64':require(machine.reg_read(UC_X86_REG_MXCSR)==0x3f80,'MXCSR')
                            else:require(machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x11,'FP state')
                            checks+=1
        print('PASS',arch,'MFRAME:',checks,'image/write/FP checks')
