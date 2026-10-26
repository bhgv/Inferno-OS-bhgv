#!/usr/bin/env python3
"""Execute native empty/identity string case paths and preserve all fallback images."""
import ctypes as C
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ
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


with tempfile.TemporaryDirectory(prefix='string-case-jit-') as directory:
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


/* Emit bounded string case leaves with compact or distant metadata fields. */
int
emit(u32int *out, int cap, int wb, int operation, int large)
{
    int ops[] = {ICASEC};
    LeafLayout a = {0};
    a.wordbytes = wb; a.pc = 0; a.s = 8; a.d = 16; a.m = 24;
    a.module = 40; a.linkmodule = large ? 256 : 8;
    a.linkcompiled = large ? 512 : 16;
    a.moduleprog = large ? 256 : 16; a.modulenprog = large ? 512 : 24;
    a.instbytes = 24; a.stringlen = large ? 256 : 12; a.stringmax = large ? 512 : 4;
    if(operation == 1)a.stringlen = -1;
    if(operation == 2)a.stringmax = a.stringlen;
    if(operation == 3)a.stringmax = 2048;
    if(operation == 4)a.stringlen = 1;
    return leafemit(out, cap, &a, ops[0]);
}
'''.replace('HEADER', json.dumps(os.environ.get('STRING_CASE_'+arch.upper()+'_EMITTER', arch+'-emit.h'))))
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
            reads = {'allowed': set(), 'owner': BASE+0x6200, 'end': BASE+0x7000}


            # Permit only exact String header reads inside the two synthetic object regions.
            def stringread(uc, access, address, size, value, context):
                if reads.get('upper') is not None:
                    upper, width = reads['upper']
                    require(address+size <= upper or address >= upper+width, ('unexpected upper-label read', arch, cells, address, size))
                if reads['owner'] <= address < reads['end']:
                    require((address, size) in reads['allowed'], ('unexpected String payload read', arch, cells, address, size))


            machine.hook_add(UC_HOOK_MEM_READ, stringread)
            for wb in (4, 8):
                for large in (0, 1):
                    for op in (0,):
                        buffer = (C.c_uint32*128)()
                        units = lib.emit(buffer, 128, wb, op, large)
                        require(units > 0, ('missing native path', arch, cells, wb, large, op))
                        for invalid in range(1, 5):
                            guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                            require(lib.emit(guard, 128, wb, invalid, large) == 0 and list(guard) == [0xa5a5a5a5]*128, ('layout rejection', arch, cells, wb, large, invalid))
                        guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                        require(lib.emit(guard, units-1, wb, 0, large) == 0 and list(guard) == [0xa5a5a5a5]*128, ('capacity rejection', arch, cells, wb, large))
                        machine.mem_write(CODE, bytes(buffer)[:units*4])
                        machine.ctl_remove_cache(CODE, CODE+0x2000)
                        cases = [(count, value, default, alignment, None)
                                 for count in (0, 1, 2, 5, 8, 127)
                                 for value in tuple(range(16))+(-2, -1, 31, 32, 1 << 31, 1 << 40)
                                 for default in (-1, 0, 31, 32)
                                 for alignment in (0, 1, 7)]
                        cases += [(1, 2, 31, 1, bad) for bad in (
                            's-nil', 's-H', 's-wrap', 'link-nil', 'link-H', 'link-wrap',
                            'module-nil', 'module-H', 'module-wrap', 'prog-nil', 'prog-H',
                            'prog-wrap', 'table-nil', 'table-H', 'table-wrap', 'table-narrow', 'table-count4', 'table-count5', 'table-count6', 'table-count7', 'owner-source', 'owner-low', 'compiled',
                            'count-negative', 'count-limit', 'count-huge', 'count-upper', 'nprog-negative', 'nprog-zero',
                            'out-nil', 'out-H', 'out-wrap')]
                        probe = os.environ.get('STRING_CASE_PROBE')
                        if probe == 'wrap':
                            cases = [case for case in cases if case[-1] and case[-1].startswith('table-count')]
                        if probe == 'narrow':
                            cases = [case for case in cases if case[-1] == 'table-narrow']
                        for count, rawvalue, default, alignment, bad in cases:
                            memory = bytearray([0xa5])*0x8000
                            link, module, table = BASE+0x1000, BASE+0x2000, BASE+0x3000+alignment
                            sourceptr, output = BASE+0x5000+alignment, BASE+0x6000+alignment
                            put(memory, 0, 0x123456789)
                            put(memory, 8, sourceptr)
                            put(memory, 16, output if op == 3 else table)
                            put(memory, 24, 1)
                            put(memory, 40, link)
                            put(memory, 0x1000+(256 if large else 8), module)
                            put(memory, 0x1000+(512 if large else 16), 0, 4)
                            put(memory, 0x2000+(256 if large else 16), BASE+0x7000)
                            put(memory, 0x2000+(512 if large else 24), 32, 4)
                            target = default
                            start = table-BASE
                            # Empty values may have separate owners; malformed headers must still decline.
                            owner = BASE+0x6200
                            other = owner+0x800
                            lengthoffset = 256 if large else 12
                            maxoffset = 512 if large else 4
                            mode = rawvalue % 16
                            pointer = MASK if mode in (2, 8, 11) else 0 if mode == 3 else owner
                            lower = other if mode in (6, 9, 13, 14) else MASK if mode in (2, 7, 10) else 0 if mode == 12 else owner
                            emptyvalue = mode >= 7
                            length = 0 if emptyvalue else -1 if mode == 1 else -0x80000000 if mode == 4 else 1
                            capacity = -1 if mode in (10, 11, 14) else 0 if mode == 5 or mode == 15 else 3 if emptyvalue else 1
                            put(memory, sourceptr-BASE, pointer)
                            put(memory, owner-BASE+lengthoffset, length, 4)
                            put(memory, owner-BASE+maxoffset, capacity, 4)
                            put(memory, other-BASE+lengthoffset, 1 if mode == 6 else 0, 4)
                            put(memory, other-BASE+maxoffset, -1 if mode == 13 else 3, 4)
                            put(memory, start, count, cells)
                            for row in range(count):
                                put(memory, start+(1+3*row)*cells, lower if row == count//2 else 0, cells)
                                put(memory, start+(2+3*row)*cells, 1, cells)
                                put(memory, start+(3+3*row)*cells, default if row == count//2 else 32, cells)
                            put(memory, start+(1+3*count)*cells, default, cells)
                            target = default
                            valid = 0 <= target < 32 and (count == 0 or count > 0 and cells == 8 and mode in (0, 1, 2, 7, 8, 9, 15))
                            if bad:
                                kind, _, issue = bad.partition('-')
                                poisoned = 0 if issue == 'nil' else MASK if issue == 'H' else MASK-int(issue[-1]) if cells == 8 and issue.startswith('count') else MASK-2
                                if kind == 's':
                                    put(memory, 8, poisoned); valid = False
                                elif kind == 'link':
                                    put(memory, 40, poisoned); valid = False
                                elif kind == 'module':
                                    put(memory, 0x1000+(256 if large else 8), poisoned); valid = False
                                elif kind == 'prog':
                                    put(memory, 0x2000+(256 if large else 16), poisoned); valid = False
                                elif kind == 'table' and op != 3:
                                    if issue == 'narrow' and cells == 4:
                                        poisoned = BASE+0x7ffc
                                        put(memory, 0x7ffc, 1, 4)
                                    put(memory, 16, poisoned); valid = False
                                elif kind == 'compiled' and op != 3:
                                    put(memory, 0x1000+(512 if large else 16), 1, 4); valid = False
                                elif kind == 'owner':
                                    put(memory, sourceptr-BASE if issue == 'source' else start+cells, MASK-2, 8 if issue == 'source' else cells)
                                    valid = False
                                elif kind == 'count' and op != 3:
                                    put(memory, start, -1 if issue == 'negative' else 4096 if issue == 'limit' else ((1 << 32)+1) if issue == 'upper' else 0x7fffffff, cells)
                                    valid = op == 2 and issue == 'huge'
                                elif kind == 'nprog':
                                    put(memory, 0x2000+(512 if large else 24), -1 if issue == 'negative' else 0, 4); valid = False
                                elif kind == 'out' and op == 3:
                                    put(memory, 16, poisoned); valid = False
                            expected = bytearray(memory)
                            if valid:
                                put(expected, 0, BASE+0x7000+target*24)
                            reads['upper'] = (table+(2+3*(count//2))*cells, cells) if count else None
                            reads['allowed'] = {(base+field, 4) for base in (owner, other) for field in (lengthoffset, maxoffset)}
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
            print('PASS string case:', arch, 'cell', cells, flush=True)
    print('PASS string case:', total, 'WORD/cell/layout/range/error/FP cases')
