#!/usr/bin/env python3
"""Execute actual AMD64/ARM64 decimal parsers with independent memory/write oracles."""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR
ROOT=Path(__file__).resolve().parents[2]
BASE=0x100000000;CODE=0x10000;STOP=0x11000;MASK=(1<<64)-1


def put(image, offset, value, size=8):
    """Write native pointers and VM fields independently of the emitter's arithmetic."""
    image[offset:offset+size]=(value & ((1<<(size*8))-1)).to_bytes(size,'little')


# The prefix parser deliberately declines whitespace, trailing text and longer integers.
texts=[b'',b'+',b'-',b'0',b'-0',b'+0',b'1',b'-1',b'999999999',b'-999999999',
       b'+999999999',b'000000000',b'1000000000',b'2147483647',b'-2147483648',
       b'9223372036854775807',b'-9223372036854775808',b' 12',b'12 ',b'12x',
       b'\x0012',b'1\x002',b'++1',b'--1',b'\xff',b'\t1',b'0x1']
texts += [b'1'+bytes([value])+b'2' for value in range(256)]
rng=random.Random(114)
for length in range(1,10):
    for sign in (b'',b'+',b'-'):
        texts.append(sign+bytes(rng.randrange(48,58) for _ in range(length)))
with tempfile.TemporaryDirectory(prefix='decimal-jit-') as directory:
    td=Path(directory);checks=0
    for arch in ('amd64','arm64'):
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include "'''+arch+'''-emit.h"
/* Derive both decimal instructions from one real staged emitter and private layout. */
int emit(u32int *out,int cap,int wb,int big,int field,int value){
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.stringlen=0;a.stringmax=4;a.stringdata=24;
 if(field==1)a.stringlen=value;if(field==2)a.stringdata=value;if(field==3)a.stringmax=value;
 return leafemit(out,cap,&a,big ? ICVTCL : ICVTCW);
}
''')
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*5
        dis=Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[]
        # A fallback may not hide same-value stores to the VM image.
        def write(machine,access,address,size,value,context):
            writes.append((address,size))
        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        for wb in (4,8):
            for big in (0,1):
                out=(C.c_uint32*64)(*[0xa5a5a5a5]*64);n=lib.emit(out,64,wb,big,0,0)
                assert 0<n<=64 and lib.emit(None,64,wb,big,0,0)==n
                blob=bytes(out)[:n*4];assert sum(i.size for i in dis.disasm(blob,CODE))==n*4
                for cap in range(n):
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,cap,wb,big,0,0)==0 and list(guard)==[0xa5a5a5a5]*64;checks+=1
                for field,values in ((1,(-1,1,2048)),(2,(-1,2048)),(3,(-1,1,2048))):
                    for value in values:
                        guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                        assert lib.emit(guard,64,wb,big,field,value)==0 and list(guard)==[0xa5a5a5a5]*64;checks+=1
                machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+256)
                cases=[(text,0) for text in texts]+[(b'12',fault) for fault in range(1,9)]
                for text,fault in cases:
                    body=text[1:] if text[:1] in (b'+',b'-') else text
                    valid=(len(body)<=9 and (not body or all(48<=x<=57 for x in body)))
                    if fault:valid=fault==1
                    value=0 if not body or fault==1 else int(text) if valid else 0
                    for sa in range(8):
                        for da in range(8):
                            for alias in (0,1,2):
                                image=bytearray(b'\xa5'*0x4000);src=0x100+sa
                                dst=src if alias==1 else 0x800+da if alias==2 else 0x200+da
                                owner=BASE+0x800+sa;length=len(text)
                                if fault==1:owner=MASK
                                if fault==2:owner=0
                                if fault==3:length=-len(text)
                                if fault==4:length=0x80000000
                                if fault==5:length=11
                                put(image,8,BASE+src);put(image,16,BASE+dst);put(image,src,owner)
                                put(image,0x800+sa,length,4);put(image,0x804+sa,-1 if fault==7 else len(text)-1 if fault==8 else len(text),4)
                                image[0x818+sa:0x818+sa+len(text)+1]=text+bytes([65 if fault==6 else 0])
                                expected=bytearray(image)
                                if valid:put(expected,dst,value,8 if big else wb)
                                machine.mem_write(BASE,bytes(image));writes.clear()
                                if arch=='amd64':
                                    machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                    machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,0x3f80)
                                else:
                                    machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                    machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x11)
                                machine.emu_start(CODE,STOP,count=1000)
                                result=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                                assert result==valid,(arch,wb,big,text,fault,sa,da,alias,result)
                                assert bytes(machine.mem_read(BASE,0x4000))==bytes(expected),(arch,wb,big,text,fault,sa,da,alias)
                                if not valid:assert not writes
                                if arch=='amd64':assert machine.reg_read(UC_X86_REG_MXCSR)==0x3f80
                                else:assert machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x11
                                checks+=1
        print('PASS',arch,'decimal parse/alias/layout/FP checks, cumulative',checks)
