#!/usr/bin/env python3
"""Execute eight production AMD64/ARM64 leaves with independent slot/read oracles."""
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RSP, UC_X86_REG_RAX, UC_X86_REG_RIP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_PC, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x100000000
CODE = 0x10000
STOP = CODE+0x1000
MASK = (1 << 64)-1
OPS = ('HEADB', 'HEADW', 'HEADL', 'HEADF', 'LENL', 'HEADM', 'MOVM', 'TCMP')


# Keep predicates active under optimized Python, reporting the precise case.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Write complete little-endian fields independently of host slot alignment.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (8*width))-1)).to_bytes(width, 'little')


with tempfile.TemporaryDirectory(prefix='list-slot-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('LIST_SLOT_ARCHES', 'amd64 arm64').split():
        source = td/(arch+'.c')
        header = Path(os.environ.get('LIST_SLOT_EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null address.
#include "isa.h"
#include '''+json.dumps(str(header))+'''


/* Use synthetic native-pointer fields independently of production host structs. */
int emit(u32int *out, int cap, int wb, int op)
{
    LeafLayout a={0};
    int ops[]={IHEADB,IHEADW,IHEADL,IHEADF,ILENL,IHEADM,IMOVM,ITCMP};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24;
    a.listdata=24; a.listtail=8; a.heaptype=16;
    return leafemit(out,cap,&a,ops[op]);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared', '-fPIC', '-I'+str(ROOT/'include'),
                       '-I'+str(ROOT/'libinterp'), str(source), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32), C.c_int, C.c_int, C.c_int]
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE, 0x3000)
        machine.mem_map(BASE, 0x4000)
        reads = set()
        writes = []
        label = None


        # Every data read must be explicitly justified by the independent case.
        def onread(machine, access, address, size, value, context):
            if BASE <= address and address+size <= BASE+32 or address == CODE+0x2800:
                return
            require((address, size) in reads, (label, 'unexpected read', hex(address), size))


        # Record stores as well as final bytes: same-value writes are observable.
        def onwrite(machine, access, address, size, value, context):
            writes.append((address, size))

        machine.hook_add(UC_HOOK_MEM_READ, onread)
        machine.hook_add(UC_HOOK_MEM_WRITE, onwrite)
        checks = 0
        for wb in (4, 8):
            for op, name in enumerate(OPS):
                if os.environ.get("LIST_SLOT_OP") and op != int(os.environ["LIST_SLOT_OP"]):
                    continue
                out = (C.c_uint32*128)()
                n = lib.emit(out, 128, wb, op)
                require(0 < n <= 128 and lib.emit(None, 128, wb, op) == n, (arch, name, 'sizing'))
                for cap in range(n+1):
                    probe = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    actual = lib.emit(probe, cap, wb, op)
                    require(actual == (n if cap == n else 0), (arch, name, 'capacity', cap))
                    require(bytes(probe)[(n*4 if cap == n else 0):] == bytes([0xa5])*(512-(n*4 if cap == n else 0)),
                            (arch, name, 'capacity canary', cap))
                machine.mem_write(CODE, bytes(out)[:n*4])
                machine.ctl_remove_cache(CODE, CODE+512)
                width = 1 if op == 0 else wb if op in (1, 4) else 8


                # Execute with a fresh memory image, preserving all FP control and sticky state.
                def execute(image, expected, result, detail, allowed):
                    global label, reads
                    label = (arch, wb, name, detail)
                    reads = allowed
                    machine.mem_write(BASE, bytes(image))
                    writes.clear()
                    if arch == 'amd64':
                        machine.reg_write(UC_X86_REG_RDI, BASE)
                        machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                        machine.mem_write(CODE+0x2800, STOP.to_bytes(8, 'little'))
                        machine.reg_write(UC_X86_REG_MXCSR, 0x5fa5)
                    else:
                        machine.reg_write(UC_ARM64_REG_X0, BASE)
                        machine.reg_write(UC_ARM64_REG_LR, STOP)
                        machine.reg_write(UC_ARM64_REG_FPCR, 0x800000)
                        machine.reg_write(UC_ARM64_REG_FPSR, 0x95)
                    machine.emu_start(CODE, STOP, count=10000)
                    require(machine.reg_read(UC_X86_REG_RIP if arch == 'amd64' else UC_ARM64_REG_PC) == STOP,
                            (label, 'instruction budget exhausted'))
                    actual = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                    require(actual == result, (label, 'return', actual, result))
                    require(bytes(machine.mem_read(BASE, len(image))) == bytes(expected), (label, 'memory'))
                    if not result or op == 7 or detail[0] == 'unused-copy':
                        require(not writes, (label, 'unexpected store', writes))
                    else:
                        destination = int.from_bytes(image[16:24], 'little')
                        span = width if op < 5 else detail[1]
                        require(all(destination <= address and address+size <= destination+span
                                    for address, size in writes), (label, 'store outside result', writes))
                    if arch == 'amd64':
                        require(machine.reg_read(UC_X86_REG_MXCSR) == 0x5fa5, (label, 'MXCSR'))
                    else:
                        require(machine.reg_read(UC_ARM64_REG_FPCR) == 0x800000 and
                                machine.reg_read(UC_ARM64_REG_FPSR) == 0x95, (label, 'FP state'))


                # Fresh slots remain disjoint from list payload and Type header storage.
                def setup(shift=0):
                    image = bytearray([0xa5]*0x4000)
                    put(image, 0, 0x12345678)
                    put(image, 8, BASE+0x100+shift)
                    put(image, 16, BASE+0x200+shift)
                    put(image, 24, BASE+0x180+shift)
                    put(image, 0x100+shift, BASE+0x800)
                    put(image, 0x180+shift, 0, wb)
                    return image

                for side in range(3):
                    if side == 0 and op == 6 or side == 1 and op in (5, 6) or side == 2 and op not in (5, 6):
                        continue
                    span = wb if side == 2 else 8 if side == 0 or op == 7 else width
                    for address in [0]+[MASK-i for i in range(span)]:
                        for shift in range(8):
                            image = setup(shift)
                            put(image, (8, 16, 24)[side], address)
                            allowed = {(BASE+0x100+shift, 8)} if op == 5 and side == 2 else set()
                            execute(image, image, 0, ('slot', side, address, shift), allowed)
                            checks += 1
                if op < 5:
                    for address in (0, MASK):
                        image = setup()
                        put(image, 8, 1)
                        put(image, 16, address)
                        execute(image, image, 0, ('result-priority', address), set())
                        checks += 1
                if op in (5, 6):
                    for count in (0, -1):
                        for address in (0, 1, MASK):
                            for shift in range(8):
                                image = setup(shift)
                                put(image, 16, address)
                                put(image, 0x180+shift, count, wb)
                                allowed = {(BASE+0x180+shift, wb)}
                                if op == 5:
                                    put(image, 0x100+shift, 1)
                                    allowed.add((BASE+0x100+shift, 8))
                                else:
                                    put(image, 8, address)
                                execute(image, image, int(count == 0), ('unused-copy', count, address, shift), allowed)
                                checks += 1
                if op < 4:
                    for owner in [0]+[MASK-i for i in range(24+width)]:
                        image = setup()
                        put(image, 0x100, owner)
                        execute(image, image, 0, ('payload-span', owner), {(BASE+0x100, 8)})
                        checks += 1
                    for sa in range(8):
                        for da in range(8):
                            for value in (0, MASK, 0x8000000080000080, 0x0123456789abcdef):
                                image = setup(sa)
                                put(image, 16, BASE+0x200+da)
                                put(image, 0x800+24, value, width)
                                expected = image.copy()
                                expected[0x200+da:0x200+da+width] = image[0x818:0x818+width]
                                execute(image, expected, 1, ('value', sa, da, value), {(BASE+0x100+sa, 8), (BASE+0x818, width)})
                                checks += 1
                elif op == 4:
                    for length in range(18):
                        for shift in range(8):
                            image = setup(shift)
                            put(image, 0x100+shift, BASE+0x800 if length else MASK)
                            allowed = {(BASE+0x100+shift, 8)}
                            for i in range(length):
                                address = BASE+0x800+32*i+8
                                put(image, address-BASE, BASE+0x800+32*(i+1) if i+1 < length else MASK)
                                allowed.add((address, 8))
                            expected = image.copy()
                            put(expected, 0x200+shift, length, wb)
                            execute(image, expected, 1, ('list-length', length, shift), allowed)
                            checks += 1
                elif op in (5, 6):
                    for count in (1, 7, 8, 9, 31, 255, 256, 257):
                        for delta in (-17, -1, 0, 1, 17):
                            for shift in range(8):
                                image = setup(shift)
                                start = 0x818 if op == 5 else 0x800+shift
                                put(image, 16, BASE+start+delta)
                                put(image, 0x180+shift, count, wb)
                                allowed = {(BASE+0x180+shift, wb)}
                                if op == 5:
                                    allowed.add((BASE+0x100+shift, 8))
                                else:
                                    put(image, 8, BASE+start)
                                if count <= 256:
                                    allowed.update((BASE+start+i, 1) for i in range(count))
                                    # Production copy kernels also read full qword blocks.
                                    # Permit only loads entirely within this source span;
                                    # short tails and fallback cases remain byte-bounded.
                                    allowed.update((BASE+start+i, 8) for i in range(max(0, count-7)))
                                # Distinct bytes make both overlap directions observable.
                                for i in range(512):
                                    image[0x700+i] = i*37 & 255
                                expected = image.copy()
                                if count <= 256:
                                    expected[start+delta:start+delta+count] = image[start:start+count]
                                execute(image, expected, int(count <= 256), ('copy-overlap', count, delta, shift), allowed)
                                checks += 1
                else:
                    for source_owner in (0, 1, 16, MASK, BASE+0x800):
                        for dest_owner in (0, 1, 16, MASK, BASE+0x900):
                            for same in (False, True):
                                image = setup()
                                put(image, 0x100, source_owner)
                                put(image, 0x200, dest_owner)
                                put(image, 0x800-16, 0x1234)
                                put(image, 0x900-16, 0x1234 if same else 0x5678)
                                allowed = {(BASE+0x100, 8), (BASE+0x200, 8)}
                                valid = source_owner == MASK
                                if source_owner == BASE+0x800 and dest_owner == BASE+0x900:
                                    allowed.update(((BASE+0x800-16, 8), (BASE+0x900-16, 8)))
                                    valid = same
                                execute(image, image, int(valid), ('types', source_owner, dest_owner, same), allowed)
                                checks += 1
        print(f'PASS {arch} list/copy/type preflight and differential: {checks} cases', flush=True)
