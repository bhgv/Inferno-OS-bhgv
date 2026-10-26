#!/usr/bin/env python3
"""Execute actual ARM64/AMD64 ADDC leaves with independent memory/write oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
MASK = (1 << 64)-1


def put(image, offset, value, width=8):
    """Write native pointers and signed scalar fields independently of emitter code."""
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='string-append-jit-') as directory:
    td = Path(directory)
    for arch in ('arm64', 'amd64'):
        checks = 0
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null pointer used by the production emitter.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Stage the real append instruction using independent object/header offsets. */
int emit(u32int *out, int cap, int wb, int field, int value)
{
 LeafLayout a = {0};
 a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24;
 a.stringlen=0; a.stringmax=4; a.stringdata=24; a.heapref=24; a.refbytes=8;
 switch(field){
 case 1:a.stringlen=value;break;
 case 2:a.stringmax=value;break;
 case 3:a.stringdata=value;break;
 case 4:a.heapref=value;break;
 case 5:a.refbytes=value;break;
 }
 return leafemit(out,cap,&a,IADDC);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'),
                               '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        library = C.CDLL(str(binary))
        library.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*4
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x4000)
        writes = []


        def written(machine, access, address, size, value, context):
            """Record even same-value writes on declined or successful paths."""
            writes.append((address, size))


        machine.hook_add(UC_HOOK_MEM_WRITE, written)
        for wb in (4, 8):
            output = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
            count = library.emit(output, 128, wb, 0, 0)
            if not 0 < count <= 128 or library.emit(None, 128, wb, 0, 0) != count:
                raise RuntimeError((arch, wb, 'append leaf unavailable'))
            code = bytes(output)[:count*4]
            for capacity in range(count):
                guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                if library.emit(guard, capacity, wb, 0, 0) != 0 or list(guard) != [0xa5a5a5a5]*128:
                    raise RuntimeError('partial append publication')
                checks += 1
            for field, values in ((1, (-1, 1, 2048)), (2, (-1, 1, 2048)),
                                  (3, (-1, 2048)), (4, (0, 1, 256)), (5, (0, 4, 16))):
                for value in values:
                    guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    if library.emit(guard, 128, wb, field, value) != 0 or list(guard) != [0xa5a5a5a5]*128:
                        raise RuntimeError((arch, field, value, 'invalid layout accepted'))
                    checks += 1
            machine.mem_write(CODE, code)
            machine.ctl_remove_cache(CODE, CODE+512)
            for leftlen in (0, 1, 7, 127, 128, 255, 256):
                for rightlen in (0, 1, 7, 128, 129, 255, 256, 257):
                    for profile in range(32):
                        for alignment in (0, 1, 4, 7):
                            for slot in (0, 1, 4, 7):
                                image = bytearray(b'\xa5'*0x4000)
                                left, right = 0x800+alignment, 0xc00+alignment
                                l, r, capacity, rightcap, ref = leftlen, rightlen, 320, 320, 1
                                middle, src = 0x100+slot, 0x200+slot
                                destination = middle
                                leftowner, rightowner = BASE+left, BASE+right
                                if profile == 1: destination = 0x300+slot
                                if profile == 2: ref = 2
                                if profile == 3: ref = 0
                                if profile == 22: ref = (1 << 32)+1
                                if profile == 4: capacity = l-1
                                if profile == 5: rightcap = r-1
                                if profile == 6: capacity = l+r
                                if profile == 7: rightowner = MASK
                                if profile == 8: rightowner = 0
                                if profile == 9: leftowner = 0
                                if profile == 10: leftowner = MASK
                                if profile == 11: l = -1
                                if profile == 12: r = -1
                                if profile == 13:
                                    right = left; rightowner = leftowner; r = l; src = middle
                                if profile == 14:
                                    l, r = 65, 128
                                    right = left+64; rightowner = BASE+right
                                if profile == 15: l, r, capacity = 0x7fffffff, 1, 0x7fffffff
                                if profile == 16: r = -0x80000000
                                if profile == 17: capacity = -0x80000000
                                if profile == 18:
                                    l, r = 32, 128
                                    middle = destination = left+24+40
                                if profile == 19:
                                    l, r = 32, 128
                                    src = left+24+40
                                if profile == 20:
                                    l, r = 32, 128
                                    right = left+64; rightowner = BASE+right
                                wide = profile >= 25
                                if wide:
                                    right = 0x1800+alignment; rightowner = BASE+right
                                    l, r = -leftlen, -rightlen
                                    if profile == 26:
                                        right = left; rightowner = leftowner; r = l; src = middle
                                    if profile == 27: capacity = abs(l)+abs(r)-1
                                    if profile == 28: rightcap = abs(r)-1
                                    if profile == 29:
                                        l, r = -8, -128
                                        middle = destination = left+24+48
                                    if profile == 30: l, r = -0x80000000, -1
                                    if profile == 31: l, r = -1, -0x80000000
                                for i in range(320*(4 if wide else 1)): image[left+24+i] = (i*73+1)&255
                                if right != left:
                                    for i in range(320*(4 if wide else 1)): image[right+24+i] = (i*17+255)&255
                                put(image, left-24, ref)
                                put(image, left, l, 4); put(image, left+4, capacity, 4)
                                if right != left:
                                    put(image, right-24, 0 if profile == 21 else MASK if profile == 23 else 1 << 32 if profile == 24 else 1)
                                    put(image, right, r, 4); put(image, right+4, rightcap, 4)
                                put(image, middle, leftowner); put(image, src, rightowner)
                                put(image, destination, leftowner)
                                put(image, 8, BASE+src); put(image, 16, BASE+destination); put(image, 24, BASE+middle)
                                good = (destination == middle and leftowner == BASE+left and rightowner == BASE+right
                                        and ref == 1 and profile != 21 and (l<0)==(r<0) and 0 <= abs(r) <= 256
                                        and abs(r) <= (capacity if right == left else rightcap) and abs(l)+abs(r) <= capacity)
                                stride = 4 if l<0 else 1
                                copied = abs(r)*stride
                                target, origin = left+24+abs(l)*stride, right+24
                                if origin < target < origin+copied: good = False
                                expected = bytearray(image)
                                expectedwrites = []
                                if good:
                                    expected[target:target+copied] = bytes(image[origin:origin+copied])
                                    put(expected, left, -(abs(l)+abs(r)) if l<0 else abs(l)+abs(r), 4)
                                    expectedwrites = [(BASE+target+i, 1) for i in range(copied)]+[(BASE+left, 4)]
                                machine.mem_write(BASE, bytes(image)); writes.clear()
                                if arch == 'amd64':
                                    machine.reg_write(UC_X86_REG_RDI, BASE)
                                    machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                    machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little'))
                                    machine.reg_write(UC_X86_REG_MXCSR, 0x3f80)
                                else:
                                    machine.reg_write(UC_ARM64_REG_X0, BASE)
                                    machine.reg_write(UC_ARM64_REG_LR, STOP)
                                    machine.reg_write(UC_ARM64_REG_FPCR, 0x800000)
                                    machine.reg_write(UC_ARM64_REG_FPSR, 0x11)
                                machine.emu_start(CODE, STOP, count=12000)
                                result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                                if result != good or bytes(machine.mem_read(BASE, 0x4000)) != bytes(expected) or writes != expectedwrites:
                                    raise RuntimeError((arch, wb, leftlen, rightlen, profile, alignment, slot, result, good, writes[:4]))
                                if arch == 'amd64':
                                    if machine.reg_read(UC_X86_REG_MXCSR) != 0x3f80: raise RuntimeError('MXCSR changed')
                                elif machine.reg_read(UC_ARM64_REG_FPCR) != 0x800000 or machine.reg_read(UC_ARM64_REG_FPSR) != 0x11:
                                    raise RuntimeError('FPCR/FPSR changed')
                                checks += 1
        print('PASS', arch, 'string append:', checks, 'instruction/image/write/FP checks', flush=True)
