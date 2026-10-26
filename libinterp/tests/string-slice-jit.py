#!/usr/bin/env python3
"""Check SLICEC leaves against complete independent VM images and write traces."""
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


with tempfile.TemporaryDirectory(prefix='string-slice-jit-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Query the production slicer and independently malformed private layouts. */
int emit(u32int *out, int cap, int wb, int field, int value)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.stringlen=0;a.stringmax=4;a.stringdata=24;a.heapref=24;a.refbytes=8;
 switch(field){case 1:a.stringlen=value;break;case 2:a.stringmax=value;break;
 case 3:a.stringdata=value;break;case 4:a.heapref=value;break;case 5:a.refbytes=value;break;}
 return leafemit(out,cap,&a,ISLICEC);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*4
        dis = Cs(CS_ARCH_X86, CS_MODE_64) if arch == 'amd64' else Cs(CS_ARCH_ARM64, 0)
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x4000)
        writes = []


        # Include same-value writes to detect publication before a declined guard.
        def write(machine, access, address, size, value, context):
            writes.append((address, size))


        machine.hook_add(UC_HOOK_MEM_WRITE, write)
        checks = 0
        for wb in (4, 8):
            out = (C.c_uint32*64)()
            n = lib.emit(out, 64, wb, 0, 0)
            require(0 < n <= 64 and lib.emit(None, 64, wb, 0, 0) == n, (arch, wb, 'sizing'))
            blob = bytes(out)[:n*4]
            require(sum(i.size for i in dis.disasm(blob, CODE)) == n*4, 'instruction decoding')
            for cap in range(n):
                guard = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                require(lib.emit(guard, cap, wb, 0, 0) == 0 and list(guard) == [0xa5a5a5a5]*64, 'partial publication')
            for field, values in ((1, (-1, 1, 2048)), (2, (-1, 1, 2048)), (3, (-1, 2048)), (4, (-1, 0, 1, 256)), (5, (0, 4, 16))):
                for value in values:
                    guard = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    require(lib.emit(guard, 64, wb, field, value) == 0 and list(guard) == [0xa5a5a5a5]*64, (arch, field, value))
            machine.mem_write(CODE, blob)
            machine.ctl_remove_cache(CODE, CODE+256)
            for wide in (0, 1):
                for profile in range(33):
                    for sa in range(8):
                        for ma in range(8):
                            for da in range(4):
                                image = bytearray(b'\xa5'*0x4000)
                                src = 0x100+sa; mid = 0x200+ma; dst = 0x300+da
                                obj = 0x800+da; owner = BASE+obj
                                start = 2; end = 7; length = 9; capacity = 160; ref = 1
                                if profile == 1:start = 0
                                if profile == 2:start = end
                                if profile == 3:start = -1
                                if profile == 4:end = 10
                                if profile == 5:ref = 0
                                if profile == 6:ref = 2
                                if profile == 7:ref = MASK
                                if profile == 8:owner = 0
                                if profile == 9:owner = MASK
                                if profile == 10:length = -(1 << 31)
                                if profile == 11:capacity = 8
                                if profile == 12:capacity = -1
                                if profile == 13:start = 1 << (wb*8-2)
                                if profile == 14:end = 1 << (wb*8-2)
                                if profile == 15:start = 7; end = 2
                                if profile == 16:length = 160; start = 32; end = 160
                                if profile == 17:length = 160; start = 31; end = 160
                                if profile == 18:src = obj+24
                                if profile == 19:mid = obj+24
                                if profile == 20:start = 0; end = 9
                                if profile == 21:length = 0; start = end = 0
                                if profile == 22:end = -1
                                if profile == 23:length = 160; start = 0; end = 160
                                if profile == 24:length = (1 << 31)-1; capacity = length; start = 0; end = length
                                if profile == 25:length = (1 << 31)-1; capacity = length; start = 0; end = 129
                                if profile == 26:length = 160; start = 0; end = 129
                                if profile >= 27:
                                    length = capacity = 512
                                    start = 7
                                    end = start+(127, 128, 129, 255, 256, 257)[profile-27]
                                signedlen = -length if wide and length > 0 else length
                                put(image, obj-24, ref); put(image, obj, signedlen, 4); put(image, obj+4, capacity, 4)
                                stride = 4 if wide else 1
                                for i in range(512 if profile >= 27 else 160):put(image, obj+24+i*stride, 0x80001001+i*0x10101 if wide else i*73+1, stride)
                                put(image, src, start, wb); put(image, mid, end, wb); put(image, dst, owner)
                                put(image, 8, BASE+src); put(image, 24, BASE+mid); put(image, 16, BASE+dst)
                                valid = owner not in (0, MASK) and ref == 1 and length >= 0 and capacity >= length and 0 <= start < end <= length and (start == 0 or end-start <= 256)
                                expected = bytearray(image)
                                expectedwrites = []
                                if valid:
                                    count = end-start
                                    if start:
                                        expected[obj+24:obj+24+count*stride] = image[obj+24+start*stride:obj+24+end*stride]
                                        expectedwrites = [(BASE+obj+24+i, 1) for i in range(count*stride)]
                                    put(expected, obj, -count if wide else count, 4)
                                    expectedwrites.append((BASE+obj, 4))
                                machine.mem_write(BASE, bytes(image)); writes.clear()
                                if arch == 'amd64':
                                    machine.reg_write(UC_X86_REG_RDI, BASE); machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                    machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little')); machine.reg_write(UC_X86_REG_MXCSR, 0x3f80)
                                else:
                                    machine.reg_write(UC_ARM64_REG_X0, BASE); machine.reg_write(UC_ARM64_REG_LR, STOP)
                                    machine.reg_write(UC_ARM64_REG_FPCR, 0x800000); machine.reg_write(UC_ARM64_REG_FPSR, 0x11)
                                machine.emu_start(CODE, STOP, count=12000)
                                result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                                detail = (arch, wb, wide, profile, sa, ma, da)
                                require(result == valid, (detail, 'return', result, valid))
                                require(bytes(machine.mem_read(BASE, 0x4000)) == expected, (detail, 'image'))
                                require(writes == expectedwrites, (detail, 'writes', writes[:8], expectedwrites[:8]))
                                if arch == 'amd64':require(machine.reg_read(UC_X86_REG_MXCSR) == 0x3f80, 'MXCSR')
                                else:require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and machine.reg_read(UC_ARM64_REG_FPSR) == 0x11, 'FPCR/FPSR')
                                checks += 1
        print('PASS', arch, 'string slicing:', checks, 'image/write/FP checks')
