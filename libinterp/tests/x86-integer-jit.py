#!/usr/bin/env python3
"""Execute production x86 integer emitters in Unicorn against a Python oracle.

Uses production addressing/ALU encoders and selected production switch bodies;
VM calls/punt are a recording stub, not a full compiled-module execution.
"""
import ctypes as C
import os
from pathlib import Path
import random
import re
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EFLAGS
root=Path(__file__).resolve().parents[2]
src=(root/'libinterp/comp-386.c').read_text()
def function(name):
    start=src.index(('static int' if name=='bc' else 'static void')+'\n'+name+'(')
    end=src.index('\n}',start)+2
    return src[start:end]
start=src.index('comp(Inst *i)')
def case(name):
    a=src.index('\tcase '+name+':',start)
    return src[a:src.index('\t\tbreak;',a)+len('\t\tbreak;')]
names=['I'+a+b for a in ('ADD','SUB','MUL','AND','OR','XOR') for b in ('B','W')]+['I'+a+'L' for a in ('ADD','SUB','AND','OR','XOR')]+['ICVTWL','IMOVF','IMOVL']
fallback=['ISHLB','ISHLW','ISHRB','ISHRW','ISHLL','ISHRL','IDIVB','IDIVW','IDIVL','IMODB','IMODW','IMODL','ILSRW','ILSRL','IMULX','IDIVX','ICVTXX','IMULX0','IDIVX0','ICVTXX0','IMULX1','IDIVX1','ICVTXX1','ICVTFX']
# Groups for a helper can be reached from multiple starting labels; obtain
# unique full blocks directly by splitting the production switch on break.
allblocks=src[start:src.index('\nstatic int\npreamble',start)].split('\t\tbreak;')
groups=[]
for block in allblocks:
    labels=re.findall(r'\tcase (\w+):',block)
    if any(n in fallback for n in labels):
        # These target groups contain only fallthrough labels and punt.
        a=block.index('\tcase ')
        groups.append(block[a:]+'\t\tbreak;')
header=src[src.index('enum\n'):src.index('static\tuchar*\tcode;')]
nativecases=[]
seen=set()
for n in names:
    if n not in seen:
        block=case(n);nativecases.append(block);seen.update(re.findall(r'case (\w+):',block))
unit=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "isa.h"
#define ulong uint32_t
typedef uint8_t uchar;
typedef union Adr {int32_t ind,imm;struct {uint16_t f,s;} i;} Adr;
typedef struct Inst {uint8_t op,add;uint16_t reg;Adr s,d;} Inst;
'''+header+r'''
static uchar *code;
static int puntmask;
static void (*optab[256])(void);
static void urk(void){abort();}
#define print(...) ((void)0)
static void punt(Inst *i,int mask,void(*fn)(void)){(void)i;(void)fn;puntmask=mask;}
'''+ '\n'.join(function(n) for n in ['bc','genb','gen2','genw','modrm','con','opwld','opwst','mid','arith','arithb','larith'])+r'''
int emit(uchar *out,int op,int add,int s,int m,int d){
 Inst obj={0},*i=&obj;
 i->op=op;i->add=add;i->s.ind=s;i->reg=m;i->d.ind=d;
 code=out;puntmask=0;
 switch(op){
'''+ '\n'.join(nativecases)+'\n'.join(groups)+r'''
 default:abort();
 }
 return puntmask?-puntmask:(int)(code-out);
}
int opid(int i){static int ops[]={OPS};return ops[i];}
int mode(int i){static int modes[]={
 SRC(AFP)|DST(AFP)|AXINF,
 SRC(AMP)|DST(AFP)|AXINM,
 SRC(AFP)|DST(AMP)|AXNON,
 SRC(AIMM)|DST(AFP)|AXINF,
 SRC(AFP)|DST(AFP)|AXIMM,
 SRC(AIND|AFP)|DST(AIND|AMP)|AXINF
};return modes[i];}
'''.replace('OPS',','.join(names+fallback))
rng=random.Random(0x386cab)
checks=0
with tempfile.TemporaryDirectory(prefix='x86-integer-jit-') as td:
    td=Path(td);f=td/'emit.c';f.write_text(unit)
    binary=td/'emit.so'
    flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-shared','-fPIC','-I'+str(root/'include'),str(f),'-o',str(binary)],check=True)
    lib=C.CDLL(str(binary));lib.emit.argtypes=[C.c_void_p]+[C.c_int]*5
    ids={n:lib.opid(i) for i,n in enumerate(names+fallback)}
    for n in fallback:
        out=(C.c_ubyte*256)();assert lib.emit(out,ids[n],lib.mode(0),0x100,0x200,0x300)==-(1|2|64),n
        checks+=1
    BASE,FP,MP=0x10000,0x20000,0x30000
    for name in names[:-3]:
        bits=8 if name.endswith('B') else 32 if name.endswith('W') else 64
        mask=(1<<bits)-1
        vectors=[0,1,2,mask,mask-1,1<<(bits-1),(1<<(bits-1))-1]
        pairs=[(a,b) for a in vectors for b in vectors]+[(rng.getrandbits(bits),rng.getrandbits(bits)) for _ in range(80)]
        forms=[(mode,imm) for mode in range(6) for imm in
               ([-2147483648,-129,-128,-1,0,126,127,128,2147483647] if mode==3 else [-129])]
        for mode,imm in forms:
            if mode==3 and bits!=32:continue
            if mode==4 and bits!=32:continue
            for alias in (0,1,2):
                # Source, explicit middle and destination addresses.
                dest=[0x300,0x100,0x200][alias]
                middle=0x200
                source=0x100
                if mode==3:source=imm
                if mode==4:middle=-17
                if mode==5:source=(0x20<<16)|0x40
                ds=(0x30<<16)|0x48 if mode==5 else dest
                out=(C.c_ubyte*256)();size=lib.emit(out,ids[name],lib.mode(mode),source,middle,ds)
                assert 0<size<256,(name,mode,size)
                blob=bytes(out[:size]);u=Uc(UC_ARCH_X86,UC_MODE_32)
                for address in (BASE,FP,MP):u.mem_map(address,0x10000)
                u.mem_write(BASE,blob)
                for a,b in pairs:
                    u.mem_write(FP,b'\xa5'*0x500);u.mem_write(MP,b'\xa5'*0x500)
                    saddr=(MP if mode==1 else FP)+0x100
                    maddr=(MP if mode==1 else FP)+0x200
                    daddr=(MP if mode in (2,5) else FP)+dest
                    if mode==5:
                        saddr=FP+0x120;daddr=MP+0x330
                        u.mem_write(FP+0x40,struct.pack('<I',FP+0x100))
                        u.mem_write(MP+0x48,struct.pack('<I',MP+0x300))
                    u.mem_write(saddr,(a&mask).to_bytes(bits//8,'little'))
                    u.mem_write(maddr,(b&mask).to_bytes(bits//8,'little'))
                    if mode==2:
                        u.mem_write(daddr,(b&mask).to_bytes(bits//8,'little'));maddr=daddr
                    # Read after setup so source/middle aliases have actual VM values.
                    av=source&mask if mode==3 else int.from_bytes(u.mem_read(saddr,bits//8),'little')
                    bv=middle&mask if mode==4 else int.from_bytes(u.mem_read(maddr,bits//8),'little')
                    if mode==5:bv=int.from_bytes(u.mem_read(FP+0x200,bits//8),'little')
                    op=name[1:-1]
                    want={'ADD':lambda:bv+av,'SUB':lambda:bv-av,'MUL':lambda:bv*av,'AND':lambda:bv&av,'OR':lambda:bv|av,'XOR':lambda:bv^av}[op]()&mask
                    expected={FP:bytearray(u.mem_read(FP,0x500)),MP:bytearray(u.mem_read(MP,0x500))}
                    base=FP if daddr<MP else MP
                    expected[base][daddr-base:daddr-base+bits//8]=want.to_bytes(bits//8,'little')
                    u.reg_write(UC_X86_REG_ESI,FP);u.reg_write(UC_X86_REG_EDI,MP);u.reg_write(UC_X86_REG_EFLAGS,2)
                    u.emu_start(BASE,BASE+size,count=100)
                    got=int.from_bytes(u.mem_read(daddr,bits//8),'little')
                    assert got==want,(name,mode,alias,hex(av),hex(bv),hex(got),hex(want),blob.hex())
                    for base in (FP,MP):
                        assert bytes(u.mem_read(base,0x500))==expected[base],(name,mode,alias,'unexpected memory write')
                    checks+=1
    # Complete source capture for overlapping big/real moves and word->big.
    patterns=[0,1,0x80000000,0x7fffffff,0xffffffffffffffff,
              0x7ff0000000000001,0xfff0000000000001,0x8000000000000000]
    patterns += [rng.getrandbits(64) for _ in range(100)]
    for name in names[-3:]:
        for mode in [0,1,2,3,5]:
            if mode==3 and name!='ICVTWL':continue
            for delta in [0,4,-4,0x100]:
                sarg=0x100;darg=0x100+delta
                sr=(MP if mode==1 else FP)+0x4000
                dr=(MP if mode in (2,5) else FP)+0x4000
                saddr=sr+0x100;daddr=dr+0x100+delta
                if mode==5:
                    sarg=(0x20<<16)|0x40;darg=(0x30<<16)|0x48
                    saddr=FP+0x4520;daddr=MP+0x4530
                if mode==3:sarg=-2147483648
                out=(C.c_ubyte*256)();size=lib.emit(out,ids[name],lib.mode(mode),sarg,0,darg)
                blob=bytes(out[:size]);u=Uc(UC_ARCH_X86,UC_MODE_32)
                for address in (BASE,FP,MP):u.mem_map(address,0x10000)
                u.mem_write(BASE,blob)
                for value in patterns:
                    for base in (FP,MP):u.mem_write(base,b'\xa5'*0x8000)
                    if mode==5:
                        u.mem_write(FP+0x4040,struct.pack('<I',FP+0x4500))
                        u.mem_write(MP+0x4048,struct.pack('<I',MP+0x4500))
                    width=4 if name=='ICVTWL' else 8
                    u.mem_write(saddr,(value&((1<<(width*8))-1)).to_bytes(width,'little'))
                    raw=(sarg&0xffffffff).to_bytes(4,'little') if mode==3 else bytes(u.mem_read(saddr,width))
                    want=int.from_bytes(raw,'little',signed=name=='ICVTWL')&((1<<64)-1)
                    expected={base:bytearray(u.mem_read(base,0x8000)) for base in (FP,MP)}
                    destbase=FP if daddr<MP else MP
                    expected[destbase][daddr-destbase:daddr-destbase+8]=want.to_bytes(8,'little')
                    u.reg_write(UC_X86_REG_ESI,FP+0x4000);u.reg_write(UC_X86_REG_EDI,MP+0x4000)
                    u.reg_write(UC_X86_REG_EFLAGS,2)
                    u.emu_start(BASE,BASE+size,count=100)
                    for base in (FP,MP):
                        assert bytes(u.mem_read(base,0x8000))==expected[base],(name,mode,delta,hex(value),'move/conversion memory differs')
                    checks+=1
print(f'PASS x86 integer JIT: {checks} executed oracle/fallback checks, byte/word/big, frame/module/immediate/indirect operands and aliases; production emitters under UBSan')
