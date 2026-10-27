#!/usr/bin/env python3
"""Execute string-owner alignment guards and forbid premature metadata reads."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR

ROOT = Path(__file__).resolve().parents[2]
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000


# Encode native slots independently of the generated instructions.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (8*width))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='string-owner-jit-') as directory:
    td = Path(directory)
    for arch in os.environ.get('STRING_OWNER_ARCHES', 'amd64 arm64').split():
        if arch not in ('amd64', 'arm64'):
            raise ValueError(arch)
        source = td/(arch+'.c')
        header = (Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')).resolve()
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Host null used by the emitter.
#include "isa.h"
#include HEADER


/* Stage actual string leaves with independent object and register offsets. */
int
emit(u32int *out, int wb, int operation)
{
    LeafLayout a = {0};
    int operations[] = {IINSC, ISLICEC, IADDC};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24;
    a.stringlen=0; a.stringmax=4; a.stringdata=24;
    a.heapref=24; a.refbytes=8;
    return leafemit(out,128,&a,operations[operation]);
}
'''.replace('HEADER', '"'+str(header)+'"'))
        library = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        subprocess.run(command+['-O1', '-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'),
                                str(source), '-o', str(library)], check=True)
        emit = C.CDLL(str(library)).emit
        emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.c_int]
        emit.restype = C.c_int
        checks = 0
        for width in (4, 8):
            for operation in map(int, os.environ.get('STRING_OWNER_OPERATIONS', '0 1 2').split()):
                if operation not in range(3):
                    raise ValueError(operation)
                code = (C.c_uint32*128)()
                size = emit(code, width, operation)
                if size <= 0:
                    raise RuntimeError((arch, width, operation, 'leaf unavailable'))
                raw = bytes(code)[:size*4]
                for wide in (0, 1):
                    for leftshift in range(16):
                        for rightshift in (range(16) if operation == 2 else (0,)):
                            for slotshift in range(8):
                                image = bytearray(0x8000)
                                owner, other = 0x3000+leftshift, 0x4000+rightshift
                                slot = 0x300+slotshift
                                put(image, 8, BASE+0x100)
                                put(image, 16, BASE+slot)
                                put(image, 24, BASE+(slot if operation == 2 else 0x200))
                                put(image, slot, BASE+owner)
                                put(image, owner-24, 1)
                                put(image, other-24, 1)
                                put(image, owner, -4 if wide else 4, 4)
                                put(image, other, -2 if wide else 2, 4)
                                put(image, owner+4, 16, 4)
                                put(image, other+4, 16, 4)
                                stride = 4 if wide else 1
                                for i in range(4):
                                    put(image, owner+24+i*stride, 65+i, stride)
                                for i in range(2):
                                    put(image, other+24+i*stride, 80+i, stride)
                                if operation == 0:
                                    put(image, 0x100, 90, width)
                                    put(image, 0x200, 1, width)
                                elif operation == 1:
                                    put(image, 0x100, 1, width)
                                    put(image, 0x200, 3, width)
                                else:
                                    put(image, 0x100, BASE+other)
                                expected = bytearray(image)
                                decline = leftshift%8 != 0 or (operation == 2 and rightshift%8 != 0)
                                if not decline:
                                    if operation == 0:
                                        put(expected, owner+24+stride, 90, stride)
                                    elif operation == 1:
                                        expected[owner+24:owner+24+2*stride] = image[owner+24+stride:owner+24+3*stride]
                                        put(expected, owner, -2 if wide else 2, 4)
                                    else:
                                        expected[owner+24+4*stride:owner+24+6*stride] = image[other+24:other+24+2*stride]
                                        put(expected, owner, -6 if wide else 6, 4)
                                machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
                                machine.mem_map(BASE, len(image))
                                machine.mem_map(CODE, 0x2000)
                                machine.mem_write(BASE, bytes(image))
                                machine.mem_write(CODE, raw)
                                writes = []

                                # No Heap/header/payload read may precede rejection of a packed owner.
                                def read_hook(uc, access, address, size, value, data):
                                    if decline and (BASE+owner-24 <= address < BASE+owner+128 or
                                                    BASE+other-24 <= address < BASE+other+128):
                                        raise RuntimeError((arch, operation, 'premature owner read', hex(address)))

                                # Capture data writes independently of the whole-image comparison.
                                def write_hook(uc, access, address, size, value, data):
                                    if BASE <= address < BASE+len(image):
                                        writes.append(address)

                                machine.hook_add(UC_HOOK_MEM_READ, read_hook)
                                machine.hook_add(UC_HOOK_MEM_WRITE, write_hook)
                                if arch == 'amd64':
                                    machine.reg_write(UC_X86_REG_RDI, BASE)
                                    machine.reg_write(UC_X86_REG_RSP, BASE+0x7000)
                                    machine.mem_write(BASE+0x7000, STOP.to_bytes(8, 'little'))
                                    expected[0x7000:0x7008] = STOP.to_bytes(8, 'little')
                                    resultreg = UC_X86_REG_RAX
                                else:
                                    machine.reg_write(UC_ARM64_REG_X0, BASE)
                                    machine.reg_write(UC_ARM64_REG_LR, STOP)
                                    resultreg = UC_ARM64_REG_X0
                                machine.emu_start(CODE, STOP, count=10000)
                                if machine.reg_read(resultreg) != (0 if decline else 1):
                                    raise RuntimeError((arch, width, operation, leftshift, rightshift, 'result'))
                                if bytes(machine.mem_read(BASE, len(image))) != bytes(expected) or (decline and writes):
                                    raise RuntimeError((arch, operation, 'changed image'))
                                checks += 1
        print(f'PASS {arch} string owner guards: {checks} machine/read/write/image cases')
