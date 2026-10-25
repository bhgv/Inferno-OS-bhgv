#!/usr/bin/env python3
"""Check bounded list lengths against independent graph and complete image oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_ARCH_RISCV, UC_MODE_RISCV64, UC_MODE_64, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.riscv_const import UC_RISCV_REG_A0, UC_RISCV_REG_RA, UC_RISCV_REG_T0
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x100000000
CODE = 0x10000
STOP = 0x11000
MASK = (1 << 64)-1
PATTERN = bytes((i*37+11) & 255 for i in range(0x4000))


# Encode fields independently of native instruction choices.
def put(image, offset, value, size=8):
    image[offset:offset+size] = (value & ((1 << (size*8))-1)).to_bytes(size, 'little')


# Keep verification enabled under optimized Python.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='list-length-jit-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64', 'riscv64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null sentinel for emitter helpers.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Query the real LENL path without coupling the graph oracle to its instructions. */
int emit(u32int *out, int cap, int wb, int offset)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;a.listtail=offset;
 return leafemit(out,cap,&a,ILENL);
}
''')
        if arch == 'riscv64':
            source.write_text(source.read_text().replace('riscv64-emit.h', 'riscv-emit.h').replace('LeafLayout a={0};', 'RvLayout a={0};a.xlen=64;').replace('leafemit(', 'rvemit('))
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary)); lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0) if arch == 'arm64' else Uc(UC_ARCH_RISCV, UC_MODE_RISCV64)
        machine.mem_map(CODE, 0x3000); machine.mem_map(BASE, 0x8000)
        if arch == 'riscv64':
            csr = (0x000062b7, 0x3002a073, 0x0018d073, 0x0020d073, 0x003022f3)
            machine.mem_write(CODE+0x1800, b''.join(word.to_bytes(4, 'little') for word in csr))
            machine.emu_start(CODE+0x1800, CODE+0x1814, count=5)
            require(machine.reg_read(UC_RISCV_REG_T0) == 0x31, 'FCSR instruction probe')
        writes = []; reads = []


        # Track writes and graph-link reads independently, including same-value stores.
        def write(machine, access, address, size, value, context):
            writes.append((address, size))


        # The graph region contains only 16-byte nodes with their tail at offset eight.
        def read(machine, access, address, size, value, context):
            if BASE+0x1000 <= address < BASE+0x6000 and (address-BASE-0x1000)%16 == 8:
                reads.append(address)


        machine.hook_add(UC_HOOK_MEM_WRITE, write); machine.hook_add(UC_HOOK_MEM_READ, read)
        checks = 0
        for wb in (4, 8):
            out = (C.c_uint32*64)(); n = lib.emit(out, 64, wb, 8)
            require(0 < n <= 64 and lib.emit(None, 64, wb, 8) == n, (arch, wb, 'sizing', n))
            for cap in range(n):
                guard = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                require(lib.emit(guard, cap, wb, 8) == 0 and list(guard)[cap if arch == 'riscv64' else 0:] == [0xa5a5a5a5]*(64-(cap if arch == 'riscv64' else 0)), 'short publication')
            for offset in (-1, 1, 7, 32768):
                guard = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                require(lib.emit(guard, 64, wb, offset) == 0 and list(guard) == [0xa5a5a5a5]*64, 'invalid tail offset')
            machine.mem_write(CODE, bytes(out)[:n*4]); machine.ctl_remove_cache(CODE, CODE+256)
            for length in (0, 1, 2, 3, 7, 15, 31, 63, 127, 128, 129, 255, 256, 257, 513):
                for terminal in (-1, -2, 0, 1, length//2, max(0, length-1)):
                    for sa in range(8):
                        for da in range(8):
                            for alias in range(3):
                                image = bytearray(b'\xa5'*0x8000)
                                nodes = [BASE+0x1000+i*16 for i in range(length)]
                                end = MASK if terminal == -1 else 0 if terminal == -2 else nodes[terminal % length] if length else BASE+0x1000
                                # Nonempty graph back edges are independent of Brent checkpoints.
                                first = nodes[0] if nodes else end
                                for i, owner in enumerate(nodes):put(image, owner-BASE+8, nodes[i+1] if i+1 < length else end)
                                src = 0x100+sa; dst = 0x300+da
                                if alias == 1:dst = src
                                if alias == 2:dst = 0x1008+da
                                put(image, src, first); put(image, 8, BASE+src); put(image, 16, BASE+dst)
                                seen = set(); current = first; count = 0
                                while current not in (0, MASK) and current not in seen:
                                    # The zero-node nonterminal profile forms one explicit self-cycle.
                                    if not nodes:put(image, 0x1008, current)
                                    seen.add(current); count += 1
                                    current = int.from_bytes(image[current-BASE+8:current-BASE+16], 'little')
                                valid = current == MASK and count <= 256
                                expected = bytearray(image)
                                if valid:put(expected, dst, count, wb)
                                machine.mem_write(BASE, bytes(image)); writes.clear(); reads.clear()
                                if arch == 'amd64':
                                    machine.reg_write(UC_X86_REG_RDI, BASE); machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                    machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little')); machine.reg_write(UC_X86_REG_MXCSR, 0x3f80)
                                elif arch == 'arm64':
                                    machine.reg_write(UC_ARM64_REG_X0, BASE); machine.reg_write(UC_ARM64_REG_LR, STOP)
                                    machine.reg_write(UC_ARM64_REG_FPCR, 0x800000); machine.reg_write(UC_ARM64_REG_FPSR, 0x11)
                                else:
                                    machine.reg_write(UC_RISCV_REG_A0, BASE); machine.reg_write(UC_RISCV_REG_RA, STOP)
                                    machine.emu_start(CODE+0x1808, CODE+0x1810, count=2)
                                machine.emu_start(CODE, STOP, count=12000)
                                result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0 if arch == 'arm64' else UC_RISCV_REG_A0)
                                detail = (arch, wb, length, terminal, sa, da, alias)
                                require(result == valid and bytes(machine.mem_read(BASE, 0x8000)) == expected, (detail, 'result/image', result))
                                if valid:
                                    touched = [address+i for address, size in writes for i in range(size)]
                                    require(touched == list(range(BASE+dst, BASE+dst+wb)), (detail, 'stores'))
                                    require(len(reads) == count, (detail, 'link reads'))
                                else:require(not writes and len(reads) <= 256, (detail, 'decline'))
                                if current not in (0, MASK) and count < 16:
                                    require(len(reads) <= 3*count+1, (detail, 'early cycle rejection'))
                                if arch == 'amd64':require(machine.reg_read(UC_X86_REG_MXCSR) == 0x3f80, 'MXCSR')
                                elif arch == 'arm64':require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and machine.reg_read(UC_ARM64_REG_FPSR) == 0x11, 'FPCR/FPSR')
                                else:
                                    machine.emu_start(CODE+0x1810, CODE+0x1814, count=1)
                                    require(machine.reg_read(UC_RISCV_REG_T0) == 0x31, 'FCSR')
                                checks += 1
        print('PASS', arch, 'list lengths:', checks, 'graph/image/read/write/FP cases')
