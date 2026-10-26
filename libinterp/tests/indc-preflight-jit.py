#!/usr/bin/env python3
"""Execute production AMD64/ARM64 INDC leaves with forbidden-read and write-free decline probes."""
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
MASK = (1 << 64)-1 # Native address domain, independent of VM WORD width.
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000 # Private fixture data and return addresses.


# Store little-endian scalar fields without depending on generated instruction selection.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


# Compile and run actual guarded leaves; external headers may replay an older owning emitter.
with tempfile.TemporaryDirectory(prefix='indc-preflight-jit-') as directory:
    td = Path(directory)
    for arch in os.environ.get('INDC_ARCHES', 'amd64 arm64').split():
        header = Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include HEADER


/* Preserve the real emitter and private capacity; vary only declared field offsets. */
int
emit(u32int *out, int cap, int wb, int layout)
{
    LeafLayout a = {0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.stringlen=layout == 0 ? 0 : layout == 1 ? 12 : 128;
    a.stringmax=4; a.stringdata=layout == 0 ? 24 : layout == 1 ? 32 : 160;
    return leafemit(out, cap, &a, IINDC);
}
'''.replace('HEADER', '"'+str(header)+'"'))
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc')) + shlex.split(os.environ.get('CFLAGS', '-O1 -g'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'),
                               str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x4000)
        forbidden = []
        writes = []


        # A declined operand-slot probe must not read either external source at all.
        def read(machine, access, address, size, value, context):
            if any(address < end and address+size > start for start, end in forbidden):
                raise RuntimeError((arch, 'forbidden operand/header read', hex(address), size))


        # Detect even stores that leave the same bytes on rejected paths.
        def write(machine, access, address, size, value, context):
            writes.append((address, size))


        machine.hook_add(UC_HOOK_MEM_READ, read)
        machine.hook_add(UC_HOOK_MEM_WRITE, write)
        checks = 0
        for wb in (4, 8):
            for layout in range(3):
                output = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                count = lib.emit(output, 128, wb, layout)
                if not 0 < count <= 128:
                    raise RuntimeError((arch, wb, layout, 'leaf unavailable'))
                for cap in range(count):
                    guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    if lib.emit(guard, cap, wb, layout) != 0 or list(guard) != [0xa5a5a5a5]*128:
                        raise RuntimeError('insufficient capacity published bytes')
                    checks += 1
                machine.mem_write(CODE, bytes(output)[:count*4])
                machine.ctl_remove_cache(CODE, CODE+512)
                length_offset = (0, 12, 128)[layout]
                data_offset = (24, 32, 160)[layout]
                probes = [('valid', 0, 0)]
                for field, width in enumerate((8, wb, wb)):
                    probes += [('slot', field, value) for value in (0, MASK, *[MASK-d for d in range(width)])]
                probes += [('owner', 0, MASK-d) for d in range(max(length_offset, 4)+4)]
                for wide in (False, True):
                    for kind, field, value in probes:
                        if kind not in os.environ.get('INDC_PROBES', 'valid slot owner').split():
                            continue
                        image = bytearray(b'\xa5'*0x4000)
                        put(image, 8, BASE+0x100); put(image, 24, BASE+0x200); put(image, 16, BASE+0x300)
                        put(image, 0x100, BASE+0x800); put(image, 0x200, 0, wb)
                        put(image, 0x800+length_offset, -1 if wide else 1, 4)
                        put(image, 0x804, 1, 4)
                        put(image, 0x800+data_offset, 0x20ac if wide else 65, 4 if wide else 1)
                        forbidden = []
                        if kind == 'slot':
                            put(image, (8, 24, 16)[field], value)
                            forbidden = [(BASE+0x100, BASE+0x108), (BASE+0x200, BASE+0x200+wb),
                                         (BASE+0x300, BASE+0x300+wb)]
                        if kind == 'owner':
                            put(image, 0x100, value)
                        expected = bytearray(image)
                        if kind == 'valid':
                            put(expected, 0x300, 0x20ac if wide else 65, wb)
                        machine.mem_write(BASE, bytes(image))
                        writes.clear()
                        if arch == 'amd64':
                            machine.reg_write(UC_X86_REG_RDI, BASE)
                            machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                            machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little'))
                        else:
                            machine.reg_write(UC_ARM64_REG_X0, BASE)
                            machine.reg_write(UC_ARM64_REG_LR, STOP)
                        machine.emu_start(CODE, STOP, count=1000)
                        result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                        if result != int(kind == 'valid') or bytes(machine.mem_read(BASE, 0x4000)) != expected:
                            raise RuntimeError((arch, wb, layout, kind, field, hex(value), 'image mismatch'))
                        if kind != 'valid' and writes:
                            raise RuntimeError('declined INDC wrote memory')
                        checks += 1
        print('PASS', arch, 'INDC preflight:', checks, 'forbidden-read/capacity/image checks', flush=True)
