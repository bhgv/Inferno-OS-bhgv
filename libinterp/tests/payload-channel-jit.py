#!/usr/bin/env python3
"""Execute AMD64/ARM64 payload-channel leaves and raw metadata publication around hostile runtime allocator callbacks."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT = Path(__file__).resolve().parents[2]
BASE, CODE, STOP, HELPER = 0x100000000, 0x10000, 0x11000, 0x123456780000
STACK, CHANNEL = BASE+0x7800, BASE+0x5000
MASK = (1<<64)-1


# Write native cells independently of the production emitter's instruction encoding.
def put(memory, offset, value, width=8):
    memory[offset:offset+width] = (value & ((1<<(width*8))-1)).to_bytes(width, 'little')


# Keep every oracle active under optimized Python execution.
def require(condition, detail):
    if not condition:
        raise RuntimeError(tuple({k:v for k,v in item.items() if k!='before'} if isinstance(item,dict) else item for item in detail))


with tempfile.TemporaryDirectory(prefix='payload-channel-jit-') as directory:
    td = Path(directory)
    for arch in os.environ.get('CHANNEL_ARCHES','amd64 arm64').split():
        source = td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null identity for bounded emitter queries.
#include "isa.h"
#include HEADER


/* Emit raw/mapped constructors with explicit metadata offsets and a high allocator identity. */
int
emit(u32int *out, int cap, int wb, int op, int bad)
{
    int ops[]={INEWCM, INEWCMP};
    LeafLayout a={0};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24;
    a.rawchannelentry=bad==1 ? nil : bad==2 ? (void*)-1 : (void*)0x123456780000ULL;
    a.mappedchannelentry=a.rawchannelentry;
    a.channelmid=bad==3 ? 8 : 40;
    a.module=bad==3 ? 3 : 32;
    return leafemit(out,cap,&a,ops[op]);
}


/* Export opcode identities without duplicating the ISA numbering in Python. */
int
opcode(int op)
{
    int ops[]={INEWCM, INEWCMP};
    return ops[op];
}
'''.replace('HEADER', '"'+arch+'-emit.h"'))
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC','cc'))
        command += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*4
        machine = Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x2000)
        machine.mem_map(HELPER,0x1000)
        machine.mem_map(BASE,0x8000)
        machine.mem_write(HELPER,b'\xc3' if arch=='amd64' else bytes.fromhex('c0035fd6'))
        state, writes = {}, []


        # Verify arguments and delayed publication, then clobber ordinary volatile registers.
        def allocator(uc, address, size, context):
            if address != HELPER:
                return
            state['calls'] += 1
            first = uc.reg_read(xr.UC_X86_REG_RDI if arch=='amd64' else ar.UC_ARM64_REG_X0)
            second = uc.reg_read(xr.UC_X86_REG_RSI if arch=='amd64' else ar.UC_ARM64_REG_X1)
            third = uc.reg_read(xr.UC_X86_REG_RDX if arch=='amd64' else ar.UC_ARM64_REG_X2)
            value=second if state['mapped'] else first
            length=third if state['mapped'] else second
            sp = uc.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)
            require(state['valid'] and state['calls']==1, ('unexpected call',state))
            require(value==state['value'] and length==state['length'] and (not state['mapped'] or first==BASE+0x2800), ('arguments',state,first,second,third))
            require(sp%16==(8 if arch=='amd64' else 0), ('stack alignment',state,sp))
            require(bytes(uc.mem_read(BASE,0x7000))==state['before'], ('early publication',state))
            if state['failure']:
                uc.emu_stop()
                return
            uc.mem_write(CHANNEL+40,(BASE+0x6000).to_bytes(8,'little') if state['mapped'] else bytes(state['wb']))
            if arch=='amd64':
                for name in ('RCX','RDX','RSI','RDI','R8','R9','R10','R11'):
                    uc.reg_write(getattr(xr,'UC_X86_REG_'+name),0xbad00000+len(name))
                uc.reg_write(xr.UC_X86_REG_RAX,CHANNEL)
            else:
                for reg in range(1,19):
                    uc.reg_write(getattr(ar,'UC_ARM64_REG_X'+str(reg)),0xbad00000+reg)
                uc.reg_write(ar.UC_ARM64_REG_X0,CHANNEL)


        machine.hook_add(UC_HOOK_CODE,allocator)
        machine.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,address,size,value,context:writes.append((address,size)))
        checks = 0
        for wb in (4,8):
            for op in range(2):
                output = (C.c_uint32*128)(*([0xa5a5a5a5]*128))
                units = lib.emit(output,128,wb,op,0)
                require(0<units<=128,('sizing',arch,wb,op,units))
                code = bytes(output)[:units*4]
                machine.mem_write(CODE,code)
                machine.ctl_remove_cache(CODE,CODE+0x1000)
                for bad in (1,2,3):
                    require(lib.emit(output,128,wb,op,bad)==0,('invalid allocator',arch,wb,op,bad))
                untouched = (C.c_uint32*128)(*([0xa5a5a5a5]*128))
                require(lib.emit(untouched,units-1,wb,op,0)==0 and bytes(untouched)==b'\xa5'*512,('capacity transaction',arch,wb,op))
                for value in (0,1,3,0x7fffffff,0x80000000,0x100000001,MASK,1<<63):
                    for sa in range(4):
                        for ma in range(4):
                            for da in range(4):
                                for profile in range(12):
                                    for failure in (0,1):
                                        memory = bytearray(b'\xa5'*0x8000)
                                        sourceptr, middle, destination = BASE+0x400+sa, BASE+0x800+ma, BASE+0x1000+da
                                        old = MASK
                                        if profile==1:
                                            middle = destination
                                        if profile==2:
                                            old = CHANNEL
                                        if profile==3:
                                            destination = 0
                                        if profile==4:
                                            destination = MASK-3
                                        if profile==5:
                                            middle = 0
                                        if profile==6:
                                            middle = MASK-3
                                        if profile==7:
                                            old = 0
                                        if profile==8:
                                            sourceptr=0
                                        if profile==9:
                                            sourceptr=MASK-3
                                        if profile==10:
                                            sourceptr=middle
                                        if profile==11:
                                            sourceptr=destination
                                        if BASE<=sourceptr<BASE+0x7000:
                                            put(memory,sourceptr-BASE,value,wb)
                                        if BASE<=middle<BASE+0x7000:
                                            put(memory,middle-BASE,(0,1,3,MASK)[ma],wb)
                                        if BASE<=destination<BASE+0x7000:
                                            put(memory,destination-BASE,old)
                                        put(memory,0,BASE+0x2000)
                                        put(memory,8,sourceptr)
                                        put(memory,16,destination)
                                        put(memory,24,middle)
                                        put(memory,32,BASE+0x2800)
                                        captured = int.from_bytes(memory[middle-BASE:middle-BASE+wb],'little') if BASE<=middle<BASE+0x7000 else 0
                                        length = 0 if middle==destination else captured
                                        captured_value=int.from_bytes(memory[sourceptr-BASE:sourceptr-BASE+wb],'little') if BASE<=sourceptr<BASE+0x7000 else 0
                                        signed_value=captured_value if captured_value < 1<<(wb*8-1) else captured_value-(1<<(wb*8))
                                        valid = profile in (0,1,10,11) and length<=0x7fffffff and (op==1 or signed_value>=0 and signed_value<=0x7fffffff)
                                        failed=bool(failure or op==1 and signed_value not in (0,1))
                                        expected = bytearray(memory[:0x7000])
                                        if valid and not failed:
                                            put(expected,0x5028,signed_value if op==0 else BASE+0x6000,wb if op==0 else 8)
                                            put(expected,destination-BASE,CHANNEL)
                                        state.clear()
                                        state.update(wb=wb,valid=valid,failure=failed,calls=0,mapped=op,value=signed_value&MASK,length=length,before=bytes(memory[:0x7000]),detail=(arch,wb,op,value,sa,ma,da,profile,failure))
                                        machine.mem_write(BASE,bytes(memory))
                                        writes.clear()
                                        if arch=='amd64':
                                            machine.mem_write(STACK+8,STOP.to_bytes(8,'little'))
                                            machine.reg_write(xr.UC_X86_REG_RSP,STACK+8)
                                            machine.reg_write(xr.UC_X86_REG_RDI,BASE)
                                            machine.reg_write(xr.UC_X86_REG_MXCSR,0x3f95)
                                            preserved = ('RBX','RBP','R12','R13','R14','R15')
                                            for i,name in enumerate(preserved):
                                                machine.reg_write(getattr(xr,'UC_X86_REG_'+name),0xabcd0000+i)
                                        else:
                                            machine.reg_write(ar.UC_ARM64_REG_SP,STACK)
                                            machine.reg_write(ar.UC_ARM64_REG_LR,STOP)
                                            machine.reg_write(ar.UC_ARM64_REG_X0,BASE)
                                            machine.reg_write(ar.UC_ARM64_REG_FPCR,0x400000)
                                            machine.reg_write(ar.UC_ARM64_REG_FPSR,0x15)
                                            preserved = tuple(range(19,30))
                                            for i,reg in enumerate(preserved):
                                                machine.reg_write(getattr(ar,'UC_ARM64_REG_X'+str(reg)),0xabcd0000+i)
                                        try:
                                            machine.emu_start(CODE,STOP,count=200)
                                        except Exception as error:
                                            pc=machine.reg_read(xr.UC_X86_REG_RIP if arch=='amd64' else ar.UC_ARM64_REG_PC)
                                            raise RuntimeError(('execution',state['detail'],hex(pc))) from error
                                        require(state['calls']==int(valid),('call count',state))
                                        require(bytes(machine.mem_read(BASE,0x7000))==bytes(expected),('VM image',state))
                                        if not (valid and failed):
                                            result = machine.reg_read(xr.UC_X86_REG_RAX if arch=='amd64' else ar.UC_ARM64_REG_X0)
                                            sp = machine.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)
                                            require(result==int(valid) and sp==STACK+(16 if arch=='amd64' else 0),('return/stack',state,result,sp))
                                            for i,reg in enumerate(preserved):
                                                actual = machine.reg_read(getattr(xr,'UC_X86_REG_'+reg) if arch=='amd64' else getattr(ar,'UC_ARM64_REG_X'+str(reg)))
                                                require(actual==0xabcd0000+i,('nonvolatile register',state,reg))
                                        if arch=='amd64':
                                            require(machine.reg_read(xr.UC_X86_REG_MXCSR)==0x3f95,('FP state',state))
                                        else:
                                            require(machine.reg_read(ar.UC_ARM64_REG_FPCR)==0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR)==0x15,('FP state',state))
                                            if not (valid and failed):
                                                require(machine.reg_read(ar.UC_ARM64_REG_LR)==STOP,('LR restoration',state))
                                        for address,size in writes:
                                            require(STACK-64<=address and address+size<=STACK+16 or valid and not failed and (destination<=address and address+size<=destination+8 or op==0 and CHANNEL+40<=address and address+size<=CHANNEL+40+wb),('write boundary',state,address,size))
                                        checks += 1
        print('PASS payload channels',arch,checks,'ABI/image/width/guard/failure checks',flush=True)
