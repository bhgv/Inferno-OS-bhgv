#!/usr/bin/env python3
"""Check every active AMD64/ARM64 tail entry, pointer arguments, stack and LR."""
import ctypes as C
import importlib.util
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RDX, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_LR, UC_ARM64_REG_SP, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]  # Production source tree.
CODE = 0x10000  # Generated stub and return sentinel page.
STOP = CODE+0x1000  # Original caller's return address.
STACK = CODE+0x10000  # Borrowed native stack with surrounding canaries.
BASE = 0x100000000  # Synthetic REG/data image above 4 GiB.
EXECUTOR = BASE+0x8000  # High-address shared C entry model.
MASK = (1<<64)-1  # Native pointer modulus.
SPEC = importlib.util.spec_from_file_location('coverage',ROOT/'libinterp/tests/jit-coverage.py')
COVERAGE = importlib.util.module_from_spec(SPEC)
exec(compile((ROOT/'libinterp/tests/jit-coverage.py').read_bytes(),
             str(ROOT/'libinterp/tests/jit-coverage.py'), 'exec'), COVERAGE.__dict__)
NAMES = COVERAGE.opnames()  # Real enum slots, including the two inactive ones.


# Require each once-evaluated invariant even with Python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='jit-dispatch-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('JIT_DISPATCH_ARCHES','amd64 arm64').split():
        header = Path(os.environ.get('JIT_DISPATCH_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output requests sizing only.
#include "isa.h"
#include "'''+str(header)+'''"


/* The complete bridge has no WORD-dependent native layout or private frame. */
int emit(u32int *out,int cap,int op,uint64_t executor,uint64_t fast)
{
    return leafbridgeemit(out,cap,(void*)(uintptr_t)executor,op,(void*)(uintptr_t)fast);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32),C.c_int,C.c_int,C.c_uint64,C.c_uint64]
        machine = Uc(UC_ARCH_X86 if arch=='amd64' else UC_ARCH_ARM64,UC_MODE_64 if arch=='amd64' else 0)
        machine.mem_map(CODE,0x3000)
        machine.mem_map(STACK,0x4000)
        machine.mem_map(BASE,0x1000)
        machine.mem_map(EXECUTOR,0x1000)
        machine.mem_write(EXECUTOR,b'\xc3' if arch=='amd64' else (0xd65f03c0).to_bytes(4,'little'))
        state, calls, reads, writes = {}, [], [], []


        # Model a shared C function entered with the original caller's return context.
        def executor(machine,address,size,context):
            if address!=EXECUTOR: return
            if arch=='amd64':
                actual = (machine.reg_read(UC_X86_REG_RDI),machine.reg_read(UC_X86_REG_RSI),machine.reg_read(UC_X86_REG_RDX))
                require(machine.reg_read(UC_X86_REG_RSP)==STACK+0x2008,'tail target gained a native frame')
                require(int.from_bytes(machine.mem_read(STACK+0x2008,8),'little')==STOP,'original x86 return address')
                machine.reg_write(UC_X86_REG_RAX,state['result'])
            else:
                actual = (machine.reg_read(UC_ARM64_REG_X0),machine.reg_read(UC_ARM64_REG_X1),machine.reg_read(UC_ARM64_REG_X2))
                require(machine.reg_read(UC_ARM64_REG_SP)==STACK+0x2000 and machine.reg_read(UC_ARM64_REG_LR)==STOP,'tail target lost original SP/LR')
                machine.reg_write(UC_ARM64_REG_X0,state['result'])
            calls.append(actual)


        # A tail entry may not inspect operand memory or build a stack transaction.
        def memory_read(machine,access,address,size,value,context):
            if BASE<=address<BASE+4096: reads.append((address,size))


        # No guest writes are permitted, even private stack writes.
        def memory_write(machine,access,address,size,value,context):
            writes.append((address,size))


        machine.hook_add(UC_HOOK_CODE,executor,begin=EXECUTOR,end=EXECUTOR)
        machine.hook_add(UC_HOOK_MEM_READ,memory_read)
        machine.hook_add(UC_HOOK_MEM_WRITE,memory_write)
        checks = 0
        for op in list(range(-8,len(NAMES)+8))+[-0x80000000,0x7fffffff]:
            active = 0<op<len(NAMES) and NAMES[op]!='IECLR'
            for fast in (0,BASE+0x1230,0x123456789abcde00,MASK-1,MASK):
                out = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                n = lib.emit(out,128,op,EXECUTOR,fast)
                accepted = active and fast!=MASK
                require((n>0)==accepted and n<=128,(arch,op,fast,'sizing'))
                if not accepted:
                    require(list(out)==[0xa5a5a5a5]*129,(arch,op,fast,'rejected output'))
                    checks+=1
                    continue
                require(lib.emit(None,128,op,EXECUTOR,fast)==n and out[n]==0xa5a5a5a5,'size/canary')
                for cap in range(n+1):
                    private = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                    result = lib.emit(private,cap,op,EXECUTOR,fast)
                    require(result==(n if cap==n else 0),'capacity result')
                    require(bytes(private)[:n*4]==bytes(out)[:n*4] if result else list(private)==[0xa5a5a5a5]*129,'no partial output')
                    require(private[n]==0xa5a5a5a5,'capacity neighbor')
                for invalid in (0,MASK): require(lib.emit(None,128,op,invalid,fast)==0,'invalid executor')
                machine.mem_write(CODE,bytes(out)[:n*4])
                machine.ctl_remove_cache(CODE,CODE+512)
                for wb in (4,8):
                    for register in (0,1,BASE,BASE+7,0x123456789abcde00,MASK):
                        for result in (0,1):
                            image = b'\xa5'*4096
                            stackimage = bytearray(b'\x5a'*0x4000)
                            state.update(result=result)
                            machine.mem_write(BASE,image)
                            if arch=='amd64':
                                stackimage[0x2008:0x2010]=STOP.to_bytes(8,'little')
                                machine.reg_write(UC_X86_REG_RDI,register)
                                machine.reg_write(UC_X86_REG_RSP,STACK+0x2008)
                                machine.reg_write(UC_X86_REG_MXCSR,0x1f84)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0,register)
                                machine.reg_write(UC_ARM64_REG_SP,STACK+0x2000)
                                machine.reg_write(UC_ARM64_REG_LR,STOP)
                                machine.reg_write(UC_ARM64_REG_FPCR,0)
                                machine.reg_write(UC_ARM64_REG_FPSR,2)
                            machine.mem_write(STACK,bytes(stackimage))
                            calls.clear(); reads.clear(); writes.clear()
                            machine.emu_start(CODE,STOP,count=100)
                            detail = (arch,op,fast,wb,register,result)
                            require(calls==[(register,op,fast)],(detail,'C arguments'))
                            require(machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)==result,(detail,'C return'))
                            require(machine.reg_read(UC_X86_REG_RSP if arch=='amd64' else UC_ARM64_REG_SP)==STACK+(0x2010 if arch=='amd64' else 0x2000),(detail,'caller stack'))
                            require(not writes and not reads and bytes(machine.mem_read(BASE,4096))==image,(detail,'VM/stack access'))
                            require(bytes(machine.mem_read(STACK,0x4000))==stackimage,(detail,'stack image'))
                            require(machine.reg_read(UC_X86_REG_MXCSR)==0x1f84 if arch=='amd64' else machine.reg_read(UC_ARM64_REG_FPCR)==0 and machine.reg_read(UC_ARM64_REG_FPSR)==2,(detail,'FP state'))
                            checks+=1
        print('PASS',arch,'complete tail entries/arguments/stack/capacity:',checks,'cases',flush=True)
