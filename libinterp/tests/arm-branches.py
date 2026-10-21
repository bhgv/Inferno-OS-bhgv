#!/usr/bin/env python3
"""Production ARM word comparison selection, executed with boundary load stubs.

Operand loads use MOVW/MOVT and scheduler/Dis target relocation are stubs;
production cbra, condition macros and comparison encodings remain unchanged.
"""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_R0
root=Path(__file__).resolve().parents[2]
s=(root/'libinterp/comp-arm.c').read_text()
def function(name,result):
    a=s.index(result+'\n'+name+'(');b=s.index('\n}',a)+2;return s[a:b]
header=s[s.index('#define\tRESCHED'):s.index('static\tulong*\tcode;')]
unit=r'''
#include <stdint.h>
#include "isa.h"
#define ulong uint32_t
'''+header+r'''
static ulong *code;
typedef struct Adr {int32_t imm,ind;struct Inst *ins;} Adr;
typedef struct Inst {uint8_t add;uint16_t reg;Adr s,d;} Inst;
static struct {Inst *prog;} module,*mod=&module;
static uint32_t source,middle;
static void schedcheck(Inst *i){(void)i;}
static void value(uint32_t v,int r){
 *code++=0xe3000000|(r<<12)|((v&0xf000)<<4)|(v&0xfff);
 *code++=0xe3400000|(r<<12)|((v>>12)&0xf0000)|((v>>16)&0xfff);
}
static void opwld(Inst *i,int op,int r){(void)op;value(UXSRC(i->add)==SRC(AIMM)?(uint32_t)i->s.imm:source,r);}
static void mid(Inst *i,int op,int r){(void)op;value((i->add&ARM)==AXIMM?(uint32_t)(int32_t)(int16_t)i->reg:middle,r);}
#undef BRADIS
#define BRADIS(cc,pc) do{DPI(AL,Mov,0,R0,0,0);DPI(cc,Mov,0,R0,0,1);}while(0)
'''+function('swapbraop','static int')+'\n'+function('cbra','static void')+r'''
int emit(uint32_t *out,int cc,int mode,int32_t a,int32_t b){
 Inst i={0};code=out;source=a;middle=b;
 i.add=mode==1?SRC(AIMM)|AXINF:mode==2?SRC(AFP)|AXIMM:SRC(AFP)|AXINF;
 i.s.imm=a;i.reg=b;cbra(&i,cc);return (int)(code-out)*4;
}
'''
rng=random.Random(0xa32be)
checks=0
with tempfile.TemporaryDirectory(prefix='arm-branches-') as td:
    td=Path(td);src=td/'branches.c';src.write_text(unit);binary=td/'branches.so'
    flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-shared','-fPIC','-I'+str(root/'include'),str(src),'-o',str(binary)],check=True)
    lib=C.CDLL(str(binary));lib.emit.argtypes=[C.c_void_p]+[C.c_int]*4
    vals=[-2147483648,-2147483647,-65536,-32768,-256,-255,-129,-128,-1,0,1,127,128,255,256,32767,65536,2147483647]
    pairs=[(a,b) for a in vals for b in vals]+[(C.c_int32(rng.getrandbits(32)).value,C.c_int32(rng.getrandbits(32)).value) for _ in range(300)]
    for cc,predicate in [(0,lambda a,b:a==b),(1,lambda a,b:a!=b),(11,lambda a,b:a<b),(13,lambda a,b:a<=b),(12,lambda a,b:a>b),(10,lambda a,b:a>=b)]:
        for mode in range(3):
            for a,b in pairs:
                out=(C.c_uint32*64)();size=lib.emit(out,cc,mode,a,b)
                blob=bytes(out)[:size];u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(0x10000,4096);u.mem_write(0x10000,blob)
                try:u.emu_start(0x10000,0x10000+size,count=100)
                except Exception as e:raise AssertionError((cc,mode,a,b,size,blob.hex())) from e
                actual=u.reg_read(UC_ARM_REG_R0);expected=predicate(a,C.c_int16(b).value if mode==2 else b)
                assert actual==expected,(cc,mode,a,b,actual,expected,blob.hex())
                checks+=1
print(f'PASS ARM branches: {checks} executed signed comparison cases, source/middle immediates and INT32_MIN under UBSan')
