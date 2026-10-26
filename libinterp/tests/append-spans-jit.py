#!/usr/bin/env python3
"""Exercise ADDC spans at native address boundaries using actual emitted code."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR

ROOT = Path(__file__).resolve().parents[2]
MASK = (1 << 64)-1
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
HIGH = (1 << 64)-0x1000


def put(image, offset, value, width=8):
    """Store independently specified metadata and native operand slots."""
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='append-spans-jit-') as directory:
    td = Path(directory)
    for arch in os.environ.get('APPEND_ARCHES', 'arm64 amd64').split():
        header = Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/(arch+'.c')
        source.write_text("""#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null used by the production emitter.
#include "isa.h"
#include HEADER

/* Emit the production append leaf using independent object offsets. */
int emit(u32int *out, int wordbytes, int layout)
{
    LeafLayout a = {0};
    a.wordbytes=wordbytes; a.s=8; a.d=16; a.m=24;
    a.stringlen=0; a.stringmax=4; a.stringdata=24;
    a.heapref=24; a.refbytes=8;
    if(layout == 1){
        a.stringlen=12; a.stringdata=32;
    }
    if(layout == 2){
        a.stringlen=120; a.stringdata=124; a.heapref=128;
    }
    return leafemit(out,128,&a,IADDC);
}
""".replace("HEADER", chr(34)+str(header)+chr(34)))
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'),
                               '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        library = C.CDLL(str(binary))
        library.emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.c_int]
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x4000)
        machine.mem_map(HIGH, 0x1000)
        physical = (1 << 52)-0x1000 if arch == 'amd64' else HIGH
        if physical != HIGH:
            machine.mem_map(physical, 0x1000)
        writes = []
        probing = [False]


        def written(machine, access, address, size, value, context):
            """Observe even same-value writes on rejection paths."""
            writes.append((address, size))


        # Rejected slots/owners must not trigger any object or reference-cell read.
        def readprobe(machine, access, address, size, value, context):
            if probing[0] and not (BASE <= address and address+size <= BASE+0x400) and address != CODE+0x2800:
                raise RuntimeError((arch, 'read before preflight', hex(address), size))


        machine.hook_add(UC_HOOK_MEM_WRITE, written)
        machine.hook_add(UC_HOOK_MEM_READ, readprobe)
        checks = 0
        for layout in (0, 1, 2):
            lenoff, dataoff = (120, 124) if layout == 2 else (12, 32) if layout else (0, 24)
            reference = 128 if layout == 2 else 24
            for wb in (4, 8):
                output = (C.c_uint32*128)()
                count = library.emit(output, wb, layout)
                if not 0 < count <= 128:
                    raise RuntimeError((arch, 'append unavailable'))
                machine.mem_write(CODE, bytes(output)[:count*4])
                machine.ctl_remove_cache(CODE, CODE+512)
                for side in (0, 1):
                    for wide in (0, 1):
                        for offset in (() if os.environ.get('APPEND_PROBE_ONLY') else (0xf00, 0xf80) if layout == 2 else (0xf00, 0xfd0, 0xfe0) if layout else (0xf00, 0xfd0, 0xfe0, 0xfe8, 0xff0, 0xff8)):
                            for leftlen in (1, 7, 127, 256, 0x7fffffff):
                                for rightlen in (1, 7, 128, 256, 257):
                                    image = bytearray(b'\xa5'*0x4000)
                                    top = bytearray(b'\x5a'*0x1000)
                                    left = HIGH+offset if side == 0 else BASE+0x800
                                    right = HIGH+offset if side == 1 else BASE+0x1800
                                    for owner, length in ((left, leftlen), (right, rightlen)):
                                        target, index = (top, owner-HIGH) if owner >= HIGH else (image, owner-BASE)
                                        put(target, index-reference, 1)
                                        put(target, index+lenoff, -length if wide else length, 4)
                                        put(target, index+4, 0x7fffffff, 4)
                                    put(image, 0x100, left); put(image, 0x200, right)
                                    put(image, 8, BASE+0x200); put(image, 16, BASE+0x100); put(image, 24, BASE+0x100)
                                    stride = 4 if wide else 1
                                    start = left+dataoff+leftlen*stride
                                    end = start+rightlen*stride
                                    origin = right+dataoff
                                    sourceend = origin+rightlen*stride
                                    valid = rightlen <= 256 and leftlen+rightlen <= 0x7fffffff and end <= MASK and sourceend <= MASK
                                    expected, expectedtop = bytearray(image), bytearray(top)
                                    if valid:
                                        payload = (top if side else image)
                                        index = origin-(HIGH if side else BASE)
                                        data = bytes(payload[index:index+rightlen*stride])
                                        target = expectedtop if side == 0 else expected
                                        index = start-(HIGH if side == 0 else BASE)
                                        target[index:index+len(data)] = data
                                        put(target, left+lenoff-(HIGH if side == 0 else BASE), -(leftlen+rightlen) if wide else leftlen+rightlen, 4)
                                    if len(image) != 0x4000 or len(top) != 0x1000:
                                        raise RuntimeError((arch, wb, side, wide, offset, leftlen, rightlen, len(image), len(top), "fixture image grew"))
                                    machine.mem_write(BASE, bytes(image)); machine.mem_write(HIGH, bytes(top))
                                    if physical != HIGH:
                                        machine.mem_write(physical, bytes(top))
                                    writes.clear()
                                    if arch == 'amd64':
                                        machine.reg_write(UC_X86_REG_RDI, BASE)
                                        machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                        machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little'))
                                    else:
                                        machine.reg_write(UC_ARM64_REG_X0, BASE)
                                        machine.reg_write(UC_ARM64_REG_LR, STOP)
                                    machine.emu_start(CODE, STOP, count=12000)
                                    result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                                    if result != valid or bytes(machine.mem_read(BASE, 0x4000)) != expected or bytes(machine.mem_read(physical, 0x1000)) != expectedtop:
                                        raise RuntimeError((arch, wb, side, wide, offset, leftlen, rightlen, 'span mismatch'))
                                    if not valid and writes:
                                        raise RuntimeError('declined append wrote memory')
                                    checks += 1
                invalid = [0, MASK]+[MASK-i for i in range(1, 8)]
                probes = [(kind, pointer) for kind in ('source-slot', 'result-slot') for pointer in invalid]
                probes += [(kind, pointer) for kind in ('left-owner', 'right-owner')
                           for pointer in invalid+list(range(1, reference+1))]
                selected = os.environ.get('APPEND_PROBE_KIND')
                for kind, pointer in probes:
                    if selected and kind != selected:
                        continue
                    image = bytearray(b'\xa5'*0x4000)
                    put(image, 8, BASE+0x200)
                    put(image, 16, BASE+0x100)
                    put(image, 24, BASE+0x100)
                    put(image, 0x100, BASE+0x800)
                    put(image, 0x200, BASE+0x1800)
                    for owner in (0x800, 0x1800):
                        put(image, owner-reference, 1)
                        put(image, owner+lenoff, 1, 4)
                        put(image, owner+4, 8, 4)
                    if kind == 'source-slot':
                        put(image, 8, pointer)
                    elif kind == 'result-slot':
                        put(image, 16, pointer); put(image, 24, pointer)
                    else:
                        put(image, 0x100 if kind == 'left-owner' else 0x200, pointer)
                    machine.mem_write(BASE, bytes(image))
                    writes.clear()
                    if arch == 'amd64':
                        machine.reg_write(UC_X86_REG_RDI, BASE)
                        machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                        machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little'))
                    else:
                        machine.reg_write(UC_ARM64_REG_X0, BASE)
                        machine.reg_write(UC_ARM64_REG_LR, STOP)
                    probing[0] = True
                    machine.emu_start(CODE, STOP, count=12000)
                    probing[0] = False
                    result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                    if result != 0 or writes or bytes(machine.mem_read(BASE, 0x4000)) != image:
                        raise RuntimeError((arch, wb, layout, kind, pointer, 'preflight publication'))
                    checks += 1
        print('PASS', arch, 'append spans:', checks, 'address/image/write checks', flush=True)
