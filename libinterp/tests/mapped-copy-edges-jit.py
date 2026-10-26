#!/usr/bin/env python3
"""Check mapped qword/byte copy access sequences at unmapped page edges on AMD64/ARM64."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR

ROOT = Path(__file__).resolve().parents[2]  # Production repository root.
CODE = 0x10000  # Code and return-stack mapping.
STOP = CODE+0x1000  # Return sentinel outside every emitted leaf.
BASE = 0x100000000  # Borrowed REG and decoded scalar slots.
PAGES = (BASE+0x10000, BASE+0x14000)  # Each header/payload region has unmapped exterior neighbors.
PATTERN = bytes((i*37+11)&255 for i in range(4096))  # Whole-page source/canary image.


# Require a once-evaluated invariant, including with Python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Encode a little-endian unsigned value in an owned register image.
def put(image, offset, value, size=8):
    image[offset:offset+size] = (value & ((1<<(size*8))-1)).to_bytes(size,'little')


# Describe independent memmove access order: full blocks precede exact byte tails.
def accesses(source, destination, count):
    records = []
    if destination > source:
        left = count
        while left >= 8:
            left -= 8
            records.append((left,8))
        records += [(i,1) for i in range(left-1,-1,-1)]
    else:
        whole = count//8*8
        records = [(i,8) for i in range(0,whole,8)]
        records += [(i,1) for i in range(whole,count)]
    return [(source+i,n) for i,n in records], [(destination+i,n) for i,n in records]


with tempfile.TemporaryDirectory(prefix='mapped-copy-edges-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('MAPPED_COPY_EDGE_ARCHES','amd64 arm64').split():
        header = Path(os.environ.get('MAPPED_COPY_EDGE_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output requests sizing only.
#include "isa.h"
#include "'''+str(header)+'''"


/* Publish a production copier with an independent native-pointer register layout. */
int emit(u32int *out,int cap,int wb,int head)
{
    LeafLayout a={0};
    a.wordbytes=wb;
    a.pc=0;
    a.s=8;
    a.d=16;
    a.m=24;
    a.listdata=24;
    a.module=40;
    a.linkmodule=8;
    a.moduletypes=32;
    a.modulenty=40;
    a.typesize=24;
    a.typenp=28;
    a.heapref=24;
    a.refbytes=8;
    return leafemit(out,cap,&a,head ? IHEADMP : IMOVMP);
}
''')
        binary = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib = C.CDLL(str(binary))
        lib.emit.argtypes = [C.POINTER(C.c_uint32),C.c_int,C.c_int,C.c_int]
        machine = Uc(UC_ARCH_X86 if arch=='amd64' else UC_ARCH_ARM64,UC_MODE_64 if arch=='amd64' else 0)
        machine.mem_map(CODE,0x3000)
        machine.mem_map(BASE,0x1000)
        for page in PAGES:
            machine.mem_map(page-4096,0x2000)
        reads, writes = [], []


        # Record payload and list-reference reads; module metadata and return stack are separate.
        def read_hook(machine, access, address, size, value, context):
            if any(page-4096 <= address < page+4096 for page in PAGES):
                reads.append((address,size))


        # Record all data writes so a wrong width or neighbor overwrite cannot hide.
        def write_hook(machine, access, address, size, value, context):
            writes.append((address,size))


        machine.hook_add(UC_HOOK_MEM_READ,read_hook)
        machine.hook_add(UC_HOOK_MEM_WRITE,write_hook)
        checks = 0
        for wb in (4,8):
            for head in (0,1):
                out = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                n = lib.emit(out,128,wb,head)
                require(0 < n <= 128 and lib.emit(None,128,wb,head)==n and out[n]==0xa5a5a5a5,(arch,wb,head,'capacity'))
                exact = (C.c_uint32*(n+1))(*([0xa5a5a5a5]*(n+1)))
                require(lib.emit(exact,n,wb,head)==n and exact[n]==0xa5a5a5a5 and bytes(exact)[:n*4]==bytes(out)[:n*4],'exact image/canary')
                for cap in range(n):
                    short = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                    require(lib.emit(short,cap,wb,head)==0 and lib.emit(None,cap,wb,head)==0 and list(short)==[0xa5a5a5a5]*129,'short publication')
                machine.mem_write(CODE,bytes(out)[:n*4])
                machine.ctl_remove_cache(CODE,CODE+512)
                for count in (-1,)+tuple(range(258)):
                    for direction in (0,1):
                        sp,dp = PAGES[direction],PAGES[1-direction]
                        for source_end in (0,1):
                            for dest_end in (0,1):
                                src = sp+4096-count if source_end else sp
                                dst = dp+4096-count if dest_end else dp
                                accepted = 0 <= count <= 256
                                if not 0 < count <= 256:
                                    src,dst = 1,1  # Empty/rejected payloads cannot be accessed.
                                registers = bytearray(b'\xa5'*4096)
                                put(registers,0,BASE+0x800)
                                put(registers,8,BASE+0x100 if head else src)
                                put(registers,16,dst)
                                put(registers,24,BASE+0x200)
                                put(registers,0x100,src-24 if head and 0 < count <= 256 else BASE+0x500)
                                put(registers,0x200,1,wb)
                                put(registers,40,BASE+0x500)
                                put(registers,0x508,BASE+0x700)
                                put(registers,0x720,BASE+0x900)
                                put(registers,0x728,2,4)
                                put(registers,0x908,BASE+0xb00)
                                put(registers,0xb18,count,4)
                                put(registers,0xb1c,0,4)
                                expectedregisters = bytearray(registers)
                                if head and 0 < count <= 256:
                                    put(expectedregisters,8,src)
                                expected = {page:bytearray(PATTERN*2) for page in PAGES}
                                if 0 < count <= 256:
                                    expected[dp][4096+dst-dp:4096+dst-dp+count] = PATTERN[src-sp:src-sp+count]
                                    if head:
                                        put(expected[sp],4096+src-sp-48,10)
                                machine.mem_write(BASE,bytes(registers))
                                for page in PAGES:
                                    machine.mem_write(page-4096,PATTERN*2)
                                if head and 0 < count <= 256:
                                    machine.mem_write(src-48,(10).to_bytes(8,'little'))
                                if arch=='amd64':
                                    machine.reg_write(UC_X86_REG_RDI,BASE)
                                    machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                    machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'))
                                else:
                                    machine.reg_write(UC_ARM64_REG_X0,BASE)
                                    machine.reg_write(UC_ARM64_REG_LR,STOP)
                                reads.clear()
                                writes.clear()
                                machine.emu_start(CODE,STOP,count=5000)
                                returned = machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                                detail = (arch,wb,head,count,direction,source_end,dest_end)
                                wanted_reads,wanted_writes = accesses(src,dst,count) if 0 < count <= 256 else ([],[])
                                if head and 0 < count <= 256:
                                    wanted_reads = [(src-48,8)]+wanted_reads
                                    wanted_writes = [(BASE+8,8)]+wanted_writes
                                require(returned==int(accepted) and reads==wanted_reads and writes==wanted_writes,detail)
                                require(bytes(machine.mem_read(BASE,4096))==expectedregisters,(detail,'REG/slots changed'))
                                for page in PAGES:
                                    require(bytes(machine.mem_read(page-4096,8192))==expected[page],(detail,'page image'))
                                checks += 1
        print('PASS',arch,'mapped qword/byte page-edge reads/writes/images:',checks,'cases',flush=True)
