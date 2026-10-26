#!/usr/bin/env python3
"""Execute guarded CVTAC leaves with complete image, ownership and ASCII oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT = Path(__file__).resolve().parents[2]
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
MASK = (1 << 64)-1
ARRAY, STRING = 0x1234567812340000, 0x8765432145670000


# Encode cells independently of every machine instruction selected by the leaf.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')


# Keep every check active with optimized Python.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='byte-string-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('BYTE_STRING_ARCHES', 'arm64 amd64').split():
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null sentinel used by production emission helpers.
#include "isa.h"
#include HEADER


/* Supply independent owner/type identities and private offsets to the real emitter. */
int
emit(u32int *out, int cap, int wb)
{
    LeafLayout a = {0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.arraylen=0; a.arraytype=8; a.arrayroot=16; a.arraydata=24;
    a.typesize=24; a.typenp=28;
    a.stringlen=0; a.stringmax=4; a.stringdata=24;
    a.heapref=24; a.heaptype=16; a.heapcolor=32; a.refbytes=8;
    a.arrayidentity=(void*)0x1234567812340000ULL;
    a.stringtype=(void*)0x8765432145670000ULL;
    return leafemit(out,cap,&a,ICVTAC);
}
'''.replace('HEADER', '"'+str(Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h'))+'"'))
        binary = td/(arch+'.so')
        subprocess.run(shlex.split(os.environ.get('CC', 'cc'))+
                       shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+
                       ['-shared', '-fPIC', '-I'+str(ROOT/'include'), '-I'+str(ROOT/'libinterp'),
                        str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.c_int]
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000); machine.mem_map(BASE, 0x8000)
        writes = []
        machine.hook_add(UC_HOOK_MEM_WRITE, lambda uc, access, address, size, value, context:
                         writes.append((address, size)))
        checks = 0
        for wb in (4, 8):
            out = (C.c_uint32*130)(*[0xdeadbeef]*130)
            count = lib.emit(out, 128, wb)
            require(0 < count <= 128, (arch, wb, 'missing leaf', count))
            require(list(out)[128:] == [0xdeadbeef]*2, 'capacity canary')
            require(lib.emit(None, 128, wb) == count, 'sizing differs')
            for cap in range(count):
                short = (C.c_uint32*130)(*[0xdeadbeef]*130)
                require(lib.emit(short, cap, wb) == 0 and all(v == 0xdeadbeef for v in short), 'partial publication')
                checks += 1
            machine.mem_write(CODE, bytes(out)[:count*4]); machine.ctl_remove_cache(CODE, CODE+512)
            for length in (0, 1, 2, 7, 31, 128, 255, 256, 257):
                for profile in range(36):
                    for capacity in (0, 7, 256, 512):
                        for align in range(8):
                            image = bytearray(b'\xa5'*0x8000)
                            src, dst, array, old, typ, data = 0x100+align, 0x200+align, 0x1000, 0x2000, 0x3000, 0x4000
                            owner, result = BASE+array, BASE+old
                            aref, sref, atype, stype, root = 1, 1, ARRAY, STRING, MASK
                            element, size, pointers = BASE+typ, 1, 0
                            n, oldlen, cap = length, 0, capacity
                            badbyte = None
                            if profile == 1: owner = 0
                            elif profile == 2: owner = MASK
                            elif profile == 3: owner = 8
                            elif profile == 4: owner = MASK-1
                            elif profile == 5: atype ^= 1 << 40
                            elif profile == 6: aref = 0
                            elif profile == 7: root = 0
                            elif profile == 8: root = BASE+0x5000
                            elif profile == 9: element = 0
                            elif profile == 10: element = MASK
                            elif profile == 11: element = MASK-1
                            elif profile == 12: size = 2
                            elif profile == 13: pointers = 1
                            elif profile == 14: n = -1
                            elif profile == 15: n = (1 << (wb*8-1))-1
                            elif profile == 16: data = -BASE
                            elif profile == 17: data = MASK-BASE
                            elif profile == 18: data = MASK-BASE-1; n = 256
                            elif profile == 19: result = 0
                            elif profile == 20: result = MASK
                            elif profile == 21: result = MASK-1
                            elif profile == 22: stype ^= 1 << 40
                            elif profile == 23: sref = 0
                            elif profile == 24: sref = 2
                            elif profile == 25: sref = MASK
                            elif profile == 26: oldlen = -7
                            elif profile == 27: oldlen = -(1 << 31)
                            elif profile == 28: oldlen = cap+1
                            elif profile == 29: cap = -1
                            elif profile == 30: badbyte = 0
                            elif profile == 31: badbyte = max(0, length//2)
                            elif profile == 32: badbyte = max(0, length-1)
                            elif profile == 33: data = old+24-3
                            elif profile == 34: data = old+24+3
                            elif profile == 35: data = old+24
                            put(image, array-24, aref); put(image, array-16, atype)
                            put(image, array, n, wb); put(image, array+8, element)
                            put(image, array+16, root); put(image, array+24, BASE+data)
                            put(image, old-24, sref); put(image, old-16, stype)
                            put(image, old, oldlen, 4); put(image, old+4, cap, 4)
                            put(image, typ+24, size, 4); put(image, typ+28, pointers, 4)
                            if 0 <= data < 0x7000:
                                for i in range(max(1, length)):
                                    image[data+i] = (i*17+align) % 128
                                if badbyte is not None:
                                    image[data+badbyte] = 0x80
                            put(image, src, owner); put(image, dst, result)
                            put(image, 8, BASE+src); put(image, 16, BASE+dst)
                            accepted = (owner == BASE+array and atype == ARRAY and aref != 0 and root == MASK and
                                        element == BASE+typ and size == 1 and pointers == 0 and 1 <= n <= 256 and
                                        BASE+data not in (0, MASK) and 0 <= BASE+data <= MASK-n and
                                        result == BASE+old and stype == STRING and sref == 1 and
                                        0 <= oldlen <= cap and cap >= n and badbyte is None)
                            # Overlapping source bytes may include or overwrite old metadata in this fixture.
                            actualold = int.from_bytes(image[old:old+4], 'little', signed=True)
                            actualcap = int.from_bytes(image[old+4:old+8], 'little', signed=True)
                            accepted = accepted and 0 <= actualold <= actualcap and actualcap >= n
                            payload = bytes(image[data:data+n]) if accepted else b''
                            accepted = accepted and all(v < 128 for v in payload)
                            expected = bytearray(image)
                            if accepted:
                                expected[old+24:old+24+n] = payload
                                put(expected, old, n, 4); put(expected, dst, result)
                            if arch == 'amd64':
                                put(image, 0x7000, STOP); put(expected, 0x7000, STOP)
                                machine.reg_write(xr.UC_X86_REG_RDI, BASE)
                                machine.reg_write(xr.UC_X86_REG_RSP, BASE+0x7000)
                                machine.reg_write(xr.UC_X86_REG_MXCSR, 0x3f95)
                                saved = [getattr(xr, 'UC_X86_REG_'+name) for name in ('RBX','RBP','R12','R13','R14','R15')]
                            else:
                                machine.reg_write(ar.UC_ARM64_REG_X0, BASE); machine.reg_write(ar.UC_ARM64_REG_LR, STOP)
                                machine.reg_write(ar.UC_ARM64_REG_FPCR, 0x400000); machine.reg_write(ar.UC_ARM64_REG_FPSR, 0x15)
                                saved = [getattr(ar, 'UC_ARM64_REG_X'+str(i)) for i in range(19, 30)]
                            for reg in saved: machine.reg_write(reg, 0xabcdef000+reg)
                            machine.mem_write(BASE, bytes(image)); writes.clear()
                            try:
                                machine.emu_start(CODE, STOP, count=10000)
                            except Exception as error:
                                raise RuntimeError((arch, wb, length, profile, capacity, align, 'execution')) from error
                            status = machine.reg_read(xr.UC_X86_REG_RAX if arch == 'amd64' else ar.UC_ARM64_REG_X0)
                            detail = (arch, wb, length, profile, capacity, align)
                            require(status == int(accepted), ('accept', detail, status, accepted))
                            require(bytes(machine.mem_read(BASE, len(image))) == bytes(expected), ('image', detail))
                            require(bool(writes) == bool(accepted), ('writes', detail))
                            for reg in saved: require(machine.reg_read(reg) == 0xabcdef000+reg, ('callee saved', detail, reg))
                            if arch == 'amd64': require(machine.reg_read(xr.UC_X86_REG_MXCSR) == 0x3f95, 'MXCSR')
                            else: require(machine.reg_read(ar.UC_ARM64_REG_FPCR) == 0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR) == 0x15, 'FP state')
                            checks += 1
            print('PASS', arch, 'CVTAC ASCII reuse:', checks, 'checks;', count, 'code units WORD', wb*8)
