#!/usr/bin/env python3
"""Execute production x87 comparison/address/jump encoders against IEEE predicates.

Scheduling is a no-op; VM operands and native branch destinations are real.
Native FP traps are masked, as in the hosted VM. This is not an OS boot.
"""
import ctypes as C
import operator
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (UC_X86_REG_ESI, UC_X86_REG_EDI,
                              UC_X86_REG_EBP, UC_X86_REG_FPCW,
                              UC_X86_REG_FPSW)
root=Path(__file__).resolve().parents[2]
src=(root/'libinterp/comp-386.c').read_text()
def function(name):
    result='static int' if name=='bc' else 'static void'
    a=src.index(result+'\n'+name+'(')
    return src[a:src.index('\n}',a)+2]
header=src[src.index('enum\n'):src.index('static\tuchar*\tcode;')]
unit=r'''
#include <stdint.h>
#include <stdlib.h>
#include "isa.h"
#define ulong uint32_t
typedef unsigned char uchar;
typedef union Adr {int32_t ind,imm;struct {uint16_t f,s;} i;struct Inst *ins;} Adr;
typedef struct Inst {uint8_t op,add;uint16_t reg;Adr s,d;} Inst;
'''+header+r'''
static uchar *code,*base;
static ulong offsets[1],*patch=offsets;
static struct {Inst *prog;} module,*mod=&module;
static void urk(void){abort();}
#define print(...) ((void)0)
#define DOT ((ulong)(uintptr_t)code)
#define RESCHED 1
static void schedcheck(Inst *i){(void)i;}
'''+ '\n'.join(function(n) for n in ('bc','genb','gen2','genw','modrm','con','opwld','opwst','mid','rbra','cbraf'))+r'''
int emit(uchar *out,int mode,int cc){
 Inst i={0};code=out;base=out;mod->prog=&i;i.d.ins=&i;patch[0]=0x400;
 i.add=mode==1?SRC(AMP)|AXINM:mode==2?SRC(AFP)|AXINM:mode==3?SRC(AIND|AFP)|AXINF:mode==4?SRC(AIND|AMP)|AXINM:SRC(AFP)|AXINF;
 i.s.ind=mode>=3?(0x20<<16)|0x40:0x100;
 i.reg=0x200;
 cbraf(&i,cc);
 return code-out;
}
'''
checks=0
rng=random.Random(0x87be)
values=[0,1,0x8000000000000000,0x8000000000000001,
        0x0010000000000000,0x8010000000000000,
        0x3ff0000000000000,0xbff0000000000000,
        0x7fefffffffffffff,0xffefffffffffffff,
        0x7ff0000000000000,0xfff0000000000000,
        0x7ff8000000000001,0xfff8000000001234,
        0x7ff0000000000001,0xfff0000000001234]
pairs=[(a,b) for a in values for b in values]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(100)]
with tempfile.TemporaryDirectory(prefix='x86-float-branches-') as td:
    td=Path(td);(td/'emit.c').write_text(unit)
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+['-shared','-fPIC','-I'+str(root/'include'),str(td/'emit.c'),'-o',str(td/'emit.so')],check=True)
    lib=C.CDLL(str(td/'emit.so'));lib.emit.argtypes=[C.c_void_p,C.c_int,C.c_int]
    for mode in range(5):
        for cc,predicate in [(0x84,operator.eq),(0x85,operator.ne),(0x82,operator.lt),(0x86,operator.le),(0x83,operator.ge),(0x87,operator.gt)]:
            out=(C.c_ubyte*256)();size=lib.emit(out,mode,cc);assert 0<size<256
            u=Uc(UC_ARCH_X86,UC_MODE_32)
            BASE,FP,MP=0x10000,0x20000,0x30000
            for address in (BASE,FP,MP):u.mem_map(address,0x10000)
            # Fallthrough and taken blocks both stop at the same end PC.
            u.mem_write(BASE,bytes(out[:size])+b'\xbd\x00\x00\x00\x00\xe9'+struct.pack('<i',0x500-size-10))
            u.mem_write(BASE+0x400,b'\xbd\x01\x00\x00\x00\xe9'+struct.pack('<i',0x500-0x400-10))
            for a,b in pairs:
                for rounding in (0,1,2,3):
                    sa=(MP if mode in (1,4) else FP)+0x100
                    ma=(MP if mode in (1,2,4) else FP)+0x200
                    if mode>=3:
                        u.mem_write((MP if mode==4 else FP)+0x40,struct.pack('<I',sa-0x20))
                    u.mem_write(sa,struct.pack('<Q',a));u.mem_write(ma,struct.pack('<Q',b))
                    u.reg_write(UC_X86_REG_ESI,FP);u.reg_write(UC_X86_REG_EDI,MP)
                    u.reg_write(UC_X86_REG_FPCW,0x37f|(rounding<<10))
                    u.reg_write(UC_X86_REG_FPSW,0)
                    u.reg_write(UC_X86_REG_EBP,0xdeadbeef)
                    u.emu_start(BASE,BASE+0x500,count=100)
                    av=struct.unpack('<d',struct.pack('<Q',a))[0];bv=struct.unpack('<d',struct.pack('<Q',b))[0]
                    actual=u.reg_read(UC_X86_REG_EBP);expected=predicate(av,bv)
                    assert actual==expected,(mode,hex(cc),hex(a),hex(b),rounding,actual,expected)
                    assert (u.reg_read(UC_X86_REG_FPSW)>>11)&7==0,'x87 stack leaked'
                    checks+=1
print(f'PASS x86 float branches: {checks} executed IEEE comparisons, NaNs, rounding modes and addressing forms')
