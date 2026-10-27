#!/usr/bin/env python3
"""Check integer formatting against independent decimal and complete image oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x100000000
CODE = 0x10000
STOP = 0x11000
MASK = (1 << 64)-1


# Encode fields independently of native instruction choices.
def put(image, offset, value, size=8):
    image[offset:offset+size] = (value & ((1 << (size*8))-1)).to_bytes(size, 'little')


# Keep verification enabled under optimized Python.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='integer-format-jit-') as directory:
    td = Path(directory)
    for arch in os.environ.get('FORMAT_ARCHES', 'amd64 arm64').split():
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Query real sizing and publication with independently corrupted layouts. */
int emit(u32int *out, int cap, int wb, int big, int field, int value)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.stringlen=0;a.stringmax=4;a.stringdata=24;a.heapref=24;a.refbytes=8;a.heaptype=8;a.stringtype=(void*)0x1234;
 switch(field){case 1:a.stringlen=value;break;case 2:a.stringmax=value;break;
 case 3:a.stringdata=value;break;case 4:a.heapref=value;break;case 5:a.refbytes=value;break;case 6:a.heaptype=value;break;case 7:a.stringtype=nil;break;}
 return leafemit(out,cap,&a,big ? ICVTLC : ICVTWC);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(Path(os.environ.get('FORMAT_EMITTER_DIR',ROOT/'libinterp'))), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*5
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000); machine.mem_map(BASE, 0x4000); machine.mem_map((1<<64)-0x1000, 0x1000)
        # Mirror top-page metadata through Unicorn x86's default 52-bit physical mask.
        # Guest owner registers retain full 64-bit values for overflow checks.
        if arch == "amd64":machine.mem_map((1<<52)-0x1000, 0x1000)
        writes = []


        # Track same-value writes as well as observable image changes.
        def write(machine, access, address, size, value, context):
            writes.append((address, size))


        # Report unmapped addresses without allowing an invalid native access.
        def invalid(machine, access, address, size, value, context):
            print("invalid guest access", arch, hex(address), size, flush=True)
            return False

        machine.hook_add(UC_HOOK_MEM_WRITE, write)
        machine.hook_add(UC_HOOK_MEM_INVALID, invalid)
        checks = 0
        for wb in (4, 8):
            for big in (0, 1):
                out = (C.c_uint32*128)()
                n = lib.emit(out, 128, wb, big, 0, 0)
                require(0 < n <= 128 and lib.emit(None, 128, wb, big, 0, 0) == n, (arch, wb, big, 'sizing', n))
                for cap in range(n):
                    guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    require(lib.emit(guard, cap, wb, big, 0, 0) == 0 and list(guard) == [0xa5a5a5a5]*128, 'short publication')
                for field, values in ((1, (-1, 1, 4, 2048)), (2, (-1, 1, 0, 2048)), (3, (-1, 0, 4, 2048)), (4, (-1, 0, 1, 136)), (5, (0, 4, 16)), (6, (-1, 0, 1, 136)), (7, (0,))):
                    for value in values:
                        guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                        require(lib.emit(guard, 128, wb, big, field, value) == 0 and list(guard) == [0xa5a5a5a5]*128, (arch, field, value))
                machine.mem_write(CODE, bytes(out)[:n*4]); machine.ctl_remove_cache(CODE, CODE+320)
                width = 8 if big else wb
                limit = 1 << (width*8-1)
                values = [0, 1, -1, 9, -9, 10, -10, 99, -99, 100, -100, limit-1, -limit, 123456789, -123456789]
                # Exercise every decimal length; retain the full alignment matrix for the original seeds.
                for power in range(1, 19):
                    for delta in (-1, 0, 1):
                        values.extend((10**power+delta, -(10**power+delta)))
                if os.environ.get("FORMAT_BOUNDARY_ONLY"):values = []
                for value_index, number in enumerate(values):
                    for profile in [int(value) for value in os.environ.get('FORMAT_PROFILES',' '.join(map(str,range(52)))).split()]:
                        for sa in range(8):
                            for da in range(8):
                                for alias in range(3):
                                    if value_index >= 15 and (sa or da):continue
                                    image = bytearray(b'\xa5'*0x4000)
                                    obj = 0x800+da; owner = BASE+obj
                                    src = 0x100+sa; dst = 0x200+da
                                    length = 7; capacity = 32; refs = 1
                                    if profile == 1:length = 0
                                    if profile == 2:length = -7
                                    if profile == 3:length = -(1 << 31)
                                    if profile == 4:capacity = 23
                                    if profile == 5:capacity = -1
                                    if profile == 6:length = 33
                                    if profile == 7:refs = 0
                                    if profile == 8:refs = 2
                                    if profile == 9:refs = MASK
                                    if profile == 10:owner = 0
                                    if profile == 11:owner = MASK
                                    if 12 <= profile < 34:length = 0; capacity = profile-12
                                    if 34 <= profile <= 39:capacity = profile-33; length = -capacity
                                    if profile == 40:length = -7; capacity = 32
                                    if profile == 41:length = -1; capacity = 0x1fffffff
                                    if profile == 42:length = -1; capacity = 0x20000000
                                    if profile == 43:length = -1; capacity = 0x7fffffff
                                    if profile == 44:length = -33; capacity = 32
                                    if profile == 45:length = -1; capacity = -1
                                    if profile == 46:length = -7; refs = 2
                                    if profile == 47:length = -7; refs = MASK
                                    if profile == 48:length = -7; refs = 0
                                    if profile >= 50:dst = obj+24+sa
                                    if profile == 51:length = -7
                                    if alias == 1:src = obj+24
                                    if alias == 2:src = dst
                                    put(image, obj-8, 0x5678 if profile == 49 else 0x1234); put(image, obj-24, refs); put(image, obj, length, 4); put(image, obj+4, capacity, 4)
                                    put(image, dst, owner); put(image, src, number, width)
                                    # A shared source/result slot contains the owner pointer as its integer.
                                    if alias == 2 or profile >= 50:put(image, dst, owner)
                                    captured = int.from_bytes(image[src:src+width], 'little', signed=True)
                                    put(image, 8, BASE+src); put(image, 16, BASE+dst)
                                    text = str(captured).encode()
                                    bytecapacity = capacity*4 if length < 0 else capacity
                                    valid = profile != 49 and owner not in (0, MASK) and owner%8 == 0 and capacity >= abs(length) and len(text) <= bytecapacity <= 0x7fffffff and refs == 1
                                    expected = bytearray(image)
                                    text = str(captured).encode()
                                    if valid:
                                        expected[obj+24:obj+25+len(text)] = text+b'\0'
                                        put(expected, obj, len(text), 4)
                                        if length < 0:put(expected, obj+4, bytecapacity, 4)
                                        put(expected, dst, owner)
                                    machine.mem_write(BASE, bytes(image)); writes.clear()
                                    if arch == 'amd64':
                                        machine.reg_write(UC_X86_REG_RDI, BASE); machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                        machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little')); machine.reg_write(UC_X86_REG_MXCSR, 0x3f80)
                                    else:
                                        machine.reg_write(UC_ARM64_REG_X0, BASE); machine.reg_write(UC_ARM64_REG_LR, STOP)
                                        machine.reg_write(UC_ARM64_REG_FPCR, 0x800000); machine.reg_write(UC_ARM64_REG_FPSR, 0x11)
                                    machine.emu_start(CODE, STOP, count=3000)
                                    result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                                    detail = (arch, wb, big, number, profile, sa, da, alias)
                                    require(result == valid, (detail, 'return', result))
                                    require(bytes(machine.mem_read(BASE, 0x4000)) == expected, (detail, 'image'))
                                    if valid:
                                        require(writes[-1] == (BASE+dst, 8) and writes[-2] == (BASE+obj, 4), (detail, 'publication'))
                                        require((length >= 0 or writes[0] == (BASE+obj+4, 4)) and all((size == 1 and BASE+obj+24 <= address <= BASE+obj+24+len(text)) for address, size in writes[1 if length < 0 else 0:-2]), (detail, 'write range'))
                                    else:require(not writes, (detail, 'decline writes'))
                                    if arch == 'amd64':require(machine.reg_read(UC_X86_REG_MXCSR) == 0x3f80, 'MXCSR')
                                    else:require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and machine.reg_read(UC_ARM64_REG_FPSR) == 0x11, 'FPCR/FPSR')
                                    checks += 1
        boundarychecks = 0
        for wb in (4, 8):
            for big in (0, 1):
                out = (C.c_uint32*128)(); n = lib.emit(out, 128, wb, big, 0, 0)
                machine.mem_write(CODE, bytes(out)[:n*4]); machine.ctl_remove_cache(CODE, CODE+320)
                width = 8 if big else wb
                for wide in (0, 1):
                    for offset in (0xff8, 0xfe0):
                        for value in (0, 1234567, -(1 << (width*8-1))):
                            high = (1 << 64)-0x1000; owner = high+offset
                            image = bytearray(b'\xa5'*0x4000); top = bytearray(b'\xa5'*0x1000)
                            put(image, 0x100, value, width); put(image, 0x200, owner)
                            put(image, 8, BASE+0x100); put(image, 16, BASE+0x200)
                            put(top, offset-8, 0x1234); put(top, offset-24, 1); put(top, offset, -1 if wide else 0, 4); put(top, offset+4, 32, 4)
                            text = str(value).encode(); start = owner+24
                            valid = start <= MASK and start+len(text) < MASK
                            expected = bytearray(top)
                            if valid:
                                expected[start-high:start-high+len(text)+1] = text+b'\0'
                                put(expected, offset, len(text), 4)
                                if wide:put(expected, offset+4, 128, 4)
                            physical = (1 << 52)-0x1000 if arch == "amd64" else high
                            machine.mem_write(BASE, bytes(image)); machine.mem_write(high, bytes(top))
                            machine.mem_write(physical, bytes(top)); writes.clear()
                            if arch == 'amd64':
                                machine.reg_write(UC_X86_REG_RDI, BASE); machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little'))
                            else:
                                machine.reg_write(UC_ARM64_REG_X0, BASE); machine.reg_write(UC_ARM64_REG_LR, STOP)
                            try:
                                machine.emu_start(CODE, STOP, count=3000)
                            except Exception as error:
                                raise RuntimeError((arch, wb, big, wide, offset, value)) from error
                            result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                            require(result == valid and bytes(machine.mem_read(BASE, 0x4000)) == image and bytes(machine.mem_read(physical, 0x1000)) == expected, (arch, wb, big, wide, offset, value, 'pointer boundary'))
                            if not valid:require(not writes, 'boundary decline wrote memory')
                            boundarychecks += 1
        print('PASS', arch, 'format pointer boundaries:', boundarychecks)
        print('PASS', arch, 'integer formatting:', checks, 'image/write/FP cases')
