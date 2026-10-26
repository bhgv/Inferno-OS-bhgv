#!/usr/bin/env python3
"""Execute both PC-table backends with independent WORD/cell widths and hostile pointers."""
import ctypes as C
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT = Path(__file__).resolve().parents[2]
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
MASK = (1 << 64)-1


# Store oracle cells independently of any emitted instruction encoding.
def put(memory, offset, value, width=8):
    memory[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


# Keep test checks active under python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='pc-tables-jit-') as directory:
    td = Path(directory)
    total = 0
    for cells in (4, 8):
        model = td/str(cells)
        model.mkdir()
        isa = re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(cells), (ROOT/'include/isa.h').read_text())
        (model/'isa.h').write_text(isa)
        for arch in ('amd64', 'arm64'):
            source = model/(arch+'.c')
            source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null pointer for standalone emitter layout queries.
#include "isa.h"
#include HEADER


/* Emit real PC-table operations with either compact or distant metadata fields. */
int
emit(u32int *out, int cap, int wb, int operation, int large)
{
    int ops[] = {ICASE, ICASEL, IGOTO, IMOVPC};
    LeafLayout a = {0};
    a.wordbytes = wb; a.pc = 0; a.s = 8; a.d = 16; a.m = 24;
    a.module = 40; a.linkmodule = large ? 256 : 8;
    a.linkcompiled = large ? 512 : 16;
    a.moduleprog = large ? 256 : 16; a.modulenprog = large ? 512 : 24;
    a.instbytes = 24;
    return leafemit(out, cap, &a, ops[operation]);
}
'''.replace('HEADER', '"'+arch+'-emit.h"'))
            binary = model/(arch+'.so')
            command = shlex.split(os.environ.get('CC', 'cc'))
            command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            subprocess.run(command+['-shared', '-fPIC', '-I'+str(model), '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
            lib = C.CDLL(str(binary))
            lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*4
            machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
            machine.mem_map(CODE, 0x2000)
            machine.mem_map(BASE, 0x8000)
            arg = xr.UC_X86_REG_RDI if arch == 'amd64' else ar.UC_ARM64_REG_X0
            result = xr.UC_X86_REG_RAX if arch == 'amd64' else ar.UC_ARM64_REG_X0
            for wb in (4, 8):
                for large in (0, 1):
                    for op in range(4):
                        buffer = (C.c_uint32*128)()
                        units = lib.emit(buffer, 128, wb, op, large)
                        require(units > 0, ('missing native path', arch, cells, wb, large, op))
                        machine.mem_write(CODE, bytes(buffer)[:units*4])
                        machine.ctl_remove_cache(CODE, CODE+0x2000)
                        cases = [(count, value, default, alignment, None)
                                 for count in (0, 1, 2, 5)
                                 for value in (-2, -1, 0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 31, 32, 1 << 31, 1 << 40)
                                 for default in (-1, 0, 31, 32)
                                 for alignment in (0, 1, 7)]
                        cases += [(2, 2, 31, 1, bad) for bad in (
                            's-nil', 's-H', 's-wrap', 'link-nil', 'link-H', 'link-wrap',
                            'module-nil', 'module-H', 'module-wrap', 'prog-nil', 'prog-H',
                            'prog-wrap', 'table-nil', 'table-H', 'table-wrap', 'compiled',
                            'count-negative', 'count-huge', 'nprog-negative', 'nprog-zero',
                            'out-nil', 'out-H', 'out-wrap')]
                        for count, rawvalue, default, alignment, bad in cases:
                            memory = bytearray([0xa5])*0x8000
                            link, module, table = BASE+0x1000, BASE+0x2000, BASE+0x3000+alignment
                            sourceptr, output = BASE+0x5000+alignment, BASE+0x6000+alignment
                            valuewidth = 8 if op == 1 else wb
                            value = rawvalue & ((1 << (valuewidth*8))-1)
                            if value & (1 << (valuewidth*8-1)):
                                value -= 1 << (valuewidth*8)
                            put(memory, 0, 0x123456789)
                            put(memory, 8, sourceptr)
                            put(memory, 16, output if op == 3 else table)
                            put(memory, 24, 1)
                            put(memory, 40, link)
                            put(memory, 0x1000+(256 if large else 8), module)
                            put(memory, 0x1000+(512 if large else 16), 0, 4)
                            put(memory, 0x2000+(256 if large else 16), BASE+0x7000)
                            put(memory, 0x2000+(512 if large else 24), 32, 4)
                            put(memory, sourceptr-BASE, value, wb if op != 1 else 8)
                            target = default
                            start = table-BASE
                            if op < 2:
                                header = cells*(2 if op == 1 else 1)
                                stride = 16+2*cells if op == 1 else 3*cells
                                put(memory, start, count, 4)
                                for row in range(count):
                                    at = start+header+row*stride
                                    low, high = row*2, row*2+1
                                    put(memory, at, low, 8 if op == 1 else 4)
                                    put(memory, at+(8 if op == 1 else cells), high, 8 if op == 1 else 4)
                                    put(memory, at+(16 if op == 1 else 2*cells), row+1, 4)
                                    if low <= value < high:
                                        target = row+1
                                put(memory, start+header+count*stride, default, 4)
                                valid = 0 <= target < 32
                            elif op == 2:
                                put(memory, start-cells, count, 4)
                                for row in range(max(count, 3)):
                                    put(memory, start+row*cells, default if row == 0 else row+1, 4)
                                target = default if value == 0 else value+1
                                valid = 0 <= value < count and 0 <= target < 32
                            else:
                                target = value
                                valid = 0 <= target < 32
                            if bad:
                                kind, _, issue = bad.partition('-')
                                poisoned = 0 if issue == 'nil' else MASK if issue == 'H' else MASK-2
                                if kind == 's':
                                    put(memory, 8, poisoned); valid = False
                                elif kind == 'link':
                                    put(memory, 40, poisoned); valid = False
                                elif kind == 'module':
                                    put(memory, 0x1000+(256 if large else 8), poisoned); valid = False
                                elif kind == 'prog':
                                    put(memory, 0x2000+(256 if large else 16), poisoned); valid = False
                                elif kind == 'table' and op != 3:
                                    put(memory, 16, poisoned); valid = False
                                elif kind == 'compiled' and op != 3:
                                    put(memory, 0x1000+(512 if large else 16), 1, 4); valid = False
                                elif kind == 'count' and op != 3:
                                    put(memory, start-(cells if op == 2 else 0), -1 if issue == 'negative' else 0x7fffffff, 4)
                                    valid = op == 2 and issue == 'huge'
                                elif kind == 'nprog':
                                    put(memory, 0x2000+(512 if large else 24), -1 if issue == 'negative' else 0, 4); valid = False
                                elif kind == 'out' and op == 3:
                                    put(memory, 16, poisoned); valid = False
                            expected = bytearray(memory)
                            if valid:
                                put(expected, output-BASE if op == 3 else 0, BASE+0x7000+target*24)
                            machine.mem_write(BASE, bytes(memory))
                            machine.reg_write(arg, BASE)
                            if arch == 'amd64':
                                machine.reg_write(xr.UC_X86_REG_RSP, BASE+0x7800)
                                machine.mem_write(BASE+0x7800, STOP.to_bytes(8, 'little'))
                                expected[0x7800:0x7808] = STOP.to_bytes(8, 'little')
                                machine.reg_write(xr.UC_X86_REG_MXCSR, 0x3fa5)
                            else:
                                machine.reg_write(ar.UC_ARM64_REG_LR, STOP)
                                machine.reg_write(ar.UC_ARM64_REG_SP, BASE+0x7800)
                                machine.reg_write(ar.UC_ARM64_REG_FPCR, 0x400000)
                                machine.reg_write(ar.UC_ARM64_REG_FPSR, 0x15)
                            preserved = [xr.UC_X86_REG_RBX, xr.UC_X86_REG_RBP, xr.UC_X86_REG_R12, xr.UC_X86_REG_R13, xr.UC_X86_REG_R14, xr.UC_X86_REG_R15] if arch == 'amd64' else [getattr(ar, 'UC_ARM64_REG_X'+str(i)) for i in range(19, 30)]
                            for register in preserved:
                                machine.reg_write(register, 0x13579bdf2468ace0+register)
                            context = (arch, cells, wb, large, op, count, rawvalue, default, alignment, bad)
                            try:
                                machine.emu_start(CODE, STOP, count=2000)
                            except Exception as error:
                                raise RuntimeError(('unexpected memory access', context)) from error
                            pc = xr.UC_X86_REG_RIP if arch == 'amd64' else ar.UC_ARM64_REG_PC
                            sp = xr.UC_X86_REG_RSP if arch == 'amd64' else ar.UC_ARM64_REG_SP
                            require(machine.reg_read(pc) == STOP, ('return', context))
                            require(machine.reg_read(sp) == BASE+0x7800+(8 if arch == 'amd64' else 0), ('stack', context))
                            for register in preserved:
                                require(machine.reg_read(register) == 0x13579bdf2468ace0+register, ('callee register', register, context))
                            require(machine.reg_read(result) == int(valid), ('status', machine.reg_read(result), context))
                            require(bytes(machine.mem_read(BASE, 0x8000)) == bytes(expected), ('publication', context))
                            if arch == 'amd64':
                                require(machine.reg_read(xr.UC_X86_REG_MXCSR) == 0x3fa5, ('FP state', context))
                            else:
                                require(machine.reg_read(ar.UC_ARM64_REG_FPCR) == 0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR) == 0x15, ('FP state', context))
                            total += 1
            print('PASS PC tables:', arch, 'cell', cells, flush=True)
    print('PASS PC tables:', total, 'WORD/cell/layout/range/error/FP cases')
