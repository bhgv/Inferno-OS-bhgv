#!/usr/bin/env python3
"""Exercise scalar cons machine code around an ABI-hostile emulated allocator."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT=Path(__file__).resolve().parents[2]
BASE,CODE,STOP,HELPER=0x100000000,0x10000,0x11000,0x123456780000
TAIL,NODE,TYPE,PROP,STACK=BASE+0x3000,BASE+0x5000,BASE+0x4000,BASE+0x4010,BASE+0x7800
MASK=(1<<64)-1
VALUES=(0,1,MASK,1<<63,0x7ff0000000000001,0x7ff8000012345678,0x0123456789abcdef,0xffffffff00000001)


# Serialize native cells and VM values independently of instruction selection.
def put(memory, offset, value, width=8):
    memory[offset:offset+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')


# Keep all oracles active under optimized Python.
def require(condition, detail):
    if not condition:raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='scalar-cons-jit-') as temporary:
    td=Path(temporary)
    for arch in os.environ.get('CONS_ARCHES','amd64 arm64').split():
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null identity for bounded emission helpers.
#include "isa.h"
#include HEADER


/* Emit each real scalar constructor with an independent high allocator identity. */
int
emit(u32int *output, int cap, int wb, int op)
{
    int ops[]={ICONSB,ICONSW,ICONSL,ICONSF};
    LeafLayout a={0};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24; a.listdata=16;
    a.consentry=(void*)0x123456780000ULL;
    return leafemit(output,cap,&a,ops[op]);
}
'''.replace('HEADER','"'+arch+'-emit.h"'))
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x2000);machine.mem_map(HELPER,0x1000);machine.mem_map(BASE,0x8000)
        machine.mem_write(HELPER,b'\xc3' if arch=='amd64' else bytes.fromhex('c0035fd6'))
        state={};writes=[]


        # Model only the allocator contract, then clobber every ordinary caller-saved integer register.
        def allocator(uc, address, size, context):
            if address!=HELPER:return
            state['called']+=1
            require(state['called']==1,('duplicate allocator',state['detail']))
            width=uc.reg_read(xr.UC_X86_REG_RDI if arch=='amd64' else ar.UC_ARM64_REG_X0)
            slot=uc.reg_read(xr.UC_X86_REG_RSI if arch=='amd64' else ar.UC_ARM64_REG_X1)
            sp=uc.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)
            require(width==state['allocation'] and slot==state['slot'],('call args',state['detail'],width,hex(slot)))
            require(sp%16==(8 if arch=='amd64' else 0),('call alignment',state['detail'],hex(sp)))
            if not state['valid']:
                uc.emu_stop();return
            old=state['old'];expected=state['expected']
            put(expected,0x5000-32,7,4);put(expected,0x5000-24,1);put(expected,0x5000-16,TYPE)
            put(expected,0x5000,old);put(expected,0x5008,0)
            put(expected,0x4000,state['refs']+1,4)
            if old!=MASK and state['color']!=7:
                put(expected,0x3000-32,3,4);put(expected,0x4010,1,4)
            put(expected,slot-BASE,NODE)
            # Host writes represent runtime allocation, not native scalar stores.
            for start,length in ((0x5000-32,48),(0x4000,4),(0x3000-32,4),(0x4010,4),(slot-BASE,8)):
                uc.mem_write(BASE+start,bytes(expected[start:start+length]))
            if arch=='amd64':
                for name in ('RCX','RDX','RSI','RDI','R8','R9','R10','R11'):
                    uc.reg_write(getattr(xr,'UC_X86_REG_'+name),0xbad00000+len(name))
                uc.reg_write(xr.UC_X86_REG_RAX,NODE)
            else:
                for reg in range(1,19):uc.reg_write(getattr(ar,'UC_ARM64_REG_X'+str(reg)),0xbad00000+reg)
                uc.reg_write(ar.UC_ARM64_REG_X0,NODE)


        machine.hook_add(UC_HOOK_CODE,allocator)
        machine.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,address,size,value,context:writes.append((address,size)))
        checks=0
        for wb in (4,8):
            for op in range(4):
                width=1 if op==0 else wb if op==1 else 8
                allocation=4 if op==0 else width
                output=(C.c_uint32*130)(*[0xdeadbeef]*130);units=lib.emit(output,128,wb,op)
                require(0<units<=128,('size',arch,wb,op,units))
                require(lib.emit(None,128,wb,op)==units and list(output)[128:]==[0xdeadbeef]*2,'size/canary')
                for cap in range(units):
                    short=(C.c_uint32*130)(*[0xdeadbeef]*130)
                    require(lib.emit(short,cap,wb,op)==0 and list(short)==[0xdeadbeef]*130,'partial code publication')
                    checks+=1
                machine.mem_write(CODE,bytes(output)[:units*4]);machine.ctl_remove_cache(CODE,CODE+512)
                for profile in range(12):
                    for sa in range(8):
                        for da in range(8):
                            for alias in range(3):
                                for value in VALUES:
                                    image=bytearray(b'\xa5'*0x8000)
                                    old=MASK if profile==0 else 0 if profile==3 else 8 if profile==10 else TAIL+1 if profile==11 else TAIL
                                    refs=(1<<31)-1 if profile==5 else 0 if profile==7 else 1 if profile==0 else 2
                                    color=7 if profile in (1,9) else 3 if profile==8 else 73
                                    oldrefs=0 if profile==4 else MASK if profile==2 else 2
                                    src,dst=0x100+sa,0x200+da
                                    if alias==1:src=dst
                                    if alias==2:src=0x3010+sa
                                    put(image,src,value,width);put(image,dst,old)
                                    captured=bytes(image[src:src+width])
                                    put(image,0x3000-32,color,4);put(image,0x3000-24,oldrefs);put(image,0x3000-16,TYPE)
                                    put(image,0x4000,refs,4);put(image,0x4010,int(profile==9),4)
                                    put(image,0,0x12345678);put(image,8,BASE+src);put(image,16,BASE+dst)
                                    expected=bytearray(image);valid=profile not in (3,4,5,6,7,10,11)
                                    detail=(arch,wb,op,profile,sa,da,alias,hex(value))
                                    state.clear();state.update(called=0,allocation=allocation,slot=BASE+dst,old=old,refs=refs,color=color,valid=valid,expected=expected,detail=detail)
                                    if arch=='amd64':
                                        put(image,0x7808,STOP);put(expected,0x7808,STOP)
                                        machine.reg_write(xr.UC_X86_REG_RDI,BASE);machine.reg_write(xr.UC_X86_REG_RSP,STACK+8)
                                        machine.reg_write(xr.UC_X86_REG_MXCSR,0x3f95)
                                        saved=[getattr(xr,'UC_X86_REG_'+v) for v in ('RBX','RBP','R12','R13','R14','R15')]
                                    else:
                                        machine.reg_write(ar.UC_ARM64_REG_X0,BASE);machine.reg_write(ar.UC_ARM64_REG_SP,STACK);machine.reg_write(ar.UC_ARM64_REG_LR,STOP)
                                        machine.reg_write(ar.UC_ARM64_REG_FPCR,0x400000);machine.reg_write(ar.UC_ARM64_REG_FPSR,0x15)
                                        saved=[getattr(ar,'UC_ARM64_REG_X'+str(v)) for v in range(19,30)]
                                    for reg in saved:machine.reg_write(reg,0xabcdef000+reg)
                                    machine.mem_write(BASE,bytes(image));writes.clear()
                                    try:machine.emu_start(CODE,STOP,count=1000)
                                    except Exception as error:raise RuntimeError(('execution',detail)) from error
                                    require(state['called']==1,('allocator not called',detail))
                                    if valid:
                                        expected[0x5010:0x5010+width]=captured
                                        status=machine.reg_read(xr.UC_X86_REG_RAX if arch=='amd64' else ar.UC_ARM64_REG_X0)
                                        require(status==1,('return',detail,status))
                                        sp=machine.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)
                                        require(sp==STACK+(16 if arch=='amd64' else 0),('restored SP',detail))
                                        for reg in saved:require(machine.reg_read(reg)==0xabcdef000+reg,('saved register',detail,reg))
                                        if arch=='arm64':require(machine.reg_read(ar.UC_ARM64_REG_LR)==STOP,('restored LR',detail))
                                    actual=bytearray(machine.mem_read(BASE,len(image)))
                                    # Native stack staging is private; all other VM bytes are exact.
                                    stackend=0x7808 if arch=='amd64' else 0x7800
                                    actual[stackend-32:stackend]=expected[stackend-32:stackend]
                                    require(actual==expected,('VM image',detail))
                                    entrysp=STACK+(8 if arch=='amd64' else 0)
                                    allowed=set(range(entrysp-32,entrysp))
                                    if valid:allowed.update(range(NODE+16,NODE+16+width))
                                    require(all(address+i in allowed for address,count in writes for i in range(count)),('write bounds',detail,writes))
                                    if arch=='amd64':require(machine.reg_read(xr.UC_X86_REG_MXCSR)==0x3f95,'MXCSR')
                                    else:require(machine.reg_read(ar.UC_ARM64_REG_FPCR)==0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR)==0x15,'FP state')
                                    checks+=1
                print('PASS',arch,'scalar cons:',checks,'ABI/clobber/alias/error/FP checks, WORD',wb*8,'op',op,'units',units,flush=True)
