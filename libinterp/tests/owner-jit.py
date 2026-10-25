#!/usr/bin/env python3
"""Execute guarded owner leaves on AMD64 and ARM64 with independent memory images."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT=Path(__file__).resolve().parents[2]
BASE=0x100000000
CODE=0x10000
STOP=0x11000
MASK=(1<<64)-1


def put(image, offset, value, size=8):
    """Store a complete little-endian field without depending on host alignment."""
    image[offset:offset+size]=int(value & ((1<<(8*size))-1)).to_bytes(size,'little')


with tempfile.TemporaryDirectory(prefix='owner-jit-') as directory:
    td=Path(directory)
    checks=0
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "'''+arch+'''-emit.h"
/* Supply an independent native-pointer layout and malformed-layout probes. */
int emit(u32int *out,int cap,int wb,int op,int bad){
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;a.module=40;
 a.heapref=24;a.heapcolor=32;a.refbytes=8;a.mutator=(void*)0x100002000ULL;
 a.listdata=24;a.listtail=8;a.nprop=(void*)0x100002008ULL;a.markcolor=3;
 switch(bad){
 case 1:a.heapref=0;break;case 2:a.heapref=256;break;case 3:a.heapref=7;break;
 case 4:a.heapcolor=0;break;case 5:a.heapcolor=256;break;case 6:a.heapcolor=2;break;
 case 7:a.refbytes=4;break;case 8:a.mutator=nil;break;
 case 9:a.module=-1;break;case 10:a.module=2048;break;case 11:a.module=1;break;
 case 12:a.nprop=nil;break;case 13:a.markcolor=-1;break;case 14:a.markcolor=256;break;
 case 15:a.listdata=-1;a.listtail=-1;break;
 }
 return leafemit(out,cap,&a,((int[]){IMOVP,ISELF,IHEADP,ITAIL})[op]);
}
''')
        binary=td/(arch+'.so')
        cmd=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary))
        lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*4
        dis=Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[]
        def record_write(machine, access, address, size, value, context):
            """Record actual guest stores so a declined leaf cannot hide same-byte writes."""
            writes.append((address,size))
        machine.hook_add(UC_HOOK_MEM_WRITE,record_write)
        for wb in (4,8):
            for op in range(4):
                out=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                n=lib.emit(out,64,wb,op,0)
                assert 0<n<=64 and lib.emit(None,64,wb,op,0)==n
                blob=bytes(out)[:n*4]
                assert sum(i.size for i in dis.disasm(blob,CODE))==n*4
                for bad in range(1,16 if op>=2 else 15):
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,wb,op,bad)==0 and list(guard)==[0xa5a5a5a5]*64
                    checks+=1
                for cap in (0,n-1):
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,cap,wb,op,0)==0 and list(guard)==[0xa5a5a5a5]*64
                machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+256)
                if op>=2:
                    for badowner in (0,MASK):
                        for shift in range(8):
                            image=bytearray([0xa5])*0x4000
                            put(image,8,BASE+0x100+shift);put(image,16,BASE+0x180+shift)
                            put(image,0x100+shift,badowner);put(image,0x180+shift,MASK)
                            if arch=='amd64':
                                machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,BASE+0x3f00)
                                put(image,0x3f00,STOP)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                            machine.mem_write(BASE,bytes(image));writes.clear();machine.emu_start(CODE,STOP,count=150)
                            assert machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)==0
                            assert bytes(machine.mem_read(BASE,0x4000))==image and not writes
                            checks+=1
                for shift in range(8):
                    for value in (0,MASK,BASE+0x540):
                        for old in (0,MASK,BASE+0x540,BASE+0x740):
                            for refs in (0,1,2,(1<<32)-1,1<<32,MASK-1,MASK):
                                for color in (-1,0,1,2,3,0x7fffffff):
                                    for livecolor in (0,1,2):
                                        image=bytearray([0xa5])*0x4000
                                        source=BASE+0x100+shift;dest=BASE+0x180+shift
                                        put(image,0,BASE+0x3000);put(image,8,source);put(image,16,dest)
                                        put(image,0x100+shift,BASE+0x900 if op>=2 else value)
                                        put(image,0x180+shift,old);put(image,40,value)
                                        put(image,0x540-24,refs);put(image,0x540-32,color,4)
                                        put(image,0x900+(24 if op==2 else 8),value)
                                        put(image,0x2000,livecolor,4);put(image,0x2008,livecolor&1,4)
                                        expected=bytearray(image)
                                        accepted=(old==MASK or old==value) and value!=0 and (value!=MASK or op!=1) and (value==MASK or (refs not in (0,MASK)))
                                        if accepted:
                                            if value!=MASK and old==MASK:put(expected,0x540-24,refs+1)
                                            if value!=MASK and color!=livecolor:
                                                put(expected,0x540-32,3,4);put(expected,0x2008,1,4)
                                            put(expected,0x180+shift,value)
                                            if op>=2:put(expected,8,BASE+0x900+(24 if op==2 else 8))
                                        machine.mem_write(BASE,bytes(image))
                                        if arch=='amd64':
                                            machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,BASE+0x3f00)
                                            machine.mem_write(BASE+0x3f00,struct.pack('<Q',STOP));put(expected,0x3f00,STOP)
                                            machine.reg_write(UC_X86_REG_MXCSR,0x5fa5)
                                        else:
                                            machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                            machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x95)
                                        writes.clear();machine.emu_start(CODE,STOP,count=150)
                                        actual=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                                        if not accepted:assert not writes
                                        if accepted and old==value and value!=MASK:
                                            assert all(not (address<BASE+0x540-16 and address+size>BASE+0x540-24) for address,size in writes)
                                        assert actual==int(accepted),(arch,wb,op,shift,value,old,refs,color,livecolor,actual)
                                        assert bytes(machine.mem_read(BASE,0x4000))==expected,(arch,wb,op,shift,value,old,refs,color,livecolor)
                                        if arch=='amd64':assert machine.reg_read(UC_X86_REG_MXCSR)==0x5fa5
                                        else:assert machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x95
                                        checks+=1
        print('PASS',arch,'guarded owner leaves, cumulative memory/GC/width/alignment/FP checks',checks)
