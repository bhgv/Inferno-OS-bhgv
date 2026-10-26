#!/usr/bin/env python3
"""Check x64/ARM64 scalar list and length leaves against independent span rules."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
MASK = (1 << 64)-1
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
HIGH = (1 << 64)-0x10000


# Keep every oracle enabled with optimized Python.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Encode VM cells without sharing any encoder arithmetic.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='scalar-view-jit-') as directory:
    td = Path(directory)
    for arch in os.environ.get('VIEW_ARCHES', 'arm64 amd64').split():
        header = Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null identity used by the production emitter.
#include "isa.h"
#include HEADER


/* Query each production path with independently varied inline field offsets. */
int
emit(u32int *out, int cap, int wb, int kind, int offset)
{
    int ops[] = {IHEADB, IHEADW, IHEADL, IHEADF, ILENL, ILENC};
    LeafLayout a = {0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.listdata=offset; a.listtail=offset; a.stringlen=offset;
    return leafemit(out,cap,&a,ops[kind]);
}
'''.replace('HEADER', '"'+str(header)+'"'))
        binary = td/(arch+'.so')
        subprocess.run(shlex.split(os.environ.get('CC', 'cc'))+
                       shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+
                       ['-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'),
                        str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*4
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x20000)
        machine.mem_map(HIGH, 0x10000)
        physical = (1 << 52)-0x10000 if arch == 'amd64' else HIGH
        if physical != HIGH:
            machine.mem_map(physical, 0x10000)
        writes = []
        machine.hook_add(UC_HOOK_MEM_WRITE, lambda uc, access, address, size, value, context:
                         writes.append((address, size)))
        checks = 0
        for wb in (4, 8):
            for kind in range(6):
                width = (1, wb, 8, 8, 8, 4)[kind]
                offsets = (0, 8, 24, 4088) if kind < 4 else (0, 8, 24, 32760) if kind == 4 else (0, 4, 12, 16380)
                for offset in offsets:
                    out = (C.c_uint32*66)(*[0xdeadbeef]*66)
                    count = lib.emit(out, 64, wb, kind, offset)
                    require(0 < count <= 64, (arch, wb, kind, offset, 'size', count))
                    require(list(out)[64:] == [0xdeadbeef]*2, 'output canary')
                    for cap in range(count):
                        short = (C.c_uint32*66)(*[0xdeadbeef]*66)
                        require(lib.emit(short, cap, wb, kind, offset) == 0, 'short buffer accepted')
                        require(all(v == 0xdeadbeef for v in short), 'partial code published')
                        checks += 1
                    machine.mem_write(CODE, bytes(out)[:count*4])
                    machine.ctl_remove_cache(CODE, CODE+256)
                    owners = (0, MASK, BASE+0x2000, BASE+0x2004, HIGH+0xff00, HIGH+0xfff0,
                              MASK-offset-width, MASK-offset-width+1, MASK-offset, MASK-1)
                    values = (0, 1, 0x8000000000000000, MASK) if kind < 4 else (MASK,) if kind == 4 else (0, 1, -7, -(1 << 31), (1 << 31)-1)
                    for owner, chain in ((o, c) for o in owners for c in ((0, 1) if kind == 4 else (0,))):
                        for value in values:
                            for sa in range(8):
                                for alias in (0, 1, 2):
                                    image = bytearray(b'\xa5'*0x20000)
                                    top = bytearray(b'\x5a'*0x10000)
                                    reg = bytearray(b'\x37'*128)
                                    field = owner+offset
                                    validspan = owner not in (0, MASK) and field+width <= MASK
                                    if validspan:
                                        target, at = (image, field-BASE) if owner < HIGH else (top, field-HIGH)
                                        put(target, at, value, width)
                                    src = 0x100+sa
                                    dst = 0x300+sa if alias == 0 else src if alias == 1 else 0x2000+offset
                                    put(image, src, BASE+0x10000 if chain else owner)
                                    if chain:
                                        put(image, 0x10000+offset, owner)
                                    put(reg, 8, BASE+src); put(reg, 16, BASE+dst)
                                    expected = bytearray(image)
                                    expectedtop = bytearray(top)
                                    accepted = (kind >= 4 and owner == MASK) or validspan
                                    if kind == 5 and owner != MASK and value == -(1 << 31):
                                        accepted = False
                                    if accepted:
                                        if kind < 4:
                                            result, resultwidth = value, width
                                        elif kind == 4:
                                            result, resultwidth = (chain if owner == MASK else 1+chain), wb
                                        else:
                                            result, resultwidth = (0 if owner == MASK else abs(value)), wb
                                        put(expected, dst, result, resultwidth)
                                    machine.mem_write(BASE, bytes(image)); machine.mem_write(HIGH, bytes(top))
                                    if physical != HIGH:
                                        machine.mem_write(physical, bytes(top))
                                    machine.mem_write(BASE+0x1f000, bytes(reg))
                                    # The REG image is separate from all observed operand and owner cells.
                                    expected[0x1f000:0x1f080] = reg
                                    if arch == 'amd64':
                                        machine.mem_write(BASE+0x1e000, struct.pack('<Q', STOP))
                                        expected[0x1e000:0x1e008] = struct.pack('<Q', STOP)
                                        machine.reg_write(UC_X86_REG_RDI, BASE+0x1f000)
                                        machine.reg_write(UC_X86_REG_RSP, BASE+0x1e000)
                                        machine.reg_write(UC_X86_REG_MXCSR, 0x1fa5)
                                    else:
                                        machine.reg_write(UC_ARM64_REG_X0, BASE+0x1f000)
                                        machine.reg_write(UC_ARM64_REG_LR, STOP)
                                        machine.reg_write(UC_ARM64_REG_FPCR, 0x400000)
                                        machine.reg_write(UC_ARM64_REG_FPSR, 0x15)
                                    writes.clear()
                                    machine.emu_start(CODE, STOP, count=4096)
                                    result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                                    detail = (arch, wb, kind, offset, hex(owner), value, sa, alias, chain)
                                    require(result == int(accepted), ('accept', detail, result))
                                    require(bytes(machine.mem_read(BASE, len(image))) == bytes(expected), ('image', detail))
                                    require(bytes(machine.mem_read(physical, len(top))) == bytes(expectedtop), ('owner', detail))
                                    require(bool(writes) == bool(accepted), ('writes', detail))
                                    if arch == 'amd64':
                                        require(machine.reg_read(UC_X86_REG_MXCSR) == 0x1fa5, 'MXCSR changed')
                                    else:
                                        require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x400000 and
                                                machine.reg_read(UC_ARM64_REG_FPSR) == 0x15, 'FP state changed')
                                    checks += 1
        print('PASS', arch, 'scalar view spans:', checks, 'checks')
