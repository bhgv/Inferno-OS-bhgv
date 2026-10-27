#!/usr/bin/env python3
"""Check MCALL leaves against complete independent VM images and write traces."""
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


with tempfile.TemporaryDirectory(prefix='mcall-jit-') as directory:
    td=Path(directory)
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0 // Null pointer in the standalone emitter fixture.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Use independent import, register, frame and heap layouts for MCALL. */
int emit(u32int *out,int cap,int wb,int bad)
{
 LeafLayout a={0};a.instbytes=24;a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.fp=32;a.module=40;a.regmp=64;a.regtemp=72;a.linkcompiled=16;
 a.framelr=0;a.framefp=8;a.framemr=24;
 a.linknlinks=48;a.linklinks=56;a.modlbytes=16;a.modlpc=0;
 a.linkprog=24;a.linkmp=0;a.heapref=24;a.refbytes=8;
 switch(bad){case -1:a.modlpc=8;break;case 1:a.module=-1;break;case 2:a.regtemp=3;break;
 case 3:a.framemr=8;break;case 4:a.heapref=0;break;case 5:a.refbytes=4;break;
 case 6:a.linknlinks=3;break;case 7:a.linklinks=2048;break;case 8:a.modlbytes=8;break;
 case 9:a.modlpc=3;break;case 10:a.linkprog=-1;break;
 case 11:a.linkcompiled=-1;break;case 12:a.linkcompiled=1;break;case 13:a.linkcompiled=2048;break;
 case 14:a.instbytes=-1;break;case 15:a.instbytes=0;break;case 16:a.instbytes=2048;break;}
 return leafemit(out,cap,&a,IMCALL);
}
''')
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*3
        dis=Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[]


        # Include same-value stores when comparing the exact native write sequence.
        def write(machine,access,address,size,value,context):
            writes.append((address,size))


        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        checks=0
        for wb in (4,8):
            for variant in (0,1):
                out=(C.c_uint32*128)();n=lib.emit(out,128,wb,-variant)
                require(0<n<=128 and lib.emit(None,128,wb,-variant)==n,(arch,wb,'sizing'))
                blob=bytes(out)[:n*4]
                require(sum(i.size for i in dis.disasm(blob,CODE))==n*4,'decoding')
                for cap in range(n):
                    guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    require(lib.emit(guard,cap,wb,-variant)==0 and list(guard)==[0xa5a5a5a5]*128,'partial publication')
                for bad in range(1,17):
                    guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    require(lib.emit(guard,128,wb,bad)==0 and list(guard)==[0xa5a5a5a5]*128,(arch,bad))
                machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+512)
                for profile in range(36):
                    for sa in range(8):
                        for fa in range(8):
                            image=bytearray(b'\xa5'*0x4000)
                            src=0x100+sa;mid=0x200+sa;dst=0x300+sa;obj=0x800;fo=0x1800+fa
                            owner=BASE+obj;current=owner;frame=BASE+fo;index=0;count=3;ref=1
                            program=BASE+0x2800;target=BASE+0x3000;mp=BASE+0x2200
                            oldpc=BASE+0x2c00;oldfp=BASE+0x2000;calleecompiled=0;callercompiled=0
                            if profile==1:index=1
                            if profile==2:index=2
                            if profile==3:index=-1
                            if profile==4:index=3
                            if profile==5:index=1<<(wb*8-2)
                            if profile==6:count=0
                            if profile==7:count=-1
                            if profile==8:owner=0
                            if profile==9:owner=MASK
                            if profile==10:current=0
                            if profile==11:current=MASK
                            if profile==12:current=BASE+0xa00
                            if profile==13:program=0
                            if profile==14:program=MASK
                            if profile==15:target=0
                            if profile==16:target=MASK
                            if profile==17:frame=0
                            if profile==18:frame=MASK
                            if profile==19:ref=0
                            if profile==20:ref=MASK
                            if profile==21:ref=MASK-1
                            if profile==22:ref=1<<32
                            if profile==23:src=fo
                            if profile==24:mid=fo+8
                            if profile==25:dst=fo+24
                            if profile==26:oldfp=0
                            if profile==27:oldpc=0x123456789abcdef0
                            if profile==28:mp=MASK
                            if profile==29:mp=0
                            if profile==30:ref=MASK>>1
                            if profile==31:ref=MASK-2;index=2
                            if profile==32:current=BASE+0xa00;callercompiled=1
                            if profile==33:current=BASE+0xa00;callercompiled=calleecompiled=1
                            if profile==34:callercompiled=calleecompiled=-1
                            if profile==35:current=BASE+0xa00;callercompiled=calleecompiled=0x12345678
                            put(image,obj+16,calleecompiled,4);put(image,0xa10,callercompiled,4);
                            put(image,obj-24,ref);put(image,obj,mp);put(image,obj+24,program);put(image,obj+48,count,4)
                            for entry in range(3):put(image,obj+56+entry*16+variant*8,MASK)
                            if 0<=index<3:put(image,obj+56+index*16+variant*8,target)
                            put(image,src,frame);put(image,mid,index,wb);put(image,dst,owner)
                            put(image,8,BASE+src);put(image,24,BASE+mid);put(image,16,BASE+dst)
                            if profile in (8,9,10,11):put(image,24,BASE+0x6000)
                            put(image,0,oldpc);put(image,32,oldfp);put(image,40,current)
                            valid=owner not in (0,MASK) and current not in (0,MASK) and (current==owner or calleecompiled==callercompiled) and program not in (0,MASK) and count>=0 and 0<=index<count and target not in (0,MASK) and frame not in (0,MASK) and ref not in (0,MASK)
                            expected=bytearray(image);expectedwrites=[]
                            if valid:
                                put(expected,fo,oldpc);put(expected,fo+8,oldfp);put(expected,fo+24,current)
                                put(expected,32,frame);put(expected,40,owner);put(expected,obj-24,ref+1);put(expected,64,mp);put(expected,0,target);put(expected,72,1,wb)
                                expectedwrites=[(BASE+fo,8),(BASE+fo+8,8),(BASE+fo+24,8),(BASE+32,8),(BASE+40,8),(BASE+obj-24,8),(BASE+64,8),(BASE,8),(BASE+72,wb)]
                            machine.mem_write(BASE,bytes(image));writes.clear()
                            if arch=='amd64':
                                machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,0x3f80)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x11)
                            machine.emu_start(CODE,STOP,count=300)
                            result=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                            detail=(arch,wb,variant,profile,sa,fa)
                            require(result==valid,(detail,'return',result,valid))
                            require(bytes(machine.mem_read(BASE,0x4000))==expected,(detail,'image'))
                            require(writes==expectedwrites,(detail,'writes',writes))
                            if arch=='amd64':require(machine.reg_read(UC_X86_REG_MXCSR)==0x3f80,'MXCSR')
                            else:require(machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x11,'FP state')
                            checks+=1
        print('PASS',arch,'MCALL:',checks,'image/write/FP checks')
