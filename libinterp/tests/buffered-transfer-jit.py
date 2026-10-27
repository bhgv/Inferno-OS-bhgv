#!/usr/bin/env python3
"""Check native transfer/call ABI independently of the C geometry implementation."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RDX, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR, UC_X86_REG_RBX, UC_X86_REG_RBP, UC_X86_REG_R12, UC_X86_REG_R13, UC_X86_REG_R14, UC_X86_REG_R15, UC_X86_REG_RCX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_R10, UC_X86_REG_R11
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X19, UC_ARM64_REG_X20, UC_ARM64_REG_X21, UC_ARM64_REG_X22, UC_ARM64_REG_X23, UC_ARM64_REG_X24, UC_ARM64_REG_X25, UC_ARM64_REG_X26, UC_ARM64_REG_X27, UC_ARM64_REG_X28, UC_ARM64_REG_X29, UC_ARM64_REG_SP, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]  # Real emitter source root.
CODE = 0x10000  # Code and helper return trampoline.
STOP = CODE+0x1000  # Native leaf return sentinel.
HELPER = CODE+0x2000  # Independently modeled pure C preflight address.
STACK = CODE+0x10000  # Private aligned helper/transaction stack.
BASE = 0x100000000  # Guest REG, scalar ring fields and payload images above 4 GiB.
MASK = (1<<64)-1  # Native pointer modulus.
PATTERN = bytes((i*37+11)&255 for i in range(0x4000))  # Immutable complete-image canary.


# Keep all invariants active under Python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Encode an owned guest value at any operand alignment.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1<<(8*width))-1)).to_bytes(width,'little')


with tempfile.TemporaryDirectory(prefix='buffered-jit-') as temporary:
    td = Path(temporary)
    for arch in ('amd64','arm64'):
        source = td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output requests sizing only.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Borrow out for cap units and capture a chosen pure helper identity. */
int emit(u32int *out,int cap,int wb,int receive,int distant,uint64_t helper)
{
    LeafLayout a={0};
    a.wordbytes=wb;
    a.pc=0;
    a.s=distant ? 512 : 8;
    a.d=distant ? 520 : 16;
    a.m=distant ? 528 : 24;
    a.channeltransferentry=(void*)(uintptr_t)helper;
    return leafemit(out,cap,&a,receive ? IRECV : ISEND);
}
''')
        command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        library = td/(arch+'.so')
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(library)],check=True)
        lib = C.CDLL(str(library))
        lib.emit.argtypes = [C.POINTER(C.c_uint32),C.c_int,C.c_int,C.c_int,C.c_int,C.c_uint64]
        machine = Uc(UC_ARCH_X86 if arch=='amd64' else UC_ARCH_ARM64,UC_MODE_64 if arch=='amd64' else 0)
        machine.mem_map(CODE,0x3000)
        machine.mem_map(STACK,0x4000)
        machine.mem_map(BASE,0x4000)
        machine.mem_write(HELPER,b'\xc3' if arch=='amd64' else (0xd65f03c0).to_bytes(4,'little'))
        writes = []
        calls = []
        state = {}


        # Stage only the transaction and clobber legal caller-saved registers as a C call may.
        def helper(machine,address,size,context):
            if address!=HELPER:
                return
            if arch=='amd64':
                reg = machine.reg_read(UC_X86_REG_RDI)
                receive = machine.reg_read(UC_X86_REG_RSI)
                transaction = machine.reg_read(UC_X86_REG_RDX)
                require(machine.reg_read(UC_X86_REG_RSP)%16==8,'SysV helper stack alignment')
            else:
                reg = machine.reg_read(UC_ARM64_REG_X0)
                receive = machine.reg_read(UC_ARM64_REG_X1)
                transaction = machine.reg_read(UC_ARM64_REG_X2)
                require(machine.reg_read(UC_ARM64_REG_SP)%16==0,'AAPCS64 helper stack alignment')
            calls.append((reg,receive))
            require(reg==BASE and receive==state['receive'],'helper arguments')
            if state['accept']:
                image = bytearray(64)
                for offset,value in enumerate(state['transaction']): put(image,offset*8,value)
                machine.mem_write(transaction,bytes(image))
            if arch=='amd64':
                for register in (UC_X86_REG_RDI,UC_X86_REG_RSI,UC_X86_REG_RDX,UC_X86_REG_RCX,UC_X86_REG_R8,UC_X86_REG_R9,UC_X86_REG_R10,UC_X86_REG_R11):
                    machine.reg_write(register,0xdeadbeef)
                machine.reg_write(UC_X86_REG_RAX,int(state['accept']))
            else:
                # Unicorn register constants for x0..x28 are contiguous.
                for register in range(UC_ARM64_REG_X0,UC_ARM64_REG_X0+19): machine.reg_write(register,0xdeadbeef)
                machine.reg_write(UC_ARM64_REG_X0,int(state['accept']))


        # Restrict visible writes to expected VM/ring/payload bytes; private stack is separate.
        def write(machine,access,address,size,value,context):
            if BASE<=address<BASE+0x4000: writes.append((address,size))


        machine.hook_add(UC_HOOK_CODE,helper,begin=HELPER,end=HELPER)
        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        checks = 0
        preserved = (UC_X86_REG_RBX,UC_X86_REG_RBP,UC_X86_REG_R12,UC_X86_REG_R13,UC_X86_REG_R14,UC_X86_REG_R15) if arch=='amd64' else (UC_ARM64_REG_X19,UC_ARM64_REG_X20,UC_ARM64_REG_X21,UC_ARM64_REG_X22,UC_ARM64_REG_X23,UC_ARM64_REG_X24,UC_ARM64_REG_X25,UC_ARM64_REG_X26,UC_ARM64_REG_X27,UC_ARM64_REG_X28,UC_ARM64_REG_X29)
        for wb in (4,8):
            for receive in (0,1):
                for distant in (0,1):
                    s,d,m = (512,520,528) if distant else (8,16,24)
                    out = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                    n = lib.emit(out,128,wb,receive,distant,HELPER)
                    require(0<n<=128 and out[n]==0xa5a5a5a5 and lib.emit(None,128,wb,receive,distant,HELPER)==n,'sizing')
                    for capacity in range(n+1):
                        private = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                        result = lib.emit(private,capacity,wb,receive,distant,HELPER)
                        require(result==(n if capacity==n else 0),'capacity result')
                        require(bytes(private)[:n*4]==bytes(out)[:n*4] if result else list(private)==[0xa5a5a5a5]*129,'no partial publication')
                        require(private[n]==0xa5a5a5a5,'capacity canary')
                    for entry in (0,MASK): require(lib.emit(None,128,wb,receive,distant,entry)==0,'invalid helper identity')
                    machine.mem_write(CODE,bytes(out)[:n*4])
                    machine.ctl_remove_cache(CODE,CODE+512)
                    for count in range(257):
                        for alignment in range(8):
                            for alias in range(5):
                                for accept in (False,True):
                                    image = bytearray(PATTERN)
                                    source = BASE+0x1000+alignment
                                    destination = BASE+0x2000+alignment if alias==0 else source+1 if alias==1 else source-1 if alias==2 else source if alias==3 else BASE+0x1800
                                    if count==0: source,destination=0,MASK
                                    put(image,0,BASE+0x3900)
                                    put(image,s,BASE+0x700)
                                    put(image,d,BASE+0x800)
                                    put(image,m,1)
                                    put(image,0x900,2,4)
                                    put(image,0x904,3,4)
                                    state.update(receive=receive,accept=accept,transaction=(source,destination,count,BASE+0x900,BASE+0x904,0,2 if receive else 4,BASE+0x908))
                                    expected = bytearray(image)
                                    if accept:
                                        payload = bytes(image[source-BASE:source-BASE+count]) if count else b''
                                        if count: expected[destination-BASE:destination-BASE+count]=payload
                                        put(expected,0x900,0,4)
                                        put(expected,0x904,2 if receive else 4,4)
                                        put(expected,s,source)
                                        put(expected,d,destination)
                                        put(expected,m,BASE+0x908)
                                    machine.mem_write(BASE,bytes(image))
                                    for i,register in enumerate(preserved): machine.reg_write(register,0x100000000+i)
                                    if arch=='amd64':
                                        stack = STACK+0x3008
                                        machine.reg_write(UC_X86_REG_RDI,BASE)
                                        machine.reg_write(UC_X86_REG_RSP,stack)
                                        machine.mem_write(stack,STOP.to_bytes(8,'little'))
                                        machine.reg_write(UC_X86_REG_MXCSR,0x1f84)
                                    else:
                                        stack = STACK+0x3000
                                        machine.reg_write(UC_ARM64_REG_X0,BASE)
                                        machine.reg_write(UC_ARM64_REG_LR,STOP)
                                        machine.reg_write(UC_ARM64_REG_SP,stack)
                                        machine.reg_write(UC_ARM64_REG_FPCR,0)
                                        machine.reg_write(UC_ARM64_REG_FPSR,2)
                                    calls.clear()
                                    writes.clear()
                                    machine.emu_start(CODE,STOP,count=4000)
                                    detail = (arch,wb,receive,distant,count,alignment,alias,accept)
                                    require(machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)==int(accept),(detail,'return'))
                                    require(bytes(machine.mem_read(BASE,0x4000))==expected,(detail,'whole image'))
                                    require(calls==[(BASE,receive)],(detail,'one preflight call'))
                                    require(machine.reg_read(UC_X86_REG_RSP if arch=='amd64' else UC_ARM64_REG_SP)==stack+(8 if arch=='amd64' else 0),(detail,'stack restore'))
                                    require(all(machine.reg_read(register)==0x100000000+i for i,register in enumerate(preserved)),(detail,'callee-saved registers'))
                                    require(machine.reg_read(UC_X86_REG_MXCSR)==0x1f84 if arch=='amd64' else machine.reg_read(UC_ARM64_REG_FPCR)==0 and machine.reg_read(UC_ARM64_REG_FPSR)==2,(detail,'FP state'))
                                    if not accept: require(not writes,(detail,'decline wrote VM'))
                                    checks+=1
        print('PASS',arch,'buffered native copy/publication/helper ABI:',checks,'cases',flush=True)
