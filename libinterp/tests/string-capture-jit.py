#!/usr/bin/env python3
"""Reject string conversion cells and formatter header bounds."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT = Path(__file__).resolve().parents[2]  # Production VM tree.
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
MASK = (1 << 64)-1  # Native pointer sentinel and wrap boundary.


# Retain each independent oracle when Python runs with -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Serialize VM cells without sharing machine-instruction selection.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='string-capture-jit-') as temporary:
    directory = Path(temporary)
    for arch in os.environ.get('STRING_CAPTURE_ARCHES', 'amd64 arm64').split():
        source = directory/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null identity for emission helpers.
#include "isa.h"
#include HEADER


/* Emit codecs and numeric formatters with independent metadata offsets. */
int
emit(u32int *output, int cap, int wb, int oi)
{
    int ops[]={ICVTWC, ICVTLC, ICVTFC, ICVTCA, ICVTAC};
    LeafLayout a={0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.arraylen=0; a.arraytype=8; a.arrayroot=16; a.arraydata=24;
    a.typesize=24; a.typenp=28;
    a.stringlen=0; a.stringmax=4; a.stringdata=24;
    a.heapref=24; a.heaptype=16; a.heapcolor=32; a.refbytes=8;
    a.arrayidentity=(void*)0x100004600ULL;
    a.stringtype=(void*)0x100004500ULL;
    return leafemit(output,cap,&a,ops[oi]);
}
'''.replace('HEADER', '"'+str(Path(os.environ.get('STRING_CAPTURE_EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h'))+'"'))
        binary = directory/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'),
                               '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        library = C.CDLL(str(binary))
        library.emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.c_int, C.c_int]
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x2000)
        machine.mem_map(BASE, 0x8000)
        writes = []


        machine.hook_add(UC_HOOK_MEM_WRITE,
                         lambda uc, access, address, size, value, context: writes.append((address, size)))
        checks = 0
        for wb in (4, 8):
            for oi in range(5):
                if os.environ.get('STRING_CAPTURE_OP') and oi != int(os.environ['STRING_CAPTURE_OP']):
                    continue
                output = (C.c_uint32*130)(*[0xdeadbeef]*130)
                units = library.emit(output, 128, wb, oi)
                require(0 < units <= 128, ('size', arch, wb, oi, units))
                require(library.emit(None, 128, wb, oi) == units, 'sizing mismatch')
                require(list(output)[128:] == [0xdeadbeef]*2, 'output canary')
                for cap in range(units):
                    short = (C.c_uint32*130)(*[0xdeadbeef]*130)
                    require(library.emit(short, cap, wb, oi) == 0 and
                            list(short) == [0xdeadbeef]*130, 'partial publication')
                    checks += 1
                machine.mem_write(CODE, bytes(output)[:units*4])
                machine.ctl_remove_cache(CODE, CODE+512)
                for kind in range(5):
                    if kind == 2 and oi >= 3:
                        continue
                    if os.environ.get('STRING_CAPTURE_KIND') and kind != int(os.environ['STRING_CAPTURE_KIND']):
                        continue
                    width = wb if kind in (0,4) and oi == 0 else 8
                    for offset in range(width+1):
                        for alignment in range(8):
                            image = bytearray(b'\xa5'*0x8000)
                            put(image, 8, BASE+0x100+alignment)
                            put(image, 16, BASE+0x200+alignment)
                            put(image, 24, BASE+0x300+alignment)
                            put(image, 0x100+alignment, -123 if oi == 0 else -(1<<63) if oi == 1 else 0x4000000000000000 if oi == 2 else BASE+0x5000 if oi == 3 else BASE+0x6000, wb if oi == 0 else 8)
                            put(image, 0x200+alignment, BASE+(0x6000 if oi == 3 else 0x5000))
                            put(image, 0x4000, 1, 4)
                            put(image, 0x4018, 1, 4)
                            put(image, 0x401c, 0, 4)
                            for owner, identity in ((0x5000, 0x4500), (0x6000, 0x4600)):
                                put(image, owner-24, 1)
                                put(image, owner-16, BASE+identity)
                                put(image, owner, 8, wb if owner == 0x6000 else 4)
                            put(image, 0x5004, 64, 4)
                            image[0x5018:0x5058] = b'A'*64
                            put(image, 0x6008, BASE+0x4000)
                            put(image, 0x6010, MASK)
                            put(image, 0x6018, BASE+0x6100)
                            image[0x6100:0x6140] = b'A'*64
                            bad = 0 if offset == 0 else (1<<64)-offset
                            if kind == 0:
                                put(image, 8, bad)
                            elif kind == 1:
                                put(image, 16, bad)
                            elif kind == 2:
                                put(image, 0x200+alignment, (0,8,32,BASE+0x5001,MASK-7)[offset%5])
                            elif kind == 3:
                                if oi < 3:
                                    put(image, 8, BASE+0x100000)
                                else:
                                    put(image, 0x100+alignment, BASE+0x100000)
                                put(image, 16, bad)
                            else:
                                put(image, 8, bad)
                                put(image, 0x200+alignment, BASE+0x100000)
                            if arch == 'amd64':
                                put(image, 0x7808, STOP)
                                machine.reg_write(xr.UC_X86_REG_RDI, BASE)
                                machine.reg_write(xr.UC_X86_REG_RSP, BASE+0x7808)
                                machine.reg_write(xr.UC_X86_REG_MXCSR, 0x3f95)
                                saved = [getattr(xr, 'UC_X86_REG_'+name) for name in ('RBX', 'RBP', 'R12', 'R13', 'R14', 'R15')]
                            else:
                                machine.reg_write(ar.UC_ARM64_REG_X0, BASE)
                                machine.reg_write(ar.UC_ARM64_REG_SP, BASE+0x7800)
                                machine.reg_write(ar.UC_ARM64_REG_LR, STOP)
                                machine.reg_write(ar.UC_ARM64_REG_FPCR, 0x400000)
                                machine.reg_write(ar.UC_ARM64_REG_FPSR, 0x15)
                                saved = [getattr(ar, 'UC_ARM64_REG_X'+str(i)) for i in range(19, 30)]
                            for reg in saved:
                                machine.reg_write(reg, 0xabcdef000+reg)
                            machine.mem_write(BASE, bytes(image))
                            writes.clear()
                            detail = (arch, wb, oi, kind, offset, alignment)
                            try:
                                machine.emu_start(CODE, STOP, count=1000)
                            except Exception as error:
                                raise RuntimeError(('execution', detail)) from error
                            require(not writes, ('allocation or publication before decline', detail))
                            status = machine.reg_read(xr.UC_X86_REG_RAX if arch == 'amd64' else ar.UC_ARM64_REG_X0)
                            require(status == 0, ('decline', detail, status))
                            require(bytes(machine.mem_read(BASE, len(image))) == image, ('VM mutation', detail))
                            for reg in saved:
                                require(machine.reg_read(reg) == 0xabcdef000+reg, ('saved register', detail, reg))
                            if arch == 'amd64':
                                require(machine.reg_read(xr.UC_X86_REG_RSP) == BASE+0x7810, ('SP', detail))
                                require(machine.reg_read(xr.UC_X86_REG_MXCSR) == 0x3f95, ('MXCSR', detail))
                            else:
                                require(machine.reg_read(ar.UC_ARM64_REG_SP) == BASE+0x7800 and
                                        machine.reg_read(ar.UC_ARM64_REG_LR) == STOP, ('SP/LR', detail))
                                require(machine.reg_read(ar.UC_ARM64_REG_FPCR) == 0x400000 and
                                        machine.reg_read(ar.UC_ARM64_REG_FPSR) == 0x15, ('FP state', detail))
                            checks += 1
        print('PASS', arch, 'string conversion cells/headers:', checks, 'decline/ABI/capacity checks', flush=True)
