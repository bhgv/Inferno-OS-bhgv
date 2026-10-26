#!/usr/bin/env python3
"""Check raw copies against independent byte images and native address-span oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_ARCH_RISCV, UC_MODE_RISCV64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.riscv_const import UC_RISCV_REG_A0, UC_RISCV_REG_RA, UC_RISCV_REG_T0
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x100000000
CODE = 0x10000
STOP = 0x11000
MASK = (1 << 64)-1
CODE_UNITS = 128  # Existing private emitter capacity; no production ABI change.
PATTERN = bytes((i*37+11) & 255 for i in range(0x4000))


# Encode fields independently of native instruction choices.
def put(image, offset, value, size=8):
    image[offset:offset+size] = (value & ((1 << (size*8))-1)).to_bytes(size, 'little')


# Keep verification enabled under optimized Python.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='raw-copy-jit-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64', 'riscv64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null sentinel used by private emitter helpers.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Ask the production copier to size or publish an independently configured layout. */
int emit(u32int *out, int cap, int wb, int head, int offset)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;a.listdata=offset;
 return leafemit(out,cap,&a,head ? IHEADM : IMOVM);
}
''')
        if arch == 'riscv64':
            source.write_text(source.read_text().replace('riscv64-emit.h', 'riscv-emit.h').replace('LeafLayout a={0};', 'RvLayout a={0};a.xlen=64;').replace('leafemit(', 'rvemit('))
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*4
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0) if arch == 'arm64' else Uc(UC_ARCH_RISCV, UC_MODE_RISCV64)
        machine.mem_map(CODE, 0x3000); machine.mem_map(BASE, 0x4000)
        if arch == 'riscv64':
            # Unicorn does not expose these CSRs through reg_read/reg_write: use real instructions.
            # Enable FS, seed FFLAGS/FRM, then read FCSR into an ordinary integer register.
            csr = (0x000062b7, 0x3002a073, 0x0018d073, 0x0020d073, 0x003022f3)
            machine.mem_write(CODE+0x1800, b''.join(word.to_bytes(4, 'little') for word in csr))
            machine.emu_start(CODE+0x1800, CODE+0x1814, count=5)
            require(machine.reg_read(UC_RISCV_REG_T0) == 0x31, 'FCSR instruction probe')
        writes = []


        # Include same-value stores so declined guards cannot hide transient writes.
        def write(machine, access, address, size, value, context):
            writes.append((address, size))


        machine.hook_add(UC_HOOK_MEM_WRITE, write)
        checks = 0
        for wb in (4, 8):
            for head in (0, 1):
                out = (C.c_uint32*CODE_UNITS)()
                n = lib.emit(out, CODE_UNITS, wb, head, 24)
                require(0 < n <= CODE_UNITS and
                        lib.emit(None, CODE_UNITS, wb, head, 24) == n,
                        (arch, wb, head, 'sizing', n))
                exact = (C.c_uint32*(n+1))(*([0xa5a5a5a5]*(n+1)))
                require(lib.emit(exact, n, wb, head, 24) == n,
                        (arch, wb, head, 'exact-capacity publication'))
                require(bytes(exact)[:n*4] == bytes(out)[:n*4] and exact[n] == 0xa5a5a5a5,
                        (arch, wb, head, 'exact-capacity image/trailing canary'))
                require(lib.emit(None, n, wb, head, 24) == n,
                        (arch, wb, head, 'exact-capacity sizing'))
                for cap in range(n):
                    require(lib.emit(None, cap, wb, head, 24) == 0,
                            (arch, wb, head, 'short sizing', cap))
                    guard = (C.c_uint32*CODE_UNITS)(*[0xa5a5a5a5]*CODE_UNITS)
                    untouched = cap if arch == 'riscv64' else 0
                    require(lib.emit(guard, cap, wb, head, 24) == 0 and
                            list(guard)[untouched:] == [0xa5a5a5a5]*(CODE_UNITS-untouched),
                            (arch, wb, head, 'short publication', cap))
                if head:
                    for offset in (-1, 4096, (1 << 31)-1):
                        guard = (C.c_uint32*CODE_UNITS)(*[0xa5a5a5a5]*CODE_UNITS)
                        require(lib.emit(guard, CODE_UNITS, wb, head, offset) == 0 and
                                list(guard) == [0xa5a5a5a5]*CODE_UNITS,
                                (arch, wb, head, 'invalid list offset', offset))
                machine.mem_write(CODE, bytes(out)[:n*4])
                machine.ctl_remove_cache(CODE, CODE+CODE_UNITS*4)
                lengths = (0, 1, 2, 7, 8, 15, 31, 64, 127, 128, 129, 255, 256, 257, -1, -(1 << (wb*8-1)), (1 << (wb*8-1))-1)
                for length in lengths:
                    for profile in range(12):
                        for sa in range(8):
                            for ma in (0, 3, 7):
                                for delta in (-17, -1, 0, 1, 4, 17, 257):
                                    image = bytearray(PATTERN)
                                    cell = 0x100+sa; mid = 0x300+ma
                                    owner = BASE+0x1000+sa
                                    src = owner+24 if head else owner
                                    dst = src+delta; count = length
                                    if profile == 1:owner = 0; src = 24 if head else 0
                                    if profile == 2:owner = MASK; src = 23 if head else MASK
                                    if profile in (3, 4):
                                        owner = MASK-27 if head and profile == 3 else MASK-23 if head else MASK-3
                                        src = owner+24 if head else owner
                                        if count > 0:count = 8
                                    if profile == 5:dst = 0
                                    if profile == 6:dst = MASK
                                    if profile == 7:
                                        dst = MASK-3
                                        if count > 0:count = 8
                                    if profile == 8:mid = 0x1000+sa+32
                                    if profile == 9:mid = 0x1000+sa+32; dst = BASE+mid
                                    if profile == 10:cell = 0x1000+sa+32
                                    if profile == 11:dst = BASE+cell if head else BASE+mid
                                    if head:put(image, cell, owner)
                                    put(image, mid, count, wb)
                                    put(image, 8, BASE+cell if head else src)
                                    put(image, 16, dst); put(image, 24, BASE+mid)
                                    ownerok = not head or owner not in (0, MASK)
                                    valid = ownerok and 0 <= count <= 256
                                    if count:
                                        valid = valid and 0 < src < MASK and 0 < dst < MASK and src <= MASK-count and dst <= MASK-count
                                    expected = bytearray(image); expectedwrites = []
                                    if valid and count:
                                        start = src-BASE; finish = dst-BASE
                                        require(0 <= start < 0x4000-count and 0 <= finish < 0x4000-count, 'fixture geometry')
                                        expected[finish:finish+count] = image[start:start+count]
                                        indices = range(count-1, -1, -1) if dst > src else range(count)
                                        expectedwrites = [(dst+i, 1) for i in indices]
                                    machine.mem_write(BASE, bytes(image)); writes.clear()
                                    if arch == 'amd64':
                                        machine.reg_write(UC_X86_REG_RDI, BASE); machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                        machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little')); machine.reg_write(UC_X86_REG_MXCSR, 0x3f80)
                                    elif arch == 'arm64':
                                        machine.reg_write(UC_ARM64_REG_X0, BASE); machine.reg_write(UC_ARM64_REG_LR, STOP)
                                        machine.reg_write(UC_ARM64_REG_FPCR, 0x800000); machine.reg_write(UC_ARM64_REG_FPSR, 0x11)
                                    else:
                                        machine.reg_write(UC_RISCV_REG_A0, BASE); machine.reg_write(UC_RISCV_REG_RA, STOP)
                                        machine.emu_start(CODE+0x1808, CODE+0x1810, count=2)
                                    machine.emu_start(CODE, STOP, count=5000)
                                    result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0 if arch == 'arm64' else UC_RISCV_REG_A0)
                                    detail = (arch, wb, head, length, profile, sa, ma, delta)
                                    require(result == valid, (detail, 'return', result, valid))
                                    require(bytes(machine.mem_read(BASE, 0x4000)) == expected, (detail, 'image'))
                                    require(writes == expectedwrites, (detail, 'writes'))
                                    if arch == 'amd64':require(machine.reg_read(UC_X86_REG_MXCSR) == 0x3f80, 'MXCSR')
                                    elif arch == 'arm64':require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and machine.reg_read(UC_ARM64_REG_FPSR) == 0x11, 'FPCR/FPSR')
                                    else:
                                        machine.emu_start(CODE+0x1810, CODE+0x1814, count=1)
                                        require(machine.reg_read(UC_RISCV_REG_T0) == 0x31, 'FCSR')
                                    checks += 1
        print('PASS', arch, 'raw copies:', checks, 'image/write/span/FP cases')
