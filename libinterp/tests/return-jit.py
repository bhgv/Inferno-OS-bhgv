#!/usr/bin/env python3
"""Check RET leaves against complete independent VM images and write traces."""
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


with tempfile.TemporaryDirectory(prefix='return-jit-') as directory:
    td=Path(directory)
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0 // Null pointer for the standalone emitter fixture.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Query RET with independent REG, frame and descriptor offsets. */
int emit(u32int *out,int cap,int wb,int bad)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.module=40;a.regmp=56;a.linkcompiled=8;a.linkmp=16;a.heapref=24;a.refbytes=8;
 a.fp=32;a.sp=48;a.frametype=16;a.framemr=24;a.framefp=8;a.framelr=0;a.typenp=28;
 switch(bad){case 1:a.fp=-1;break;case 2:a.sp=3;break;case 3:a.pc=-1;break;
 case 4:a.frametype=8;break;case 5:a.typenp=1;break;case 6:a.framemr=0;break;
 case 7:a.framelr=2048;break;case 8:a.framefp=1;break;
 case 9:a.module=3;break;case 10:a.regmp=-1;break;case 11:a.linkmp=1;break;
 case 12:a.linkcompiled=2;break;case 13:a.heapref=0;break;case 14:a.refbytes=4;break;}
 return leafemit(out,cap,&a,IRET);
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


        # Track every store, including same-value writes on declined paths.
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
            for bad in range(1,15):
                guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                require(lib.emit(guard,128,wb,bad)==0 and list(guard)==[0xa5a5a5a5]*128,(arch,bad))
            machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+512)
            for profile in range(43):
                for alignment in range(64):
                    image=bytearray(b'\xa5'*0x4000)
                    obj=0x1800+alignment;frame=BASE+obj;descriptor=BASE+0x800
                    previous=BASE+0x2400;pc=BASE+0x3000;module=0;np=0
                    current=BASE+0xa00;refs=3;oldmode=0;newmode=0;mp=BASE+0x3800
                    if profile==1:frame=0
                    if profile==2:frame=MASK
                    if profile==3:descriptor=0
                    if profile==4:descriptor=MASK
                    if profile==5:np=1
                    if profile==6:np=-1
                    if profile==7:module=MASK
                    if profile==8:module=BASE+0x600
                    if profile==9:previous=0
                    if profile==10:previous=MASK
                    if profile==11:pc=0
                    if profile==12:pc=MASK
                    if profile==13:previous=frame
                    if profile==14:pc=0x123456789abcdef0
                    if profile==15:np=-(1<<31)
                    if profile==16:np=(1<<31)-1
                    if profile==17:descriptor=BASE+0x900
                    if profile==18:previous=BASE+32
                    if profile==19:pc=frame
                    if profile>=20:module=BASE+0x600
                    if profile==21:module=current
                    if profile==22:refs=1
                    if profile==23:refs=0
                    if profile==24:refs=MASK
                    if profile==25:refs=2
                    if profile==26:oldmode=1
                    if profile==27:current=0
                    if profile==28:current=MASK
                    if profile==29:oldmode=newmode=-1
                    if profile==30:oldmode=newmode=0x12345678
                    if profile==31:mp=0
                    if profile==32:mp=MASK
                    if profile==33:module=current;refs=2
                    if profile==34:module=current;refs=1
                    if profile==35:module=current;refs=0
                    if profile==36:module=current;refs=MASK
                    if profile==37:previous=BASE+40
                    if profile==38:pc=module
                    if profile==39:np=1
                    if profile==40:module=0;current=0
                    if profile==41:module=0;current=MASK
                    if profile==42:module=0;refs=0
                    put(image,0xa00-24,refs);put(image,0xa08,oldmode,4);put(image,0xa10,mp)
                    put(image,0x608,newmode,4);put(image,0x610,mp)
                    put(image,40,current)
                    put(image,obj,pc);put(image,obj+8,previous);put(image,obj+16,descriptor);put(image,obj+24,module)
                    put(image,0x81c,np,4);put(image,0x91c,np,4)
                    put(image,32,frame)
                    valid=frame not in (0,MASK) and descriptor not in (0,MASK) and np==0 and (module==0 or (module!=MASK and current not in (0,MASK) and refs>1 and (oldmode==newmode or module==current))) and previous not in (0,MASK) and pc not in (0,MASK)
                    expected=bytearray(image);expectedwrites=[]
                    if valid:
                        put(expected,32,previous);put(expected,48,frame);put(expected,0,pc)
                        expectedwrites=[(BASE+32,8),(BASE+48,8),(BASE,8)]
                        if module:
                            put(expected,0xa00-24,refs-1);put(expected,40,module);put(expected,56,mp)
                            expectedwrites += [(BASE+0xa00-24,8),(BASE+40,8),(BASE+56,8)]
                    machine.mem_write(BASE,bytes(image));writes.clear()
                    mxcsr=0x3f80 if profile&1 else 0x1f80
                    fpcr=0x800000 if profile&1 else 0
                    if arch=='amd64':
                        machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                        machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,mxcsr)
                    else:
                        machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                        machine.reg_write(UC_ARM64_REG_FPCR,fpcr);machine.reg_write(UC_ARM64_REG_FPSR,0x11)
                    machine.emu_start(CODE,STOP,count=300)
                    result=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                    detail=(arch,wb,profile,alignment)
                    require(result==valid,(detail,'return',result,valid))
                    require(bytes(machine.mem_read(BASE,0x4000))==expected,(detail,'image'))
                    require(writes==expectedwrites,(detail,'writes',writes))
                    if arch=='amd64':require(machine.reg_read(UC_X86_REG_MXCSR)==mxcsr,'MXCSR')
                    else:require(machine.reg_read(UC_ARM64_REG_FPCR)==fpcr and machine.reg_read(UC_ARM64_REG_FPSR)==0x11,'FP state')
                    checks+=1
        print('PASS',arch,'RET:',checks,'image/write/FP checks')
