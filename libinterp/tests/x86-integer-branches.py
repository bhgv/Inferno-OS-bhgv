#!/usr/bin/env python3
"""Execute production IA-32 byte/word/big comparison and addressing encoders.

Real relative branches; scheduler is a boundary no-op. No full VM boot.
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
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP
root=Path(__file__).resolve().parents[2]
s=(root/'libinterp/comp-386.c').read_text()
def function(name):
    a=s.index(('static int' if name in ['bc','swapbraop'] else 'static void')+'\n'+name+'(')
    return s[a:s.index('\n}',a)+2]
header=s[s.index('enum\n'):s.index('static\tuchar*\tcode;')]
unit=r'''
#include <stdint.h>
#include <stdlib.h>
#include "isa.h"
#define ulong uint32_t
typedef unsigned char uchar;
#define nil NULL
#define DOT ((ulong)(uintptr_t)code)
#define RESCHED 1
typedef union Adr {int32_t ind,imm;struct {uint16_t f,s;} i;struct Inst *ins;} Adr;
typedef struct Inst {uint8_t op,add;uint16_t reg;Adr s,d;} Inst;
'''+header+r'''
static uchar *code,*base;
static ulong offsets[1],*patch=offsets;
static struct {Inst *prog;} module,*mod=&module;
static void urk(void){abort();}
#define print(...) ((void)0)
static void schedcheck(Inst *i){(void)i;}
'''+ '\n'.join(function(n) for n in ['bc','genb','gen2','genw','modrm','con','opwld','opwst','mid','rbra','cmpl','swapbraop','cbra','cbrab','cbral'])+r'''
int emit(uchar *out,int bits,int relation,int mode,int a,int b){
 Inst i={0};code=base=out;mod->prog=&i;patch[0]=0x400;i.d.ins=&i;
 i.add=mode==1?SRC(AMP)|AXINM:mode==2?SRC(AIMM)|AXINF:mode==3?SRC(AFP)|AXIMM:mode==4?SRC(AIND|AFP)|AXINM:mode==5?SRC(AMP)|AXINF:SRC(AFP)|AXINF;
 i.s.ind=mode==2?a:mode==4?(0x20<<16)|0x40:0x100;i.reg=mode==3?b:0x200;
 static int word[]={Ojeql,Ojnel,Ojltl,Ojlel,Ojgtl,Ojgel};
 static int byte[]={Ojeql,Ojnel,Ojbl,Ojbel,Ojal,Ojael};
 static int high[]={Ojnel,Ojnel,Ojltl,Ojltl,Ojgtl,Ojgtl};
 static int low[]={Ojeql,Ojnel,Ojbl,Ojbel,Ojal,Ojael};
 if(bits==64)cbral(&i,high[relation],low[relation],relation==0?ANDAND:relation==1?OROR:EQAND);
 else if(bits==8)cbrab(&i,byte[relation]);else cbra(&i,word[relation]);
 return code-out;
}
'''
checks=0
rng=random.Random(0xb386)
with tempfile.TemporaryDirectory(prefix='x86-integer-branches-') as td:
    td=Path(td);(td/'emit.c').write_text(unit)
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+['-shared','-fPIC','-I'+str(root/'include'),str(td/'emit.c'),'-o',str(td/'emit.so')],check=True)
    lib=C.CDLL(str(td/'emit.so'));lib.emit.argtypes=[C.c_void_p]+[C.c_int]*5
    BASE,FP,MP=0x10000,0x20000,0x30000
    predicates=[operator.eq,operator.ne,operator.lt,operator.le,operator.gt,operator.ge]
    for bits in [8,32,64]:
        mask=(1<<bits)-1
        vals=[0,1,2,127,128,255,256,32767,32768,0x7fffffff,0x80000000,0xffffffff,(1<<(bits-1))-1,1<<(bits-1),mask,mask-1]
        pairs=[(a,b) for a in vals for b in vals]+[(rng.getrandbits(bits),rng.getrandbits(bits)) for _ in range(100)]
        for mode in ([0,1,4,5] if bits==64 else range(6)):
            for relation,predicate in enumerate(predicates):
                for a,b in pairs:
                    out=(C.c_ubyte*256)();size=lib.emit(out,bits,relation,mode,C.c_int32(a),C.c_int32(b));assert 0<size<256
                    u=Uc(UC_ARCH_X86,UC_MODE_32)
                    for page in [BASE,FP,MP]:u.mem_map(page,4096)
                    u.mem_write(BASE,bytes(out[:size])+b'\xbd\x00\x00\x00\x00\xe9'+struct.pack('<i',0x500-size-10))
                    u.mem_write(BASE+0x400,b'\xbd\x01\x00\x00\x00\xe9'+struct.pack('<i',0x500-0x400-10))
                    u.reg_write(UC_X86_REG_ESI,FP);u.reg_write(UC_X86_REG_EDI,MP)
                    sa=(MP if mode in [1,5] else FP)+0x100;ma=(MP if mode in [1,4] else FP)+0x200
                    if mode==4:
                        sa=FP+0x120;u.mem_write(FP+0x40,struct.pack('<I',sa-0x20))
                    u.mem_write(sa,(a&mask).to_bytes(bits//8,'little'));u.mem_write(ma,(b&mask).to_bytes(bits//8,'little'))
                    av=a&mask;bv=(C.c_int16(b).value if mode==3 else b)&mask
                    if bits!=8:
                        av=av-(1<<bits) if av>>(bits-1) else av;bv=bv-(1<<bits) if bv>>(bits-1) else bv
                    u.emu_start(BASE,BASE+0x500,count=100)
                    actual=u.reg_read(UC_X86_REG_EBP);assert actual==predicate(av,bv),(bits,relation,mode,a,b,av,bv,actual)
                    checks+=1
print(f'PASS x86 integer branches: {checks} byte/word/big, immediate and indirect executed comparisons under UBSan')
