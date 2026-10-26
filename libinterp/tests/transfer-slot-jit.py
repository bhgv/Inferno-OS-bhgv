#!/usr/bin/env python3
"""Execute 18 production AMD64/ARM64 transfers with read, write and FP oracles."""
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import struct
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
OPS = ('MOVB','MOVW','MOVL','MOVF','CVTBW','CVTWB','CVTWS','CVTSW','CVTWL','CVTLW',
       'LEA','NEGF','CVTRF','CVTFR','CVTWF','CVTLF','CVTFL','CVTFW')


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


with tempfile.TemporaryDirectory(prefix='transfer-slot-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('TRANSFER_SLOT_ARCHES', 'amd64 arm64').split():
        source = td/(arch+'.c')
        header = Path(os.environ.get('TRANSFER_SLOT_EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
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


        # A bad output must cause decline before even a valid source is read.
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
            inputs = (1,wb,8,8,1,wb,wb,2,wb,8,0,8,4,8,wb,8,8,8)
            outputs = (1,wb,8,8,wb,1,2,wb,8,wb,wb,8,8,4,8,8,8,wb)
            for op,name in enumerate(OPS):
                if os.environ.get('TRANSFER_SLOT_OP') and op != int(os.environ['TRANSFER_SLOT_OP']):
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
                        require(all(destination <= address and address+size <= destination+outputs[op]
                                    for address,size in writes), (label,'result span',writes))
                    if arch == 'amd64':
                        require(machine.reg_read(UC_X86_REG_MXCSR) == 0x1f84 | mode << 13, (label,'MXCSR'))
                    else:
                        require(machine.reg_read(UC_ARM64_REG_FPCR) == mode << 22 and
                                machine.reg_read(UC_ARM64_REG_FPSR) == 0x84, (label,'FP state'))


                # Keep the ignored middle operand inaccessible in every case.
                def setup(shift=0):
                    image = bytearray((i*37+11) & 255 for i in range(1024))
                    put(image,0,0x12345678)
                    put(image,8,BASE+0x100+shift)
                    put(image,16,BASE+0x200+shift)
                    put(image,24,1)
                    put(image,0x100+shift,0x7ff0000000000042)
                    return image

                for side in (0,1):
                    if op == 10 and side == 0:
                        continue
                    width = outputs[op] if side else inputs[op]
                    for address in [0]+[MASK-i for i in range(width)]:
                        for shift in range(8):
                            for mode in range(4):
                                image = setup(shift)
                                put(image,16 if side else 8,address)
                                execute(image,image,0,mode,('slot',side,address,shift),set())
                                checks += 1
                for mode in range(4):
                    image = setup()
                    put(image,8,1)
                    put(image,16,MASK)
                    execute(image,image,0,mode,'output-priority',set())
                    checks += 1
                if op == 10:
                    for address in (0,1,MASK,0x100000123,*[MASK-i for i in range(1,9)]):
                        for shift in range(8):
                            image = setup(shift)
                            put(image,8,address)
                            expected = image.copy()
                            put(expected,0x200+shift,address,wb)
                            execute(image,expected,1,0,('unused-address',address,shift),set())
                            checks += 1
                    continue
                patterns = (0,1,0x80,0x8000,0x80000000,0x8000000000000000,MASK,0x7ff0000000000042)
                if op >= 12:
                    patterns = (-42,-1,0,1,42)
                for value in patterns:
                    for shift in range(8):
                        for delta in (-17,-8,-4,-1,0,1,2,4,7,17):
                            for mode in range(4):
                                image = setup(shift)
                                sourcepos = 0x100+shift
                                destination = sourcepos+delta
                                put(image,16,BASE+destination)
                                if op < 12:
                                    put(image,sourcepos,value,inputs[op])
                                    raw = value & ((1 << (8*inputs[op]))-1)
                                    wanted = signed(raw,2) if op == 7 else signed(raw,wb) if op == 8 else raw
                                    if op == 11:
                                        wanted = raw ^ (1 << 63)
                                elif op in (12,13):
                                    raw = struct.pack('<f' if op == 12 else '<d',float(value))
                                    image[sourcepos:sourcepos+inputs[op]] = raw
                                    wanted = int.from_bytes(struct.pack('<d' if op == 12 else '<f',float(value)),'little')
                                elif op in (14,15):
                                    put(image,sourcepos,value,inputs[op])
                                    wanted = int.from_bytes(struct.pack('<d',float(value)),'little')
                                else:
                                    # Half-integers round to exact integers with no new sticky flags.
                                    real = value+0.5 if value >= 0 else value-0.5
                                    image[sourcepos:sourcepos+8] = struct.pack('<d',real)
                                    wanted = value+1 if value >= 0 else value-1
                                expected = image.copy()
                                put(expected,destination,wanted,outputs[op])
                                execute(image,expected,1,mode,('overlap',value,shift,delta),
                                        {(BASE+sourcepos,inputs[op])})
                                checks += 1
        print(f'PASS {arch} transfer slots/width/overlap/FP/capacity: {checks} cases',flush=True)
