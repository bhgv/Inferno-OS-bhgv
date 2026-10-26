#!/usr/bin/env python3
"""Execute mapped-cons descriptor and allocation callbacks under both native ABIs."""
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
BASE,CODE,STOP=0x100000000,0x10000,0x11000
LOOKUP,ALLOC=0x123456780000,0x123456781000
TYPE,NODE,TAIL,STACK=BASE+0x4000,BASE+0x5000,BASE+0x3000,BASE+0x7800
MASK=(1<<64)-1


# Serialize all model cells independently of the production instruction encodings.
def put(memory, offset, value, width=8):
    memory[offset:offset+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')


# Preserve all oracles under optimized Python and avoid dumping complete byte images.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='mapped-cons-jit-') as directory:
    td=Path(directory)
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null callback identity for bounded emitter probes.
#include "isa.h"
#include HEADER


/* Emit the production mapped constructor with independent high callback identities. */
int
emit(u32int *out, int cap, int wb, int bad)
{
    LeafLayout a={0};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24; a.module=32;
    a.listtail=0; a.listtype=8; a.listdata=16;
    a.typeref=0; a.typesize=24; a.typenp=28;
    a.consentry=(void*)0x123456781000ULL;
    a.constypeentry=(void*)0x123456780000ULL;
    if(bad==1){a.constypeentry=nil;}
    if(bad==2){a.constypeentry=(void*)-1;}
    if(bad==3){a.listtype=16;}
    if(bad==4){a.typesize=0;}
    return leafemit(out,cap,&a,ICONSMP);
}
'''.replace('HEADER','"'+arch+'-emit.h"'))
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))
        command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary))
        lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(BASE,0x8000)
        machine.mem_map(CODE,0x2000)
        machine.mem_map(LOOKUP,0x2000)
        ret=b'\xc3' if arch=='amd64' else bytes.fromhex('c0035fd6')
        machine.mem_write(LOOKUP,ret)
        machine.mem_write(ALLOC,ret)
        state={}
        writes=[]


        # Enforce the two runtime contracts, then destroy ordinary caller-saved register contents.
        def callback(uc, address, size, context):
            if address not in (LOOKUP,ALLOC):
                return
            x64=arch=='amd64'
            first=uc.reg_read(xr.UC_X86_REG_RDI if x64 else ar.UC_ARM64_REG_X0)
            second=uc.reg_read(xr.UC_X86_REG_RSI if x64 else ar.UC_ARM64_REG_X1)
            sp=uc.reg_read(xr.UC_X86_REG_RSP if x64 else ar.UC_ARM64_REG_SP)
            detail=state['detail']
            require(sp%16==(8 if x64 else 0),('callback alignment',detail,sp))
            require(int.from_bytes(uc.mem_read(state['destination'],8),'little')==state['old'],('early publication',detail))
            if address==LOOKUP:
                state['lookups']+=1
                require(state['lookups']==1 and state['allocations']==0,('lookup order',detail))
                require(first==BASE+0x6000 and second==state['index'],('lookup arguments',detail,first,second))
                if state['typeerror']:
                    uc.emu_stop()
                    return
                result=TYPE
            else:
                state['allocations']+=1
                require(state['accepted'] and state['allocations']==1 and state['lookups']==1,('allocation order',detail))
                require(first==state['size'] and STACK-96<=second<STACK,('allocation arguments',detail,first,second))
                require(int.from_bytes(uc.mem_read(second,8),'little')==state['old'],('staged tail',detail))
                if state['allocerror']:
                    uc.emu_stop()
                    return
                expected=state['expected']
                put(expected,0x5000,state['old'])
                put(expected,0x5008,0)
                if state['profile']==5:
                    put(expected,0x4000,5,4)
                    uc.mem_write(TYPE,(5).to_bytes(4,'little'))
                uc.mem_write(NODE,bytes(expected[0x5000:0x5010]))
                uc.mem_write(second,NODE.to_bytes(8,'little'))
                captured=bytes(expected[state['source']-BASE:state['source']-BASE+state['size']])
                expected[0x5010:0x5010+state['size']]=captured
                put(expected,0x5008,TYPE)
                put(expected,0x4000,(5 if state['profile']==5 else 1)+1,4)
                put(expected,state['destination']-BASE,NODE)
                result=NODE
            if x64:
                for name in ('RCX','RDX','RSI','RDI','R8','R9','R10','R11'):
                    uc.reg_write(getattr(xr,'UC_X86_REG_'+name),0xbad00000+len(name))
                uc.reg_write(xr.UC_X86_REG_RAX,result)
            else:
                for reg in range(1,19):
                    uc.reg_write(getattr(ar,'UC_ARM64_REG_X'+str(reg)),0xbad00000+reg)
                uc.reg_write(ar.UC_ARM64_REG_X0,result)


        machine.hook_add(UC_HOOK_CODE,callback)
        machine.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,address,size,value,context:writes.append((address,size)))
        checks=0
        for wb in (4,8):
            output=(C.c_uint32*128)()
            units=lib.emit(output,128,wb,0)
            require(0<units<=128,('size',arch,wb,units))
            code=bytes(output)[:units*4]
            machine.mem_write(CODE,code)
            machine.ctl_remove_cache(CODE,CODE+0x1000)
            for bad in range(1,5):
                require(lib.emit(output,128,wb,bad)==0,('bad layout',arch,wb,bad))
            sentinel=(C.c_uint32*128)(*([0xa5a5a5a5]*128))
            require(lib.emit(sentinel,units-1,wb,0)==0 and bytes(sentinel)==b'\xa5'*512,('transactional size',arch,wb))
            for size in (0,1,7,16,64,256,257,300):
                for profile in range(11):
                    for sa in range(4):
                        for da in range(4):
                            for alias in range(3):
                                memory=bytearray(b'\xa5'*0x8000)
                                sourceptr,destination,indexptr=BASE+0x1000+sa,BASE+0x2000+da,BASE+0x2800
                                old=MASK if alias==0 else TAIL
                                index=0 if profile!=4 else (1<<32)+1
                                if alias==1:
                                    sourceptr=destination
                                if alias==2:
                                    sourceptr=TAIL+16+sa
                                if profile==6:
                                    old=0
                                if profile==7:
                                    sourceptr=0
                                if profile==8:
                                    destination=MASK-3
                                if profile==9:
                                    sourceptr=TYPE
                                if profile==10:
                                    sourceptr=indexptr
                                put(memory,0,BASE+0x6800)
                                put(memory,8,sourceptr)
                                put(memory,16,destination)
                                put(memory,24,indexptr)
                                put(memory,32,BASE+0x6000)
                                put(memory,0x4000,0 if profile==2 else 1,4)
                                put(memory,0x4018,size,4)
                                put(memory,0x401c,1 if profile==1 else 0,4)
                                put(memory,indexptr-BASE,index,wb)
                                if BASE<=destination<BASE+0x7000:
                                    put(memory,destination-BASE,old)
                                captured_index=int.from_bytes(memory[indexptr-BASE:indexptr-BASE+wb],'little')
                                signed_index=captured_index if captured_index<1<<(wb*8-1) else captured_index-(1<<(wb*8))
                                early=profile in (7,8)
                                typeerror=profile==2 or signed_index!=0 or profile==1 and size==0
                                accepted=not early and not typeerror and profile!=1 and size<=256
                                allocerror=accepted and profile in (3,6)
                                expected=bytearray(memory[:0x7000])
                                state.clear()
                                state.update(size=size,profile=profile,source=sourceptr,destination=destination,old=old,index=signed_index&MASK,typeerror=typeerror,accepted=accepted,allocerror=allocerror,expected=expected,lookups=0,allocations=0,detail=(arch,wb,size,profile,sa,da,alias))
                                machine.mem_write(BASE,bytes(memory))
                                writes.clear()
                                if arch=='amd64':
                                    machine.mem_write(STACK+8,STOP.to_bytes(8,'little'))
                                    machine.reg_write(xr.UC_X86_REG_RDI,BASE)
                                    machine.reg_write(xr.UC_X86_REG_RSP,STACK+8)
                                    machine.reg_write(xr.UC_X86_REG_MXCSR,0x3f95)
                                    saved=[getattr(xr,'UC_X86_REG_'+name) for name in ('RBX','RBP','R12','R13','R14','R15')]
                                else:
                                    machine.reg_write(ar.UC_ARM64_REG_X0,BASE)
                                    machine.reg_write(ar.UC_ARM64_REG_SP,STACK)
                                    machine.reg_write(ar.UC_ARM64_REG_LR,STOP)
                                    machine.reg_write(ar.UC_ARM64_REG_FPCR,0x400000)
                                    machine.reg_write(ar.UC_ARM64_REG_FPSR,0x15)
                                    saved=[getattr(ar,'UC_ARM64_REG_X'+str(reg)) for reg in range(19,30)]
                                for reg in saved:
                                    machine.reg_write(reg,0xabcdef000+reg)
                                try:
                                    machine.emu_start(CODE,STOP,count=3000)
                                except Exception as error:
                                    raise RuntimeError(('execution',state['detail'])) from error
                                detail=state['detail']
                                require(state['lookups']==int(not early) and state['allocations']==int(accepted),('call counts',detail,state['lookups'],state['allocations']))
                                require(bytes(machine.mem_read(BASE,0x7000))==bytes(expected),('VM image',detail))
                                if not (not early and typeerror or allocerror):
                                    result=machine.reg_read(xr.UC_X86_REG_RAX if arch=='amd64' else ar.UC_ARM64_REG_X0)
                                    sp=machine.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)
                                    require(result==int(accepted) and sp==STACK+(16 if arch=='amd64' else 0),('return/frame',detail,result,sp))
                                    for reg in saved:
                                        require(machine.reg_read(reg)==0xabcdef000+reg,('nonvolatile register',detail,reg))
                                    if arch=='arm64':
                                        require(machine.reg_read(ar.UC_ARM64_REG_LR)==STOP,('LR',detail))
                                if arch=='amd64':
                                    require(machine.reg_read(xr.UC_X86_REG_MXCSR)==0x3f95,('FP',detail))
                                else:
                                    require(machine.reg_read(ar.UC_ARM64_REG_FPCR)==0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR)==0x15,('FP',detail))
                                for address,width in writes:
                                    valid=STACK-96<=address and address+width<=STACK+16
                                    if accepted and not allocerror:
                                        valid=valid or destination<=address and address+width<=destination+8
                                        valid=valid or NODE+8<=address and address+width<=NODE+16+size
                                        valid=valid or TYPE<=address and address+width<=TYPE+4
                                    require(valid,('native write span',detail,address,width))
                                checks+=1
        print('PASS mapped cons',arch,checks,'ABI/guard/Type/image/frame checks',flush=True)
