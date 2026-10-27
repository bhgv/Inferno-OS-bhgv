#!/usr/bin/env python3
"""Reject malformed list operand cells before native writes or allocator calls."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT = Path(__file__).resolve().parents[2]  # Production VM tree.
BASE, CODE, STOP, HELPER = 0x100000000, 0x10000, 0x11000, 0x123456780000
MASK = (1 << 64)-1  # Native pointer sentinel and wrap boundary.


# Retain each independent oracle when Python runs with -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Serialize VM cells without sharing machine-instruction selection.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='cons-slots-jit-') as temporary:
    directory = Path(temporary)
    for arch in os.environ.get('CONS_ARCHES', 'amd64 arm64').split():
        source = directory/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null identity for emission helpers.
#include "isa.h"
#include HEADER


/* Emit every affected constructor with independent offsets and callback identities. */
int
emit(u32int *output, int cap, int wb, int oi)
{
    int ops[]={ICONSB, ICONSW, ICONSL, ICONSF, ICONSP, ICONSM};
    LeafLayout a={0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.listtail=0; a.listtype=8; a.listdata=16; a.typeref=0;
    a.heapref=24; a.heapcolor=32; a.refbytes=8; a.markcolor=3;
    a.consentry=(void*)0x123456780000ULL;
    a.ptrtype=(void*)0x100004100ULL;
    a.mutator=(void*)0x100004020ULL; a.nprop=(void*)0x100004024ULL;
    return leafemit(output,cap,&a,ops[oi]);
}
'''.replace('HEADER', '"'+str(Path(os.environ.get('CONS_EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h'))+'"'))
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
        machine.mem_map(HELPER, 0x1000)
        writes, calls = [], []


        # Bad operands must return to C before reaching even the shared allocator.
        def allocator(uc, address, size, context):
            if address == HELPER:
                calls.append(address)
                uc.emu_stop()


        machine.hook_add(UC_HOOK_CODE, allocator)
        machine.hook_add(UC_HOOK_MEM_WRITE,
                         lambda uc, access, address, size, value, context: writes.append((address, size)))
        checks = 0
        for wb in (4, 8):
            for oi in range(6):
                if os.environ.get('CONS_SLOT_OP') and oi != int(os.environ['CONS_SLOT_OP']):
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
                for kind in range(3):
                    if kind == 2 and oi != 5:
                        continue
                    if os.environ.get('CONS_SLOT_KIND') and kind != int(os.environ['CONS_SLOT_KIND']):
                        continue
                    width = wb if kind == 2 else 8 if kind == 1 else 1 if oi == 0 else wb if oi == 1 else 8
                    for offset in range(width+1):
                        for alignment in range(8):
                            image = bytearray(b'\xa5'*0x8000)
                            put(image, 8, BASE+0x100+alignment)
                            put(image, 16, BASE+0x200+alignment)
                            put(image, 24, BASE+0x300+alignment)
                            put(image, 0x100+alignment, MASK)
                            put(image, 0x200+alignment, MASK)
                            put(image, 0x300+alignment, 8, wb)
                            put(image, 0x4100, 1, 4)
                            put(image, (8, 16, 24)[kind], 0 if offset == 0 else (1 << 64)-offset)
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
                            calls.clear()
                            detail = (arch, wb, oi, kind, offset, alignment)
                            try:
                                machine.emu_start(CODE, STOP, count=1000)
                            except Exception as error:
                                raise RuntimeError(('execution', detail)) from error
                            require(not calls and not writes, ('allocation or publication before decline', detail))
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
        print('PASS', arch, 'constructor slots:', checks, 'decline/ABI/capacity checks', flush=True)
