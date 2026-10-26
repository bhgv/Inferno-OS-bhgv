#!/usr/bin/env python3
"""Execute 24 production integer leaves with independent slot, zero and arithmetic oracles."""
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RSP, UC_X86_REG_RAX, UC_X86_REG_RIP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_PC, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x100000000
CODE = 0x10000
STOP = CODE+0x1000
MASK = (1 << 64)-1
OPS = tuple(f+s for s in ('B','W','L') for f in ('ADD','SUB','MUL','AND','OR','XOR'))
OPS += ('DIVB','MODB','DIVW','MODW','DIVL','MODL')


# Keep all fixture predicates active under Python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Store independent little-endian fields without host alignment assumptions.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (8*width))-1)).to_bytes(width, 'little')


# Interpret one signed input independently of emitted widening instructions.
def signed(raw, width):
    raw &= (1 << (8*width))-1
    return raw-(1 << (8*width)) if raw >> (8*width-1) else raw


with tempfile.TemporaryDirectory(prefix='integer-slot-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('INTEGER_SLOT_ARCHES', 'amd64 arm64').split():
        source = td/(arch+'.c')
        header = Path(os.environ.get('INTEGER_SLOT_EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null address.
#include "isa.h"
#include '''+json.dumps(str(header))+'''


/* Use synthetic native-pointer fields independently of the host REG struct. */
int emit(u32int *out, int cap, int wb, int op)
{
    LeafLayout a={0};
    int ops[]={'''+','.join('I'+op for op in OPS)+'''};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24;
    return leafemit(out,cap,&a,ops[op]);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),
                       '-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.c_int, C.c_int]
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x2000)
        allowed = set()
        writes = []
        label = None


        # Reject reads outside the operands justified by the current arithmetic/error case.
        def onread(machine, access, address, size, value, context):
            if BASE <= address and address+size <= BASE+32 or address == CODE+0x2800:
                return
            require((address,size) in allowed, (label,'unexpected read',hex(address),size))


        # Detect stores even when they would leave the final bytes unchanged.
        def onwrite(machine, access, address, size, value, context):
            writes.append((address,size))

        machine.hook_add(UC_HOOK_MEM_READ,onread)
        machine.hook_add(UC_HOOK_MEM_WRITE,onwrite)
        checks = 0
        for wb in (4,8):
            widths = (1,)*6+(wb,)*6+(8,)*6+(1,1,wb,wb,8,8)
            for op,name in enumerate(OPS):
                if os.environ.get('INTEGER_SLOT_OP') and op != int(os.environ['INTEGER_SLOT_OP']):
                    continue
                out = (C.c_uint32*128)()
                n = lib.emit(out,128,wb,op)
                require(0 < n <= 128 and lib.emit(None,128,wb,op) == n, (arch,wb,name,'sizing',n))
                for cap in range(n+1):
                    probe = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    actual = lib.emit(probe,cap,wb,op)
                    end = n*4 if cap == n else 0
                    require(actual == (n if cap == n else 0) and
                            bytes(probe)[end:] == bytes([0xa5])*(512-end),
                            (arch,wb,name,'capacity/canary',cap))
                    if cap == n:
                        require(bytes(probe)[:end] == bytes(out)[:end], (arch,name,'image agreement'))
                machine.mem_write(CODE,bytes(out)[:n*4])
                machine.ctl_remove_cache(CODE,CODE+512)


                # Execute with complete image/REG, exact read/store-span and FP invariants.
                def execute(image, expected, result, mode, detail, reads):
                    global allowed,label
                    allowed = reads
                    label = (arch,wb,name,mode,detail)
                    machine.mem_write(BASE,bytes(image))
                    writes.clear()
                    if arch == 'amd64':
                        machine.reg_write(UC_X86_REG_RDI,BASE)
                        machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                        machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'))
                        machine.reg_write(UC_X86_REG_MXCSR,0x1f84 | mode << 13)
                    else:
                        machine.reg_write(UC_ARM64_REG_X0,BASE)
                        machine.reg_write(UC_ARM64_REG_LR,STOP)
                        machine.reg_write(UC_ARM64_REG_FPCR,mode << 22)
                        machine.reg_write(UC_ARM64_REG_FPSR,0x84)
                    machine.emu_start(CODE,STOP,count=1000)
                    require(machine.reg_read(UC_X86_REG_RIP if arch == 'amd64' else UC_ARM64_REG_PC) == STOP,
                            (label,'instruction budget'))
                    require(machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0) == result,
                            (label,'return'))
                    require(bytes(machine.mem_read(BASE,len(image))) == bytes(expected), (label,'image'))
                    if not result:
                        require(not writes, (label,'decline stores',writes))
                    else:
                        destination = int.from_bytes(image[16:24],'little')
                        require(all(destination <= address and address+size <= destination+widths[op]
                                    for address,size in writes), (label,'result span',writes))
                    if arch == 'amd64':
                        require(machine.reg_read(UC_X86_REG_MXCSR) == 0x1f84 | mode << 13, (label,'MXCSR'))
                    else:
                        require(machine.reg_read(UC_ARM64_REG_FPCR) == mode << 22 and
                                machine.reg_read(UC_ARM64_REG_FPSR) == 0x84, (label,'FP state'))



                # Use disjoint unaligned inputs; success placements can alias either one.
                def setup(shift=0):
                    image = bytearray((i*37+11) & 255 for i in range(1024))
                    put(image,0,0x12345678)
                    put(image,8,BASE+0x100+shift)
                    put(image,16,BASE+0x300+shift)
                    put(image,24,BASE+0x200+shift)
                    put(image,0x100+shift,1,widths[op])
                    put(image,0x200+shift,1,widths[op])
                    return image

                width = widths[op]
                division = op >= 18
                for side in range(3):
                    for address in [0]+[MASK-i for i in range(width)]:
                        for shift in range(8):
                            for mode in range(4):
                                image = setup(shift)
                                put(image,(8,24,16)[side],address)
                                reads = {(BASE+0x100+shift,width)} if division and side != 0 else set()
                                execute(image,image,0,mode,('slot',side,address,shift),reads)
                                checks += 1
                for mode in range(4):
                    image = setup()
                    if not division:
                        put(image,8,1)
                    put(image,24,1)
                    put(image,16,MASK)
                    execute(image,image,0,mode,'output-priority',
                            {(BASE+0x100,width)} if division else set())
                    checks += 1
                if division:
                    for shift in range(8):
                        for identity in (0,1,MASK,MASK-1):
                            for mode in range(4):
                                image = setup(shift)
                                put(image,0x100+shift,0,width)
                                put(image,24,identity)
                                put(image,16,identity)
                                execute(image,image,0,mode,('zero-nonaccess',identity,shift),
                                        {(BASE+0x100+shift,width)})
                                checks += 1
                patterns = (0,1,127,128,255,0x7fffffff,0x80000000,
                            0x7fffffffffffffff,0x8000000000000000,MASK)
                for source in patterns:
                    for middle in patterns:
                        for shift in range(8):
                            for placement in range(6):
                                image = setup(shift)
                                sourcepos = 0x100+shift
                                middlepos = 0x200+shift
                                destination = (0x300+shift,sourcepos,sourcepos-1,sourcepos+1,middlepos,middlepos+1)[placement]
                                put(image,16,BASE+destination)
                                put(image,sourcepos,source,width)
                                put(image,middlepos,middle,width)
                                mask = (1 << (8*width))-1
                                right,left = source & mask,middle & mask
                                reads = {(BASE+sourcepos,width)}
                                valid = not division or right != 0
                                if valid:
                                    reads.add((BASE+middlepos,width))
                                    if not division:
                                        alu = op % 6
                                        wanted = (left+right if alu == 0 else left-right if alu == 1 else
                                                  left*right if alu == 2 else left & right if alu == 3 else
                                                  left | right if alu == 4 else left ^ right)
                                    else:
                                        if width != 1:
                                            right,left = signed(right,width),signed(left,width)
                                        quotient = abs(left)//abs(right)
                                        if (left < 0) != (right < 0):
                                            quotient = -quotient
                                        wanted = quotient if op % 2 == 0 else left-quotient*right
                                expected = image.copy()
                                if valid:
                                    put(expected,destination,wanted,width)
                                execute(image,expected,int(valid),(shift+placement)%4,
                                        ('arithmetic',source,middle,shift,placement),reads)
                                checks += 1
        print(f'PASS {arch} integer slot/zero/arithmetic/overlap/FP/capacity: {checks} cases',flush=True)
