#!/usr/bin/env python3
"""Check integer branch preflight and exact native images on AMD64 and ARM64."""
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64
from unicorn import UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RSP, UC_X86_REG_RAX, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]  # Explicit production source root.
BASE = 0x100000000  # Independent guest register/data mapping.
CODE = 0x10000  # Guest instruction mapping.
STOP = CODE + 0x1000  # Return sentinel, outside every emitted leaf.
MASK = (1 << 64) - 1  # Native pointer limit.
OPS = tuple('B' + relation + kind for kind in ('B', 'W', 'L')
            for relation in ('EQ', 'NE', 'LT', 'LE', 'GT', 'GE')) + ('JMP',)  # Checked paths.


def require(condition, detail):
    """Raise on a false evaluated predicate, including under -O; never block."""
    if not condition:
        raise RuntimeError(detail)


def put(image, offset, value, width=8):
    """Write width little-endian bytes to the owned image at offset, modulo width."""
    image[offset:offset + width] = (value & ((1 << (8 * width)) - 1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='branch-slot-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('BRANCH_SLOT_ARCHES', 'amd64 arm64').split():
        header = Path(os.environ.get('BRANCH_SLOT_EMITTER_DIR', ROOT / 'libinterp')) / (arch + '-emit.h')
        source = td / (arch + '.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include ''' + json.dumps(str(header)) + '''
/* emit borrows out (cap code units), wb is 4/8 and op indexes eighteen branches and JMP.
 * Return the published/sized length, or zero without stores on invalid capacity.
 * Layout is independent of production REG; no ownership or blocking is involved.
 */
int emit(u32int *out, int cap, int wb, int op)
{
    LeafLayout a = {0};
    static int ops[] = {OPERATIONS};
    a.wordbytes = wb;
    a.pc = 0;
    a.s = 8;
    a.d = 16;
    a.m = 24;
    return leafemit(out, cap, &a, ops[op]);
}
'''.replace('{OPERATIONS}', '{' + ','.join('I' + op for op in OPS) + '}'))
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


        def read_hook(machine, access, address, size, value, context):
            """Record actual guest data reads; callback borrows the emulator only."""
            reads.append((address, size))


        def write_hook(machine, access, address, size, value, context):
            """Record every guest store, including same-value writes on decline."""
            writes.append((address, size))


        machine.hook_add(UC_HOOK_MEM_READ, read_hook)
        machine.hook_add(UC_HOOK_MEM_WRITE, write_hook)
        checks = 0
        for wb in (4, 8):
            for op, name in enumerate(OPS):
                if os.environ.get('BRANCH_SLOT_OP') and op != int(os.environ['BRANCH_SLOT_OP']):
                    continue
                jump = op == 18
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
                badtarget = [0, MASK] + [MASK - i for i in range(1, 8)]
                badsource = [0, MASK] + [MASK - i for i in range(1, width)]
                cases = [('slot', 'd', pointer) for pointer in badtarget]
                if jump:
                    cases += [('unused', pointer, 0) for pointer in badtarget + [1]]
                else:
                    cases += [('slot', slot, pointer) for slot in ('s', 'm') for pointer in badsource]
                    cases += [('false-slot', 'd', pointer) for pointer in badtarget + [1]]
                    edges = (0, 1, 127, 128, 255, 256, 0x7fffffff, 0x80000000,
                             0xffffffff, 0x100000000, 0x7fffffffffffffff, 1 << 63, MASK - 1, MASK)
                    cases += [('compare', a, b) for a in edges for b in edges]
                for kind, parameter, value in cases:
                    for alignment in range(8):
                        aliases = (0, 1, 2, 3) if kind == 'compare' and value in (0, MASK - 1, MASK) else (0,)
                        for alias in aliases:
                            image = bytearray(b'\xa5' * 0x4000)
                            source, middle, dest = BASE + 0x100 + alignment, BASE + 0x200 + alignment, BASE + 0x300 + alignment
                            true_pairs = ((1,1), (1,2), (1,2), (1,1), (2,1), (1,1))
                            false_pairs = ((1,2), (1,1), (2,1), (2,1), (1,2), (1,2))
                            left, right = false_pairs[op % 6] if kind == 'false-slot' else true_pairs[op % 6]
                            if kind == 'compare':
                                left, right = parameter & mask, value & mask
                                if width > 1:
                                    if left >> (width * 8 - 1):
                                        left -= 1 << (width * 8)
                                    if right >> (width * 8 - 1):
                                        right -= 1 << (width * 8)
                            take = jump or (left == right, left != right, left < right, left <= right,
                                            left > right, left >= right)[op % 6]
                            put(image, 0x100 + alignment, left, width)
                            put(image, 0x200 + alignment, right, width)
                            put(image, 0x300 + alignment, BASE + 0x3800)
                            valid = kind != 'slot'
                            if kind in ('slot', 'false-slot'):
                                if parameter == 's':
                                    source, middle = value, 1
                                elif parameter == 'm':
                                    middle, source = value, 1
                                else:
                                    dest = value
                            if jump:
                                source = middle = parameter if kind == 'unused' else 1
                            if alias:
                                dest = source if alias == 1 else middle if alias == 2 else BASE
                            put(image, 0, BASE + 0x3900)
                            put(image, 8, source)
                            put(image, 16, dest)
                            put(image, 24, middle)
                            expected = bytearray(image)
                            if valid and take:
                                target = int.from_bytes(image[dest - BASE:dest - BASE + 8], 'little')
                                # Aliasing may expose nil/H as the captured PC. The
                                # production direct-target guard declines before any store;
                                # MASK-1 remains an accepted opaque non-sentinel token.
                                valid = target not in (0, MASK)
                                if valid:
                                    put(expected, 0, target)
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
                            expected_writes = [(BASE, 8)] if valid and take else []
                            require(writes == expected_writes, (detail, 'write boundary/order', writes))
                            if not take:
                                require(not any(address == BASE + 16 for address, size in reads), (detail, 'unused target consulted'))
                            if jump:
                                require(not any(address in (BASE + 8, BASE + 24) for address, size in reads), (detail, 'JMP source consulted'))
                            if arch == 'amd64':
                                require(machine.reg_read(UC_X86_REG_MXCSR) == 0x5fa5, 'MXCSR changed')
                            else:
                                require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and
                                        machine.reg_read(UC_ARM64_REG_FPSR) == 0x95, 'FP state changed')
                            checks += 1
        print('PASS', arch, 'integer branch/PC/nonaccess/FP:', checks, 'cases', flush=True)
