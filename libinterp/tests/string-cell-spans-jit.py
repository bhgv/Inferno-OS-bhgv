#!/usr/bin/env python3
"""Execute string cell and slice leaves at native address boundaries."""
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
    """Encode scalar fields independently of native instruction selection."""
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='string-cell-spans-') as directory:
    td = Path(directory)
    for arch in os.environ.get('CELL_ARCHES', 'arm64 amd64').split():
        header = Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null pointer used by the production emitter.
#include "isa.h"
#include HEADER


/* Stage each actual leaf with adjacent or nonadjacent string metadata. */
int
emit(u32int *out, int wb, int kind, int layout)
{
    LeafLayout a = {0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.stringlen=layout ? 12 : 0; a.stringmax=4;
    a.stringdata=layout ? 32 : 24; a.heapref=24; a.refbytes=8;
    return leafemit(out,128,&a,kind == 0 ? IINDC : kind == 1 ? IINSC : ISLICEC);
}
'''.replace('HEADER', '"'+str(header)+'"'))
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'),
                               '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        library = C.CDLL(str(binary))
        library.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000); machine.mem_map(BASE, 0x4000)
        machine.mem_map(HIGH, 0x1000)
        physical = (1 << 52)-0x1000 if arch == 'amd64' else HIGH
        if physical != HIGH:
            machine.mem_map(physical, 0x1000)
        writes = []
        probing = [False]


        def written(machine, access, address, size, value, context):
            """Detect even same-value writes on declined paths."""
            writes.append((address, size))


        # No rejected slot/owner probe may inspect a String or reference cell.
        def readprobe(machine, access, address, size, value, context):
            if probing[0] and not (BASE <= address and address+size <= BASE+0x400) and address != CODE+0x2800:
                raise RuntimeError((arch, 'read before cell preflight', address, size))


        machine.hook_add(UC_HOOK_MEM_WRITE, written)
        machine.hook_add(UC_HOOK_MEM_READ, readprobe)
        checks = [0, 0, 0]
        for wb in (4, 8):
            for layout in (0, 1):
                lenoff, dataoff = (12, 32) if layout else (0, 24)
                offsets = (0xf00, 0xfd0, 0xfe0) if layout else (0xf00, 0xfe0, 0xfe8, 0xff0, 0xff8)
                for kind in map(int, os.environ.get('CELL_KINDS', '0 1 2').split()):
                    output = (C.c_uint32*128)()
                    count = library.emit(output, wb, kind, layout)
                    if not 0 < count <= 128:
                        raise RuntimeError((arch, wb, kind, layout, 'leaf unavailable'))
                    machine.mem_write(CODE, bytes(output)[:count*4])
                    machine.ctl_remove_cache(CODE, CODE+512)
                    if kind < 2:
                        cases = [(length, index, value) for length in (1, 7, 128, 0x7fffffff)
                                 for index in (-1, 0, 1, 7, 127, 128, 0x7ffffffe, 0x7fffffff)
                                 for value in ((0,) if kind == 0 else (0x41, 0x7f, 0x80, 0x80000000, 0x100000041))]
                    else:
                        cases = [(0x7fffffff, start, end) for start, end in
                                 ((-1, 1), (1, 0), (0, 1), (0, 128), (0, 0x7fffffff),
                                  (1, 2), (1, 129), (1, 257), (1, 258), (7, 8),
                                  (7, 263), (7, 264), (0x7ffffffe, 0x7fffffff))]
                    if os.environ.get("CELL_PROBE_ONLY"):
                        cases = []
                    for wide in (0, 1):
                        stride = 4 if wide else 1
                        for offset in offsets:
                            for length, index, value in cases:
                                image = bytearray(b'\xa5'*0x4000)
                                top = bytearray(b'\x5a'*0x1000)
                                owner = HIGH+offset
                                put(top, offset-24, 1)
                                put(top, offset+lenoff, -length if wide else length, 4)
                                put(top, offset+4, 0x7fffffff, 4)
                                put(image, 0x100, owner if kind == 0 else value if kind == 1 else index, 8 if kind == 0 else wb)
                                put(image, 0x200, index if kind < 2 else value, wb)
                                put(image, 0x300, 0x13579 if kind == 0 else owner)
                                put(image, 8, BASE+0x100); put(image, 24, BASE+0x200); put(image, 16, BASE+0x300)
                                expected, expectedtop = bytearray(image), bytearray(top)
                                end = owner+dataoff+(index+1)*stride
                                if kind == 0:
                                    valid = 0 <= index < length and end <= MASK
                                    if valid:
                                        begin = owner+dataoff+index*stride-HIGH
                                        scalar = int.from_bytes(top[begin:begin+stride], 'little', signed=bool(wide))
                                        put(expected, 0x300, scalar, wb)
                                elif kind == 1:
                                    rune = value & 0xffffffff
                                    valid = 0 <= index <= length and index < 0x7fffffff and (wide or rune < 128) and end <= MASK
                                    if valid:
                                        put(expectedtop, owner+dataoff+index*stride-HIGH, rune, stride)
                                        if index == length:
                                            put(expectedtop, offset+lenoff, -(length+1) if wide else length+1, 4)
                                else:
                                    valid = 0 <= index < value <= length and (index == 0 or value-index <= 256)
                                    if index:
                                        valid = valid and owner+dataoff+value*stride <= MASK
                                    if valid:
                                        copied = value-index
                                        if index:
                                            expectedtop[offset+dataoff:offset+dataoff+copied*stride] = top[offset+dataoff+index*stride:offset+dataoff+value*stride]
                                        put(expectedtop, offset+lenoff, -copied if wide else copied, 4)
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
                                    raise RuntimeError((arch, wb, layout, kind, wide, offset, length, index, value, result, valid, 'span mismatch'))
                                if not valid and writes:
                                    raise RuntimeError('declined cell operation wrote memory')
                                checks[kind] += 1
        for layout in (0, 1):
            lenoff, dataoff = (12, 32) if layout else (0, 24)
            for wb in (4, 8):
                for kind in map(int, os.environ.get('CELL_PROBE_KINDS', '0 1').split()):
                    output = (C.c_uint32*128)()
                    units = library.emit(output, wb, kind, layout)
                    if units <= 0:
                        raise RuntimeError((arch, wb, kind, layout, 'missing preflight leaf'))
                    machine.mem_write(CODE, bytes(output)[:units*4])
                    machine.ctl_remove_cache(CODE, CODE+512)
                    probes = [(slot, pointer) for slot in ('source', 'index', 'destination')
                              for pointer in (0, MASK, MASK-1, MASK-2)]
                    probes += [('owner', pointer) for pointer in [0, MASK]+[MASK-i for i in range(1, 16)]+
                               (list(range(1, 25)) if kind else [])]
                    family = os.environ.get('CELL_PROBE_FAMILY')
                    for slot, pointer in probes:
                        if family and (slot == 'owner') != (family == 'owners'):
                            continue
                        image = bytearray(b'\xa5'*0x4000)
                        put(image, 8, BASE+0x100); put(image, 24, BASE+0x200); put(image, 16, BASE+0x300)
                        put(image, 0x100, 65 if kind else BASE+0x800, wb if kind else 8)
                        put(image, 0x200, 0, wb)
                        put(image, 0x300, BASE+0x800 if kind else 77, 8 if kind else wb)
                        put(image, 0x800-24, 1)
                        put(image, 0x800+lenoff, 1, 4); put(image, 0x804, 8, 4)
                        if slot == 'owner':
                            put(image, 0x300 if kind else 0x100, pointer)
                        else:
                            put(image, {'source': 8, 'index': 24, 'destination': 16}[slot], pointer)
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
                            raise RuntimeError((arch, wb, kind, layout, slot, pointer, 'preflight publication'))
                        checks[kind] += 1
        print('PASS', arch, 'INDC/INSC/SLICEC spans:', checks, 'address/image/write checks', flush=True)
