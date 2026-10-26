#!/usr/bin/env python3
"""Execute nineteen production branch leaves with independent integer and image oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]  # Owning production repository.
OPS = tuple('B' + condition + width for width in ('B', 'W', 'L') for condition in ('EQ', 'NE', 'LT', 'LE', 'GT', 'GE')) + ('JMP',)  # All changed commands.
BASE = 0x100000000  # Independent guest register/data mapping.
CODE = 0x10000  # Executable guest mapping.
STOP = CODE + 0x1000  # Leaf return sentinel.
MASK = (1 << 64) - 1  # Native pointer limit.


# Require an evaluated predicate even under Python -O, without changing guest state.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Store a modulo-width integer in the owned image, without alignment assumptions.
def put(image, offset, value, width=8):
    image[offset:offset + width] = (value & ((1 << (8 * width)) - 1)).to_bytes(width, 'little')


# Return the independent Python signed/unsigned comparison for one opcode and width.
def predicate(op, left, right, width):
    if op == 18:
        return True
    if width != 1:
        sign = 1 << (width * 8 - 1)
        left = (left ^ sign) - sign
        right = (right ^ sign) - sign
    return (left == right, left != right, left < right, left <= right, left > right, left >= right)[op % 6]


with tempfile.TemporaryDirectory(prefix='integer-branch-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('INTEGER_BRANCH_ARCHES', 'amd64 arm64').split():
        header = Path(os.environ.get('INTEGER_BRANCH_EMITTER_DIR', ROOT / 'libinterp')) / (arch + '-emit.h')
        source = td / 'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null output for sizing.
#include "isa.h"
#include "''' + str(header) + '''"


/* Borrow out for cap code units; return length, or zero without publication. */
int emit(u32int *out, int cap, int wb, int op)
{
    LeafLayout a = {0};
    static int ops[] = {''' + ','.join('I' + op for op in OPS) + '''};
    a.wordbytes = wb;
    a.pc = 0;
    a.s = 8;
    a.d = 16;
    a.m = 24;
    return leafemit(out, cap, &a, ops[op]);
}
''')
        binary = td / (arch + '.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command + ['-shared', '-fPIC', '-I' + str(ROOT / 'include'),
                                  '-I' + str(ROOT / 'libinterp'), str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.c_int, C.c_int]
        machine = Uc(UC_ARCH_X86 if arch == 'amd64' else UC_ARCH_ARM64, UC_MODE_64 if arch == 'amd64' else 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x4000)
        reads, writes = [], []


        # Record actual guest reads to detect forbidden early source/target access.
        def read_hook(machine, access, address, size, value, context):
            reads.append((address, size))


        # Record even same-value guest writes on rejected leaves.
        def write_hook(machine, access, address, size, value, context):
            writes.append((address, size))


        machine.hook_add(UC_HOOK_MEM_READ, read_hook)
        machine.hook_add(UC_HOOK_MEM_WRITE, write_hook)
        checks = 0
        for wb in (4, 8):
            for op, name in enumerate(OPS):
                if os.environ.get('INTEGER_BRANCH_OP') and op != int(os.environ['INTEGER_BRANCH_OP']):
                    continue
                width = 1 if op < 6 else wb if op < 12 else 8
                mask = (1 << (width * 8)) - 1
                out = (C.c_uint32 * 129)(*([0xa5a5a5a5] * 129))
                n = lib.emit(out, 128, wb, op)
                require(0 < n <= 128 and lib.emit(None, 128, wb, op) == n, (arch, wb, name, 'sizing'))
                exact = (C.c_uint32 * (n + 1))(*([0xa5a5a5a5] * (n + 1)))
                require(lib.emit(exact, n, wb, op) == n and exact[n] == 0xa5a5a5a5 and
                        bytes(exact)[:n * 4] == bytes(out)[:n * 4], 'exact capacity/canary')
                for cap in range(n):
                    guard = (C.c_uint32 * 129)(*([0xa5a5a5a5] * 129))
                    require(lib.emit(None, cap, wb, op) == 0 and lib.emit(guard, cap, wb, op) == 0 and
                            list(guard) == [0xa5a5a5a5] * 129, 'short capacity publication')
                machine.mem_write(CODE, bytes(out)[:n * 4])
                machine.ctl_remove_cache(CODE, CODE + 512)
                edges = (0, 1, 2, 127, 128, 255, mask, mask - 1, 1 << (width * 8 - 1), (1 << (width * 8 - 1)) - 1)
                bad = [0, MASK] + [MASK - i for i in range(1, 8)]
                cases = [('compare', a, b) for a in edges for b in edges]
                cases += [('slot', slot, pointer) for slot in (('d',) if op == 18 else ('s', 'm', 'd'))
                          for pointer in (bad if slot == 'd' else [0, MASK] + [MASK - i for i in range(1, width)])]
                if op != 18:
                    cases += [('false-slot', 'd', pointer) for pointer in bad + [1]]
                for kind, parameter, value in cases:
                    for alignment in range(8):
                        for alias in ((0, 1, 2, 3) if kind == 'compare' else (0,)):
                            image = bytearray(b'\xa5' * 0x4000)
                            source = BASE + 0x100 + alignment
                            middle = BASE + 0x200 + alignment
                            dest = BASE + 0x300 + alignment
                            if alias:
                                dest = source if alias == 1 else middle if alias == 2 else source + 1
                            truth = ((1, 1), (1, 2), (1, 2), (1, 1), (2, 1), (1, 1))
                            falsity = ((1, 2), (1, 1), (2, 1), (2, 1), (1, 2), (1, 2))
                            left, right = (parameter, value) if kind == 'compare' else (falsity if kind == 'false-slot' else truth)[op % 6]
                            put(image, source - BASE, left, width)
                            put(image, middle - BASE, right, width)
                            put(image, dest - BASE, BASE + 0x3800)
                            # Aliased targets can replace operand bytes; recapture for the oracle.
                            left = int.from_bytes(image[source - BASE:source - BASE + width], 'little')
                            right = int.from_bytes(image[middle - BASE:middle - BASE + width], 'little')
                            take = predicate(op, left, right, width)
                            valid = kind != 'slot'
                            if kind in ('slot', 'false-slot'):
                                if parameter == 's':
                                    source, middle = value, 1
                                elif parameter == 'm':
                                    middle, source = value, 1
                                else:
                                    dest = value
                            if op == 18:
                                source = middle = 1  # JMP has no scalar inputs.
                            put(image, 0, BASE + 0x3900)
                            put(image, 8, source)
                            put(image, 16, dest)
                            put(image, 24, middle)
                            expected = bytearray(image)
                            if valid and take:
                                put(expected, 0, BASE + 0x3800)
                            machine.mem_write(BASE, bytes(image))
                            if arch == 'amd64':
                                machine.reg_write(UC_X86_REG_RDI, BASE)
                                machine.reg_write(UC_X86_REG_RSP, CODE + 0x2800)
                                machine.mem_write(CODE + 0x2800, STOP.to_bytes(8, 'little'))
                                machine.reg_write(UC_X86_REG_MXCSR, 0x5fa5)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0, BASE)
                                machine.reg_write(UC_ARM64_REG_LR, STOP)
                                machine.reg_write(UC_ARM64_REG_FPCR, 0x800000)
                                machine.reg_write(UC_ARM64_REG_FPSR, 0x95)
                            reads.clear()
                            writes.clear()
                            machine.emu_start(CODE, STOP, count=1000)
                            returned = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                            detail = (arch, wb, name, kind, parameter, value, alignment, alias)
                            require(returned == int(valid) and bytes(machine.mem_read(BASE, 0x4000)) == expected, detail)
                            if not valid:
                                require(not writes, (detail, 'decline wrote'))
                            if kind == 'false-slot':
                                require(not any(address == BASE + 16 for address, size in reads), (detail, 'false target consulted'))
                            if arch == 'amd64':
                                require(machine.reg_read(UC_X86_REG_MXCSR) == 0x5fa5, 'MXCSR changed')
                            else:
                                require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and
                                        machine.reg_read(UC_ARM64_REG_FPSR) == 0x95, 'FP state changed')
                            checks += 1
        print('PASS', arch, 'integer branch slots/images/FP:', checks, 'cases', flush=True)
