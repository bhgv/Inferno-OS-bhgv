#!/usr/bin/env python3
"""Execute all string predicates around the native address-window boundary."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
MASK = (1 << 64)-1
LIMIT = (1 << 64)-(1 << 34)
EDGE, TOP = LIMIT-0x2000, (1 << 64)-0x1000
OLDPC, TARGET = BASE+0x5400, BASE+0x5500


def put(images, address, value, width=8):
    """Write mapped fixture bytes while leaving wrapped payload addresses unmapped."""
    raw = (value & ((1 << (width*8))-1)).to_bytes(width, 'little')
    for start, image in images.items():
        if start <= address < start+len(image):
            amount = min(width, start+len(image)-address)
            image[address-start:address-start+amount] = raw[:amount]
            return


def key(value, wide):
    """Model the C comparator's signed Rune keys and unsigned ASCII bytes."""
    value &= 0xffffffff if wide else 255
    return value-(1 << 32) if wide and value >= (1 << 31) else value


with tempfile.TemporaryDirectory(prefix='string-compare-jit-') as directory:
    td = Path(directory)
    for arch in os.environ.get('COMPARE_ARCHES', 'arm64 amd64').split():
        header = Path(os.environ.get('EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null used by the staged emitter.
#include "isa.h"
#include HEADER


/* Stage each predicate using independent String and REG layouts. */
int
emit(u32int *out, int wb, int op, int layout)
{
    LeafLayout a = {0};
    int codes[] = {IBEQC,IBNEC,IBLTC,IBLEC,IBGTC,IBGEC};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24;
    a.stringlen=layout ? 12 : 0; a.stringmax=4; a.stringdata=layout ? 32 : 24;
    return leafemit(out,64,&a,codes[op]);
}
'''.replace('HEADER', '"'+str(header)+'"'))
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),
                               '-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        library = C.CDLL(str(binary))
        library.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine = Uc(UC_ARCH_X86, UC_MODE_64) if arch == 'amd64' else Uc(UC_ARCH_ARM64, 0)
        machine.mem_map(CODE,0x3000)
        sizes = {BASE:0x6000, EDGE:0x5000, TOP:0x1000}
        physical = {}
        for start,size in sizes.items():
            machine.mem_map(start,size)
            mirror = start & ((1 << 52)-1) if arch == 'amd64' else start
            physical[start] = mirror
            if mirror != start:
                machine.mem_map(mirror,size)
        writes = []


        def written(machine, access, address, size, value, context):
            """Record even same-value PC stores and all declined-path writes."""
            writes.append((address,size))


        machine.hook_add(UC_HOOK_MEM_WRITE,written)
        checks = 0
        locations = (None,LIMIT-0x100,LIMIT-8,LIMIT-1,LIMIT,LIMIT+8,TOP+0xfe0,TOP+0xff8,MASK-1)
        profiles = range(23)
        if os.environ.get('COMPARE_WRAP_ONLY'):
            locations, profiles = (TOP+0xfe0,), (7,)
        for wb in (4,8):
            for layout in (0,1):
                lenoff,dataoff = (12,32) if layout else (0,24)
                for op in range(6):
                    output = (C.c_uint32*64)()
                    count = library.emit(output,wb,op,layout)
                    if not 0 < count <= 64:
                        raise RuntimeError((arch,wb,layout,op,'comparison unavailable'))
                    machine.mem_write(CODE,bytes(output)[:count*4])
                    machine.ctl_remove_cache(CODE,CODE+256)
                    for side in (0,1):
                        for location in locations:
                            for aw in (0,1):
                                for bw in (0,1):
                                    for profile in profiles:
                                        images = {start:bytearray(b'\xa5'*size) for start,size in sizes.items()}
                                        owners = [BASE+0x1000,BASE+0x3000]
                                        if location is not None:
                                            owners[side] = location
                                        lengths, maximum = [3,3], [512,512]
                                        values = [[0x41,0,255]+[0x41]*254,[0x41,0,255]+[0x41]*254]
                                        widths = [aw,bw]
                                        if profile == 1: values[0][0],values[1][0] = 1,2
                                        if profile == 2:
                                            lengths = [7,7]; values[0][6],values[1][6] = 0x20ac,0x20ad
                                        if profile == 3: lengths = [1,7]
                                        if profile == 4: lengths = [7,1]
                                        if profile == 5: lengths[0] = 0
                                        if profile == 6: lengths[1] = 0
                                        if profile == 7:
                                            lengths = [257,257]; values[0][256],values[1][256] = 1,2
                                        if profile == 8: values[0][0],values[1][0] = 0x80000000,127
                                        if profile == 9: values[0][0],values[1][0] = 0xffffffff,0x7fffffff
                                        if profile == 10: owners[0] = MASK
                                        if profile == 11: owners[1] = MASK
                                        if profile == 12: owners = [MASK,MASK]
                                        if profile == 13: owners[0] = 0
                                        if profile == 14: owners[1] = 0
                                        if profile == 15:
                                            owners[1] = owners[0]; widths[1] = widths[0]; values[1] = values[0]
                                        if profile == 16: maximum[0] = 2
                                        if profile == 17: maximum[1] = -1
                                        if profile == 18: lengths[0] = -0x80000000
                                        if profile == 19: lengths[1] = -0x80000000
                                        if profile == 20:
                                            lengths[0] = maximum[0] = 0x7fffffff; lengths[1] = 1
                                            values[0][0],values[1][0] = 1,2
                                        if profile == 21:
                                            lengths[1] = maximum[1] = 0x7fffffff; lengths[0] = 1
                                            values[0][0],values[1][0] = 1,2
                                        if profile == 22:
                                            lengths = maximum = [0x7fffffff,0x7fffffff]
                                            owners[1] = owners[0]; widths[1] = widths[0]; values[1] = values[0]
                                        for index,owner in enumerate(owners):
                                            if owner in (0,MASK):
                                                continue
                                            length = lengths[index]
                                            signed = -length if widths[index] and length > 0 else length
                                            put(images,owner+lenoff,signed,4); put(images,owner+4,maximum[index],4)
                                            stride = 4 if widths[index] else 1
                                            for at,value in enumerate(values[index][:min(max(length,0),257)]):
                                                put(images,owner+dataoff+at*stride,value,stride)
                                        valid = all(owner != 0 and (owner == MASK or
                                                    owner < LIMIT and 0 <= lengths[index] <= maximum[index])
                                                    for index,owner in enumerate(owners))
                                        order = 0
                                        if valid and owners[0] != owners[1]:
                                            actual = [0 if owners[i] == MASK else lengths[i] for i in (0,1)]
                                            for index in range(min(actual)):
                                                a,b = key(values[0][index],widths[0]),key(values[1][index],widths[1])
                                                if a != b:
                                                    order = (a>b)-(a<b)
                                                    break
                                            if order == 0:
                                                order = (actual[0]>actual[1])-(actual[0]<actual[1])
                                        take = valid and (order == 0,order != 0,order < 0,order <= 0,order > 0,order >= 0)[op]
                                        target = OLDPC if profile%3 == 0 else TARGET
                                        put(images,BASE,OLDPC); put(images,BASE+0x100,owners[0]); put(images,BASE+0x200,owners[1])
                                        put(images,BASE+0x300,target)
                                        put(images,BASE+8,BASE+0x100); put(images,BASE+24,BASE+0x200)
                                        put(images,BASE+16,BASE+0x300 if take else 0x700000000)
                                        expected = {start:bytearray(image) for start,image in images.items()}
                                        if take:
                                            put(expected,BASE,target)
                                        for start,image in images.items():
                                            machine.mem_write(start,bytes(image))
                                            if physical[start] != start:
                                                machine.mem_write(physical[start],bytes(image))
                                        writes.clear()
                                        if arch == 'amd64':
                                            machine.reg_write(UC_X86_REG_RDI,BASE); machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                            machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'))
                                            machine.reg_write(UC_X86_REG_MXCSR,0x1f80 | (op&3)<<13 | (0x21 if profile&1 else 0))
                                        else:
                                            machine.reg_write(UC_ARM64_REG_X0,BASE); machine.reg_write(UC_ARM64_REG_LR,STOP)
                                            machine.reg_write(UC_ARM64_REG_FPCR,(op&3)<<22)
                                            machine.reg_write(UC_ARM64_REG_FPSR,0x11 if profile&1 else 0)
                                        machine.emu_start(CODE,STOP,count=12000)
                                        result = machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                                        if result != valid or writes != ([(BASE,8)] if take else []):
                                            raise RuntimeError((arch,wb,layout,op,side,location,aw,bw,profile,result,valid,writes,'predicate/write mismatch'))
                                        for start,image in expected.items():
                                            if bytes(machine.mem_read(physical[start],len(image))) != image:
                                                raise RuntimeError((arch,wb,layout,op,side,location,aw,bw,profile,'image mismatch'))
                                        if arch == 'amd64':
                                            if machine.reg_read(UC_X86_REG_MXCSR) != 0x1f80 | (op&3)<<13 | (0x21 if profile&1 else 0):
                                                raise RuntimeError('comparison changed MXCSR')
                                        elif (machine.reg_read(UC_ARM64_REG_FPCR) != (op&3)<<22 or
                                              machine.reg_read(UC_ARM64_REG_FPSR) != (0x11 if profile&1 else 0)):
                                            raise RuntimeError('comparison changed ARM64 FP state')
                                        checks += 1
        print('PASS',arch,'string comparisons:',checks,'window/predicate/image/write/FP cases',flush=True)
