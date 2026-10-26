#!/usr/bin/env python3
"""Check shift slot preflight and exact native images on AMD64 and ARM64."""
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
OPS = ('SHLB', 'SHRB', 'SHLW', 'SHRW', 'LSRW', 'SHLL', 'SHRL', 'LSRL')  # Tested ISA names.


def require(condition, detail):
    """Raise on a false evaluated predicate, including under -O; never block."""
    if not condition:
        raise RuntimeError(detail)


def put(image, offset, value, width=8):
    """Write width little-endian bytes to the owned image at offset, modulo width."""
    image[offset:offset + width] = (value & ((1 << (8 * width)) - 1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='shift-slot-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('SHIFT_SLOT_ARCHES', 'amd64 arm64').split():
        header = Path(os.environ.get('SHIFT_SLOT_EMITTER_DIR', ROOT / 'libinterp')) / (arch + '-emit.h')
        source = td / (arch + '.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include ''' + json.dumps(str(header)) + '''
/* emit borrows out (cap code units), wb is 4/8 and op indexes the eight shifts.
 * Return the published/sized length, or zero without stores on invalid capacity.
 * Layout is independent of production REG; no ownership or blocking is involved.
 */
int emit(u32int *out, int cap, int wb, int op)
{
    LeafLayout a = {0};
    static int ops[] = {ISHlb};
    a.wordbytes = wb;
    a.pc = 0;
    a.s = 8;
    a.d = 16;
    a.m = 24;
    return leafemit(out, cap, &a, ops[op]);
}
'''.replace('{ISHlb}', '{' + ','.join('I' + op for op in OPS) + '}'))
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
                if os.environ.get('SHIFT_SLOT_OP') and op != int(os.environ['SHIFT_SLOT_OP']):
                    continue
                width = 1 if name.endswith('B') else wb if name.endswith('W') else 8
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
                bad = [0, MASK] + [MASK - i for i in range(1, 9)]
                cases = [('slot', slot, pointer) for slot in ('s', 'm', 'd') for pointer in bad]
                cases += [('count', count, 0) for count in (-1, width * 8, width * 8 + 1, 1 << 31)]
                cases += [('value', count, value) for count in range(width * 8)
                          for value in (0, 1, (1 << (width * 8 - 1)), (1 << (width * 8)) - 1)]
                for kind, parameter, value in cases:
                    for alignment in range(8):
                        image = bytearray(b'\xa5' * 0x4000)
                        source, middle, dest = BASE + 0x100 + alignment, BASE + 0x200 + alignment, BASE + 0x300 + alignment
                        count = parameter if kind in ('value', 'count') else 1
                        put(image, 0x100 + alignment, count, wb)
                        put(image, 0x200 + alignment, value, width)
                        if kind == 'slot':
                            if parameter == 's':
                                source = value
                            elif parameter == 'm':
                                middle = value
                            else:
                                dest = value
                        if kind == 'count':
                            middle = dest = 1  # Unmapped but non-null: must never be read.
                        put(image, 0, BASE + 0x3800)
                        put(image, 8, source)
                        put(image, 16, dest)
                        put(image, 24, middle)
                        valid = kind == 'value' or (kind == 'slot' and value not in (0, MASK) and
                                value <= MASK - (wb if parameter == 's' else width))
                        # Nonwrapping high pointers are deliberately omitted, not treated as mapped.
                        if kind == 'slot' and valid:
                            continue
                        expected = bytearray(image)
                        if valid:
                            bits = width * 8
                            signed = value - (1 << bits) if value & (1 << (bits - 1)) else value
                            result = value << count if name.startswith('SHL') else (
                                signed >> count if name.startswith('SHR') and width != 1 else value >> count)
                            put(expected, 0x300 + alignment, result, width)
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
                        detail = (arch, wb, name, kind, parameter, value, alignment)
                        require(returned == int(valid) and bytes(machine.mem_read(BASE, 0x4000)) == expected, detail)
                        if not valid:
                            require(not writes, (detail, 'decline wrote'))
                            if kind == 'slot' and parameter in ('s', 'd'):
                                require(not any(address == middle for address, size in reads), (detail, 'early value read'))
                        if arch == 'amd64':
                            require(machine.reg_read(UC_X86_REG_MXCSR) == 0x5fa5, 'MXCSR changed')
                        else:
                            require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and
                                    machine.reg_read(UC_ARM64_REG_FPSR) == 0x95, 'FP state changed')
                        checks += 1
        print('PASS', arch, 'shift slot/image/read/write/count/FP:', checks, 'cases', flush=True)
