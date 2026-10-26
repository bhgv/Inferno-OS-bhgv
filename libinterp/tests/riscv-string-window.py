#!/usr/bin/env python3
"""Check RV64 string leaves at the native-address boundary and ordinary slots."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV64, UC_HOOK_MEM_WRITE
from unicorn.riscv_const import UC_RISCV_REG_A0, UC_RISCV_REG_RA

ROOT = Path(__file__).resolve().parents[2]
MASK = (1 << 64) - 1
WINDOW = (1 << 64) - (1 << 34)
FIELDS = ('xlen wordbytes hasm pc s d m arraylen arraydata stringlen stringdata '
          'listdata listtail fp fixedtmp arraytype typesize hasdiv framelr framefp '
          'fixedcoef module linkmodule moduleprog modulenprog instbytes heaptype').split()


class Layout(C.Structure):
    _fields_ = [(name, C.c_int) for name in FIELDS]


# Fail explicitly so optimized Python retains every regression check.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# The destination can alias either input slot; capture both before publishing it.
def execute(blob, abi, owner, length, index, destination, headbytes=0):
    base, reg, data, string = 0x10000, 0x20000, 0x30000, 0x40000
    u = Uc(UC_ARCH_RISCV, UC_MODE_RISCV64)
    for page in (base, reg, data, string):
        u.mem_map(page, 0x1000)
    u.mem_write(base, blob)
    image = bytearray(b'\xa5' * 0x1000)
    struct.pack_into('<Q', image, 4, owner)
    struct.pack_into('<q' if abi.wordbytes == 8 else '<i', image, 36, index)
    u.mem_write(data, bytes(image))
    u.mem_write(reg, struct.pack('<QQQQ', 0x12345678, data+4, data+destination, data+36))
    if owner == string:
        if not headbytes:
            u.mem_write(string+abi.stringlen, struct.pack('<i', length))
        payload = bytes((j % 251)+1 for j in range(512))
        u.mem_write(string+(abi.listdata if headbytes else abi.stringdata), payload)
    before = bytes(u.mem_read(data, 0x1000))
    registers = bytes(u.mem_read(reg, 0x1000))
    header = bytes(u.mem_read(string, 0x1000))
    writes = []
    u.hook_add(UC_HOOK_MEM_WRITE, lambda uc, access, address, size, value, context:
               writes.append((address, size)))
    u.reg_write(UC_RISCV_REG_A0, reg)
    u.reg_write(UC_RISCV_REG_RA, base+len(blob))
    u.emu_start(base, base+len(blob), count=256)
    accepted = u.reg_read(UC_RISCV_REG_A0)
    require(bytes(u.mem_read(reg, 0x1000)) == registers, 'REG modified')
    require(bytes(u.mem_read(string, 0x1000)) == header, 'string modified')
    return accepted, before, bytes(u.mem_read(data, 0x1000)), writes


with tempfile.TemporaryDirectory(prefix='rv-string-window-') as temporary:
    td = Path(temporary)
    source = td/'emit.c'
    source.write_text('#include <stdint.h>\ntypedef uint32_t u32int;\n'
                      '#include "isa.h"\n#include "riscv-emit.h"\n'
                      '\n\n/* Expose production leaves to the host regression fixture. */\n'
                      'int emit(u32int *out, int cap, RvLayout *abi, int which)\n'
                      '{ int ops[] = { IINDC, ILENC, IHEADB, IHEADW, IHEADL, IHEADF };\n'
                      '  return rvemit(out, cap, abi, ops[which]); }\n')
    binary = td/'emit.so'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) +
                   shlex.split(os.environ.get('CFLAGS', '-O1 -g')) +
                   ['-shared', '-fPIC', '-I'+str(ROOT/'include'),
                    '-I'+os.environ.get('EMITTER_DIR', str(ROOT/'libinterp')), str(source), '-o', str(binary)], check=True)
    lib = C.CDLL(str(binary))
    lib.emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.POINTER(Layout), C.c_int]
    checks = 0
    for wb in (4, 8):
        for lenoff, dataoff in ((0, 24), (12, 32), (0, 2044)):
            abi = Layout(64, wb, 1, 0, 8, 16, 24)
            abi.stringlen, abi.stringdata = lenoff, dataoff
            for which in (0, 1):
                savedlen, saveddata = abi.stringlen, abi.stringdata
                for field in ('stringlen', 'stringdata') if not which else ('stringlen',):
                    for bad in range(-8, 2057):
                        setattr(abi, field, bad)
                        rejected = (C.c_uint32*66)(*[0xdeadbeef]*66)
                        validlayout = 0 <= bad <= 2047 and bad % 4 == 0
                        if not which:
                            validlayout = validlayout and (
                                saveddata >= bad+4 if field == 'stringlen' else bad >= savedlen+4)
                        emitted = lib.emit(rejected, 64, C.byref(abi), which)
                        require(bool(emitted) == validlayout, ('layout result', field, bad))
                        if not validlayout:
                            require(all(v == 0xdeadbeef for v in rejected), 'bad layout published bytes')
                        require(list(rejected)[64:] == [0xdeadbeef]*2, 'layout emitter overflow')
                        abi.stringlen, abi.stringdata = savedlen, saveddata
                        checks += 1
                if not which:
                    for overlap in (savedlen, savedlen-4):
                        abi.stringdata = overlap
                        rejected = (C.c_uint32*66)(*[0xdeadbeef]*66)
                        require(lib.emit(rejected, 64, C.byref(abi), which) == 0, 'overlap accepted')
                        require(all(v == 0xdeadbeef for v in rejected), 'overlap published bytes')
                        checks += 1
                    abi.stringdata = saveddata
                out = (C.c_uint32*66)(*[0xdeadbeef]*66)
                n = lib.emit(out, 64, C.byref(abi), which)
                require(0 < n <= 64, ('size', wb, which, n))
                require(list(out)[64:] == [0xdeadbeef]*2, 'emitter overflow')
                blob = bytes(out)[:n*4]
                for cap in range(n):
                    limited = (C.c_uint32*66)(*[0xdeadbeef]*66)
                    require(lib.emit(limited, cap, C.byref(abi), which) == 0, 'short cap accepted')
                    require(all(v == 0xdeadbeef for v in list(limited)[cap:]), 'short cap overflow')
                    checks += 1
                for owner in (0, MASK, WINDOW, WINDOW+4, MASK-31, 0x40000):
                    for length in (0, 1, 7, 64, -1, -7, -64, -(1 << 31)):
                        for index in (-1, 0, 1, 6, 63, 64, (1 << 31)-1):
                            for destination in (4, 36, 68):
                                accepted, before, after, writes = execute(
                                    blob, abi, owner, length, index, destination)
                                valid = (which and owner == MASK) or (
                                    owner == 0x40000 and length != -(1 << 31) and
                                    (which or 0 <= index < abs(length)))
                                require(accepted == int(bool(valid)),
                                        ('result', wb, which, owner, length, index))
                                expected = bytearray(before)
                                if valid:
                                    if which:
                                        value = abs(length) if owner != MASK else 0
                                    else:
                                        offset = index*(4 if length < 0 else 1)
                                        raw = bytes(((offset+j) % 251)+1 for j in range(4))
                                        value = struct.unpack('<i', raw)[0] if length < 0 else raw[0]
                                    struct.pack_into('<q' if wb == 8 else '<i', expected, destination, value)
                                require(after == bytes(expected), 'destination/image mismatch')
                                require(bool(writes) == bool(valid), 'fallback wrote memory')
                                checks += 1
    for wb in (4, 8):
        abi = Layout(64, wb, 1, 0, 8, 16, 24)
        for offset in (8, 24, 2040):
            abi.listdata = offset
            for which, width in ((2, 1), (3, wb), (4, 8), (5, 8)):
                out = (C.c_uint32*66)(*[0xdeadbeef]*66)
                n = lib.emit(out, 64, C.byref(abi), which)
                require(0 < n <= 64, ('head size', wb, which, n))
                blob = bytes(out)[:n*4]
                for owner in (0, MASK, MASK-1, MASK-7, MASK-offset-width+1, 0x40000):
                    for destination in (4, 36, 68):
                        accepted, before, after, writes = execute(
                            blob, abi, owner, 0, 0, destination, width)
                        valid = owner == 0x40000
                        require(accepted == int(valid), ('head result', wb, which, owner))
                        expected = bytearray(before)
                        if valid:
                            expected[destination:destination+width] = bytes(range(1, width+1))
                        require(after == bytes(expected), 'head image mismatch')
                        require(bool(writes) == valid, 'head fallback wrote memory')
                        checks += 1
    print('PASS RV64 string/list address spans:', checks, 'checks')
