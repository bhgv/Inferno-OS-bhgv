#!/usr/bin/env python3
"""Execute decimal range and trailing-NUL guards at native address boundaries."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
MASK = (1 << 64)-1
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
HIGH = (1 << 64)-0x1000


def put(image, offset, value, width=8):
    """Encode scalar fields independently of native instruction selection."""
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='decimal-spans-') as directory:
    td = Path(directory)
    for arch in os.environ.get('DECIMAL_ARCHES', 'arm64 amd64').split():
        header = Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null pointer used by the production emitter.
#include "isa.h"
#include HEADER


/* Stage each production decimal leaf with independently specified string metadata. */
int
emit(u32int *out, int wb, int kind, int layout)
{
    LeafLayout a = {0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.stringlen=layout ? 12 : 0; a.stringmax=4;
    a.stringdata=layout ? 32 : 24; a.heapref=24; a.refbytes=8;
    return leafemit(out,64,&a,kind == 0 ? ICVTCW : kind == 1 ? ICVTCL : ICVTCF);
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


        def written(machine, access, address, size, value, context):
            """Detect even same-value writes on declined paths."""
            writes.append((address, size))


        machine.hook_add(UC_HOOK_MEM_WRITE, written)
        checks = [0, 0, 0]
        for wb in (4, 8):
            for layout in (0, 1):
                lenoff, dataoff = (12, 32) if layout else (0, 24)
                offsets = (0xf00, 0xfd0, 0xfe0) if layout else (0xf00, 0xfe0, 0xfe8, 0xff0, 0xff8)
                for kind in map(int, os.environ.get('DECIMAL_KINDS', '0 1 2').split()):
                    output = (C.c_uint32*64)()
                    count = library.emit(output, wb, kind, layout)
                    if not 0 < count <= 64:
                        raise RuntimeError((arch, wb, kind, layout, 'leaf unavailable'))
                    machine.mem_write(CODE, bytes(output)[:count*4])
                    machine.ctl_remove_cache(CODE, CODE+256)
                    texts = (b'', b'+', b'-', b'0', b'-0',
                             b'2147483647', b'2147483648', b'-2147483648', b'-2147483649',
                             b'9223372036854775807', b'-9223372036854775808',
                             b'9223372036854775808', b'-9223372036854775809',
                             b'9999999999999999999', b'-9999999999999999999',
                             b'0000000000', b'0000000000000000000', b'00000000000000000000',
                             b'123456789012345', b'-123456789012345', b'1.5', b' 12', b'12x',
                             b'1\x002', b'\xff')
                    if os.environ.get("DECIMAL_SPAN_ONLY"):
                        texts = (b"", b"0", b"-1")
                    for offset in offsets:
                        for text in texts:
                            for unterminated in (0, 1):
                                for alias in (0, 1, 2):
                                    width = wb if kind == 0 else 8
                                    if alias == 2 and offset+8+width > 0x1000:
                                        continue
                                    image = bytearray(b'\xa5'*0x4000)
                                    top = bytearray(b'\x5a'*0x1000)
                                    owner = HIGH+offset
                                    put(top, offset+lenoff, len(text), 4)
                                    put(top, offset+4, len(text), 4)
                                    payload = text+(b'x' if unterminated else b'\x00')
                                    begin = offset+dataoff
                                    if begin < len(top):
                                        chunk = payload[:len(top)-begin]
                                        top[begin:begin+len(chunk)] = chunk
                                    put(image, 0x100, owner)
                                    destination = HIGH+offset+8 if alias == 2 else BASE+(0x100 if alias else 0x300)
                                    put(image, 8, BASE+0x100); put(image, 16, destination); put(image, 24, BASE+0x200)
                                    body = text[1:] if text[:1] in (b'+', b'-') else text
                                    digits = 15 if kind == 2 else 10 if kind == 0 and wb == 4 else 19
                                    valid = (not unterminated and len(body) <= digits and
                                             all(48 <= byte <= 57 for byte in body) and
                                             owner+dataoff+len(text)+1 <= MASK)
                                    if kind == 2 and text in (b'+', b'-'):
                                        valid = False
                                    scalar = int(text) if valid and body else 0
                                    if valid and kind != 2:
                                        valid = -(1 << (width*8-1)) <= scalar < (1 << (width*8-1))
                                    expected, expectedtop = bytearray(image), bytearray(top)
                                    if valid:
                                        if kind == 2:
                                            bits = struct.pack('<d', float(text) if text else 0.0)
                                        else:
                                            bits = (scalar & ((1 << (width*8))-1)).to_bytes(width, 'little')
                                        target = expectedtop if alias == 2 else expected
                                        index = offset+8 if alias == 2 else (0x100 if alias else 0x300)
                                        target[index:index+width] = bits
                                    machine.mem_write(BASE, bytes(image)); machine.mem_write(HIGH, bytes(top))
                                    if physical != HIGH:
                                        machine.mem_write(physical, bytes(top))
                                    writes.clear()
                                    if arch == 'amd64':
                                        machine.reg_write(UC_X86_REG_RDI, BASE)
                                        machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                                        machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little'))
                                        machine.reg_write(UC_X86_REG_MXCSR, 0x1f80 | ((alias+wb//4)&3)<<13 | (0x21 if unterminated else 0))
                                    else:
                                        machine.reg_write(UC_ARM64_REG_X0, BASE)
                                        machine.reg_write(UC_ARM64_REG_LR, STOP)
                                        machine.reg_write(UC_ARM64_REG_FPCR, ((alias+wb//4)&3)<<22)
                                        machine.reg_write(UC_ARM64_REG_FPSR, 0x11 if unterminated else 0)
                                    machine.emu_start(CODE, STOP, count=2000)
                                    result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                                    if result != valid or bytes(machine.mem_read(BASE, 0x4000)) != expected or bytes(machine.mem_read(physical, 0x1000)) != expectedtop:
                                        raise RuntimeError((arch, wb, layout, kind, offset, text, unterminated, alias, result, valid, 'decimal span mismatch'))
                                    if not valid and writes:
                                        raise RuntimeError('declined decimal operation wrote memory')
                                    if arch == 'amd64':
                                        if machine.reg_read(UC_X86_REG_MXCSR) != 0x1f80 | ((alias+wb//4)&3)<<13 | (0x21 if unterminated else 0):
                                            raise RuntimeError('decimal span guard changed MXCSR')
                                    elif (machine.reg_read(UC_ARM64_REG_FPCR) != ((alias+wb//4)&3)<<22 or
                                          machine.reg_read(UC_ARM64_REG_FPSR) != (0x11 if unterminated else 0)):
                                        raise RuntimeError('decimal span guard changed ARM64 FP state')
                                    checks[kind] += 1
        print('PASS', arch, 'CVTCW/CVTCL/CVTCF spans:', checks, 'address/image/write/FP checks', flush=True)
