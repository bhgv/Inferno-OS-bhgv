#!/usr/bin/env python3
"""Execute both priority emitters with independent direct-target and write-trace oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT=Path(__file__).resolve().parents[2]  # Owning emitter tree.
BASE,CODE,STOP=0x100000000,0x10000,0x11000  # Native data and host-return identities.
MASK=(1<<64)-1  # Native address modulus.
NAMES=tuple('b'+relation+kind for kind in ('b','w','l','f','c')
            for relation in ('eq','ne','lt','le','gt','ge'))+('jmp',)  # All direct branches.


# Keep every execution, publication and ABI predicate active under Python -O.
def require(condition,detail):
    if not condition:
        raise RuntimeError(detail)


# Encode independent VM cells without using emitter field helpers.
def put(image,offset,value,width=8):
    require(0<=offset<=len(image)-width,('oracle span',offset,width))
    image[offset:offset+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')


# Compare source order without reproducing native comparison encodings.
def predicate(relation,order):
    return (order==0,order!=0,order<0,order<=0,order>0,order>=0)[relation]


with tempfile.TemporaryDirectory(prefix='direct-target-jit-') as directory:
    td=Path(directory)
    for arch in os.environ.get('DIRECT_TARGET_ARCHES','amd64 arm64').split():
        require(arch in ('amd64','arm64'),arch)
        emitter=(Path(os.environ.get('DIRECT_TARGET_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')).resolve()
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output requests bounded sizing.
#include "isa.h"
#include "HEADER"


/* Keep REG and String layouts independent of the native interpreter structs. */
int
emit(u32int *out,int cap,int wb,int bias,int operation)
{
    int ops[]={OPCODES};
    LeafLayout a={0};
    a.wordbytes=wb;a.pc=bias;a.s=8+bias;a.d=16+bias;a.m=24+bias;
    a.stringlen=12+bias;a.stringmax=16+bias;a.stringdata=32+bias;
    return leafemit(out,cap,&a,ops[operation]);
}
'''.replace('HEADER',str(emitter)).replace('OPCODES',','.join('I'+name.upper() for name in NAMES)))
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))
        command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*4
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x2000);machine.mem_map(BASE,0x3000)
        writes=[]
        machine.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,address,size,value,context:writes.append((address,size)))
        checks=rejections=ignored=0
        for wb in (4,8):
            for bias in (0,32):
                for op in range(31):
                    if os.environ.get('DIRECT_TARGET_OP') and op!=int(os.environ['DIRECT_TARGET_OP']):continue
                    out=(C.c_uint32*130)(*([0xa5a5a5a5]*130))
                    units=lib.emit(out,128,wb,bias,op)
                    require(0<units<=128 and lib.emit(None,128,wb,bias,op)==units,(arch,wb,bias,op,'sizing'))
                    require(list(out)[units:]==[0xa5a5a5a5]*(130-units),'output suffix')
                    for capacity in (-1,0,units-1):
                        guard=(C.c_uint32*130)(*([0xa5a5a5a5]*130))
                        require(lib.emit(guard,capacity,wb,bias,op)==0 and list(guard)==[0xa5a5a5a5]*130,'partial publication')
                    machine.mem_write(CODE,bytes(out)[:units*4]);machine.ctl_remove_cache(CODE,CODE+0x2000)
                    kind=op//6;count=1 if op==30 else 4 if kind in (3,4) else 3
                    for left in range(count):
                        for right in range(count):
                            for sa in (0,1,7):
                                for da in (0,1,7):
                                    for mode in range(7):
                                        for alias in range(2):
                                            image=bytearray(b'\xa5'*0x3000)
                                            s,m,d=0x400+sa,0x500+(sa+3)%8,0x600+da
                                            put(image,bias,BASE+0x2000);put(image,8+bias,BASE+s)
                                            put(image,16+bias,BASE+d);put(image,24+bias,BASE+m)
                                            if kind==4:
                                                for i in range(3):
                                                    at=0x1000+i*0x100
                                                    put(image,at+12+bias,-1 if i==2 else 1,4)
                                                    put(image,at+16+bias,8,4)
                                                    put(image,at+32+bias,256 if i==2 else ord('a' if i==0 else 'z'),4 if i==2 else 1)
                                                put(image,s,MASK if left==0 else BASE+0x1000+(left-1)*0x100)
                                                put(image,m,MASK if right==0 else BASE+0x1000+(right-1)*0x100)
                                            elif kind==3:
                                                image[s:s+8]=struct.pack('<d',float('nan') if left==3 else left-1.0)
                                                image[m:m+8]=struct.pack('<d',float('nan') if right==3 else right-1.0)
                                            elif op!=30:
                                                width=1 if kind==0 else wb if kind==1 else 8
                                                put(image,s,(0,1,255)[left] if kind==0 else left-1,width)
                                                put(image,m,(0,1,255)[right] if kind==0 else right-1,width)
                                            else:
                                                put(image,8+bias,MASK);put(image,24+bias,MASK)
                                            unordered=kind==3 and (left==3 or right==3)
                                            take=op==30 or (op%6==1 if unordered else predicate(op%6,(left>right)-(left<right)))
                                            target=0 if mode==1 else MASK if mode==2 else MASK-1 if mode==3 else BASE+0x2100
                                            if alias:d=bias;put(image,16+bias,BASE+d)
                                            put(image,d,target)
                                            if mode>=4:put(image,16+bias,MASK if mode==4 else 0 if mode==5 else MASK-1)
                                            valid=(not take or mode in (0,3)) and not unordered
                                            expected=bytearray(image)
                                            if valid and take:put(expected,bias,target)
                                            if arch=='amd64':
                                                put(image,0x2800,STOP);put(expected,0x2800,STOP)
                                                machine.reg_write(xr.UC_X86_REG_RDI,BASE);machine.reg_write(xr.UC_X86_REG_RSP,BASE+0x2800)
                                                machine.reg_write(xr.UC_X86_REG_MXCSR,0x3fa5)
                                                preserved=[xr.UC_X86_REG_RBX,xr.UC_X86_REG_RBP,xr.UC_X86_REG_R12,xr.UC_X86_REG_R13,xr.UC_X86_REG_R14,xr.UC_X86_REG_R15]
                                            else:
                                                machine.reg_write(ar.UC_ARM64_REG_X0,BASE);machine.reg_write(ar.UC_ARM64_REG_SP,BASE+0x2800)
                                                machine.reg_write(ar.UC_ARM64_REG_LR,STOP);machine.reg_write(ar.UC_ARM64_REG_FPCR,0x400000);machine.reg_write(ar.UC_ARM64_REG_FPSR,0x15)
                                                preserved=[getattr(ar,'UC_ARM64_REG_X'+str(i)) for i in range(19,30)]
                                            for register in preserved:machine.reg_write(register,0x13579bdf2468ace0+register)
                                            machine.mem_write(BASE,bytes(image));writes.clear()
                                            detail=(arch,wb,bias,NAMES[op],left,right,sa,da,mode,alias)
                                            try:
                                                machine.emu_start(CODE,STOP,count=2000)
                                            except Exception as error:
                                                raise RuntimeError(('unexpected target access',detail)) from error
                                            result=machine.reg_read(xr.UC_X86_REG_RAX if arch=='amd64' else ar.UC_ARM64_REG_X0)
                                            require(result==valid,(detail,'completion',result))
                                            require(bytes(machine.mem_read(BASE,0x3000))==bytes(expected),(detail,'VM image'))
                                            require(writes==([(BASE+bias,8)] if valid and take else []),(detail,'write trace',writes))
                                            require(machine.reg_read(xr.UC_X86_REG_RIP if arch=='amd64' else ar.UC_ARM64_REG_PC)==STOP,(detail,'return'))
                                            require(machine.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)==BASE+0x2800+(8 if arch=='amd64' else 0),(detail,'stack'))
                                            for register in preserved:require(machine.reg_read(register)==0x13579bdf2468ace0+register,(detail,'callee register'))
                                            if arch=='amd64':require(machine.reg_read(xr.UC_X86_REG_MXCSR)==0x3fa5,(detail,'FP state'))
                                            else:require(machine.reg_read(ar.UC_ARM64_REG_FPCR)==0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR)==0x15,(detail,'FP state'))
                                            checks+=1;rejections+=not valid;ignored+=not take and mode>=4
        require(checks and rejections,(arch,'empty matrix'))
        print('PASS',arch,'direct targets:',checks,'image/write/ABI cases,',rejections,'declines,',ignored,'ignored poisoned targets',flush=True)
