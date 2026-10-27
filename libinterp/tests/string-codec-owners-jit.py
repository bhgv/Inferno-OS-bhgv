#!/usr/bin/env python3
"""Execute codec result-owner guards while retaining packed borrowed inputs."""
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


with tempfile.TemporaryDirectory(prefix='string-codec-owners-jit-') as directory:
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
    int operations[] = {ICVTAC, ICVTCA};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24;
    a.stringlen=0; a.stringmax=4; a.stringdata=24;
    a.heapref=24; a.refbytes=8; a.heaptype=16; a.heapcolor=32;
    a.arraylen=0; a.arraytype=8; a.arrayroot=16; a.arraydata=24;
    a.typeref=0; a.typesize=24; a.typenp=28;
    a.stringtype=(void*)0xaaaa; a.arrayidentity=(void*)0xbbbb;
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
            for operation in map(int, os.environ.get('STRING_OWNER_OPERATIONS', '0 1').split()):
                if operation not in range(2):
                    raise ValueError(operation)
                code = (C.c_uint32*128)()
                size = emit(code, width, operation)
                if size <= 0:
                    raise RuntimeError((arch, width, operation, 'leaf unavailable'))
                raw = bytes(code)[:size*4]
                for sourceshift in range(8):
                    for leftshift in range(8):
                        for rightshift in map(int, os.environ.get('CODEC_JIT_TYPE_SHIFTS', '0 1 2 3 4 5 6 7').split()):
                            for slotshift in range(8):
                                image = bytearray(0x8000)
                                sourceowner, owner = 0x2000+sourceshift, 0x3000+leftshift
                                typ = 0x4000+rightshift
                                slot = 0x300+slotshift
                                put(image, 8, BASE+0x100)
                                put(image, 16, BASE+slot)
                                put(image, 0x100, BASE+sourceowner)
                                put(image, slot, BASE+owner)
                                put(image, sourceowner-24, 1)
                                put(image, sourceowner-16, 0xbbbb if operation == 0 else 0xaaaa)
                                put(image, owner-24, 1)
                                put(image, owner-16, 0xaaaa if operation == 0 else 0xbbbb)
                                put(image, typ, 1, 4)
                                put(image, typ+24, 1, 4)
                                put(image, typ+28, 0, 4)
                                if operation == 0:
                                    put(image, sourceowner, 4, width)
                                    put(image, sourceowner+8, BASE+typ)
                                    put(image, sourceowner+16, (1<<64)-1)
                                    put(image, sourceowner+24, BASE+0x5000)
                                    image[0x5000:0x5004] = b'abcd'
                                    put(image, owner, 0, 4)
                                    put(image, owner+4, 16, 4)
                                else:
                                    put(image, sourceowner, 4, 4)
                                    put(image, sourceowner+4, 4, 4)
                                    image[sourceowner+24:sourceowner+28] = b'abcd'
                                    put(image, owner, 16, width)
                                    put(image, owner+8, BASE+typ)
                                    put(image, owner+16, (1<<64)-1)
                                    put(image, owner+24, BASE+0x5000)
                                expected = bytearray(image)
                                decline = leftshift != 0 or (operation == 1 and rightshift != 0)
                                if not decline:
                                    if operation == 0:
                                        put(expected, owner, 4, 4)
                                        expected[owner+24:owner+28] = b'abcd'
                                    else:
                                        put(expected, owner, 4, width)
                                        expected[0x5000:0x5004] = b'abcd'
                                machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
                                machine.mem_map(BASE, len(image))
                                machine.mem_map(CODE, 0x2000)
                                machine.mem_write(BASE, bytes(image))
                                machine.mem_write(CODE, raw)
                                writes = []

                                # Packed result owners and releasable descriptors must decline before their reads.
                                def read_hook(uc, access, address, size, value, data):
                                    if leftshift and BASE+owner-24 <= address < BASE+owner+128:
                                        raise RuntimeError((arch, operation, 'premature owner read', hex(address)))
                                    if operation == 1 and rightshift and BASE+typ <= address < BASE+typ+64:
                                        raise RuntimeError((arch, operation, 'premature descriptor read', hex(address)))

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
        print(f'PASS {arch} codec owner guards: {checks} machine/read/write/image cases')
