#!/usr/bin/env python3
"""Execute production ARM byte-conversion and string/array address paths.

Projected ARM32 layouts; real address/constant/call emitters and literal pools.
Error C bodies are recording boundaries, not native VM signal/handler tests.
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
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R5,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_CPSR
root=Path(__file__).resolve().parents[2]
s=(root/'libinterp/comp-arm.c').read_text()
def function(name,result='static void'):
    a=s.index(result+'\n'+name+'(');brace=s.index('{',a);depth=0
    for t in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',s[brace:]):
        if t.group()=='{':depth+=1
        elif t.group()=='}':
            depth-=1
            if not depth:return s[a:brace+t.end()]
    raise ValueError(name)
def cases(first,last):
    a=s.index('\tcase '+first+':',s.index('comp(Inst *i)'));b=s.index('\tcase '+last+':',a);return s[a:b]
header=s[s.index('#define\tRESCHED'):s.index('static\tulong*\tcode;')]
constants=s[s.index('typedef struct Const'):s.index('static Con rcon;')+len('static Con rcon;')]
names=[('gen','void'),('immrot','long'),('flushcon','void'),('flushchk','void'),('ccon','void'),('con','void'),('armcall','void'),('memc','void'),('mem','void'),('opx','void'),('opwld','void'),('opwst','void'),('mid','void')]
unit=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define ulong uint32_t
typedef uint8_t uchar;
#define nil NULL
#define O(t,f) ((uint32_t)offsetof(t,f))
#include "isa.h"
'''+header+constants+r'''
typedef union Adr {int32_t imm,ind;struct {uint16_t f,s;} i;struct Inst *ins;} Adr;
typedef struct Inst {uint8_t op,add;uint16_t reg;Adr s,d;} Inst;
typedef struct REG {uint32_t st,dt;} REG;
typedef struct String {int32_t len,max;uint32_t tmp,data;} String;
typedef struct Array {uint32_t t;int32_t len;uint32_t root,data;} Array;
typedef struct Type {int32_t size;} Type;
static ulong *code;static int pass,codeoff,bflag;
static REG R;
static void flushchk(void);static void gen(ulong);
static void urk(char *s){(void)s;abort();}
static void print(char *s,...){(void)s;}
static void __attribute__((aligned(16))) bounds(void){}
static void __attribute__((aligned(16))) nullity(void){}
'''+ '\n'.join(function(n,'static '+r) for n,r in names)+r'''
static ulong image[2048];
int emit(int op,int mode,int arg,int darg,int checked){
 Inst obj={0},*i=&obj;int r,imm,sizes[2];bflag=checked;
 i->op=op;i->add=SRC(AFP)|DST(AFP)|AXINF;i->s.ind=0x100;i->reg=0x200;i->d.ind=0x300;
 if(mode==1){i->add=SRC(AMP)|DST(AMP)|AXINM;}
 if(mode==2){i->add=SRC(AIMM)|DST(AFP)|AXINF;i->s.imm=arg;}
 if(mode==3){i->add=SRC(AIND|AFP)|DST(AIND|AMP)|AXINF;i->s.ind=(0x20<<16)|0x40;i->d.ind=(0x30<<16)|0x48;}
 if(mode==4){i->add=SRC(AFP)|DST(AFP)|AXIMM;i->reg=arg;}
 if(mode==5){i->add=SRC(AFP)|DST(AIMM)|AXINF;i->d.imm=darg;}
 for(pass=0;pass<2;pass++){
  code=image;codeoff=0;rcon.ptr=0;
  switch(op){
'''+cases('ICVTBW','ICVTWB')+cases('IINDC','IADDL')+r'''
  default:abort();
  }
  flushcon(1);sizes[pass]=code-image;
 }
 assert(sizes[0]==sizes[1]&&sizes[1]<2048);return sizes[1]*4;
}
void *blob(void){return image;}
unsigned erroraddr(int is_nil){return (uint32_t)(uintptr_t)(is_nil?nullity:bounds);}
int opid(int n){static int ops[]={ICVTBW,IINDC,IINDB,IINDW,IINDL,IINDF,IINDX};return ops[n];}
'''
checks=0
rng=random.Random(0xa32add)
with tempfile.TemporaryDirectory(prefix='arm-indexing-') as td:
    td=Path(td);(td/'emit.c').write_text(unit)
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+['-shared','-fPIC','-I'+str(root/'include'),str(td/'emit.c'),'-o',str(td/'emit.so')],check=True)
    lib=C.CDLL(str(td/'emit.so'));lib.blob.restype=C.c_void_p;lib.erroraddr.restype=C.c_uint32
    ops={n:lib.opid(j) for j,n in enumerate(['ICVTBW','IINDC','IINDB','IINDW','IINDL','IINDF','IINDX'])}
    BASE,FP,MP,REG=0x10000,0x20000,0x30000,0x40000
    def setup(op,mode=0,arg=0,darg=0,checked=1,obj=0x50000):
        size=lib.emit(ops[op],mode,C.c_int32(arg),C.c_int32(darg),checked)
        code=C.string_at(lib.blob(),size)
        u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
        pages={BASE,FP,MP,REG,obj}
        errors={lib.erroraddr(0):'bounds',lib.erroraddr(1):'nil'}
        pages.update(p&~4095 for p in errors)
        for p in pages:u.mem_map(p,4096)
        u.mem_write(BASE,code);u.mem_write(FP,b'\xa5'*4096);u.mem_write(MP,b'\xa5'*4096)
        u.reg_write(UC_ARM_REG_R5,REG);u.reg_write(UC_ARM_REG_R9,FP);u.reg_write(UC_ARM_REG_R8,MP)
        saddr=(MP if mode==1 else FP)+0x100;daddr=(MP if mode in (1,3) else FP)+0x300;maddr=(MP if mode==1 else FP)+0x200
        if mode==3:
            saddr=FP+0x120;daddr=MP+0x330
            u.mem_write(FP+0x40,struct.pack('<I',saddr-0x20));u.mem_write(MP+0x48,struct.pack('<I',daddr-0x30))
        state=[]
        def hook(uc,pc,n,data):
            if pc in errors:state.append(errors[pc]);uc.emu_stop()
        u.hook_add(UC_HOOK_CODE,hook)
        return u,size,saddr,maddr,daddr,state
    def execute(test,expected,error=None,result=None,flags=0):
        global checks
        u,size,sa,ma,da,state=test
        before=[bytes(u.mem_read(p,4096)) for p in (FP,MP)]
        u.reg_write(UC_ARM_REG_CPSR,(u.reg_read(UC_ARM_REG_CPSR)&0x0fffffff)|(flags<<28))
        u.emu_start(BASE,BASE+size,count=1000)
        assert state==([] if error is None else [error]),(expected,error,state)
        dest=da if result is None else result
        assert int.from_bytes(u.mem_read(dest,4),'little')==(0xa5a5a5a5 if error else expected)&0xffffffff,(expected,error,bytes(u.mem_read(dest,4)).hex())
        after=[bytes(u.mem_read(p,4096)) for p in (FP,MP)]
        for page,old,new in zip((FP,MP),before,after):
            target=bytearray(old)
            if error is None and page<=dest<page+4096:target[dest-page:dest-page+4]=(expected&0xffffffff).to_bytes(4,'little')
            assert new==target,'surrounding memory changed'
        checks+=1
    # Conversion uses the same real immediate/indirect operand loader as arithmetic.
    for mode in [0,1,2,3]:
        for value in [0,1,127,128,255,256,257,-1,-128,-255,-256,-257,-2147483648,2147483647]+[C.c_int32(rng.getrandbits(32)).value for _ in range(100)]:
            t=setup('ICVTBW',mode,value);u,_,sa,_,_,_=t
            u.mem_write(sa,struct.pack('<I',value&0xffffffff));execute(t,value&255)
    # A positive pointer previously left LT false even for a negative Rune length.
    for obj in [0x50000,0x80050000]:
        for length in [0,1,4,17,-1,-4,-17]:
            for mode in [0,1,3,4]:
                for index in [-32768,-1024,-1,0,1,3,4,16,17,255,1023,32767]:
                    for flags in [0,5,10,15]:
                        t=setup('IINDC',mode,index,obj=obj);u,_,sa,ma,_,_=t
                        u.mem_write(sa,struct.pack('<I',obj));u.mem_write(ma,struct.pack('<i',index))
                        data=bytes((j*29+7)&255 for j in range(256))
                        u.mem_write(obj,struct.pack('<iii',length,0,0)+data)
                        valid=0<=index<abs(length)
                        expected=int.from_bytes(data[index*4:index*4+4],'little') if length<0 and valid else data[index] if valid else 0
                        execute(t,expected,None if valid else 'bounds',flags=flags)
    # Typed fixed-width and general array indices, including immediate invalid indices.
    for op,stride in [('IINDB',1),('IINDW',4),('IINDL',8),('IINDF',8),('IINDX',12)]:
        for mode in [0,1,3,5]:
            for index in [-2147483648,-32768,-1,0,1,3,4,15,16,255,0x40000000,2147483647]:
                t=setup(op,mode,darg=index);u,_,sa,_,da,_=t
                u.mem_write(sa,struct.pack('<I',0x50000));u.mem_write(0x50000,struct.pack('<IiII',0x50080,16,0,0x50100));u.mem_write(0x50080,struct.pack('<I',stride))
                if mode!=5:u.mem_write(da,struct.pack('<i',index))
                # ind instructions put the result through the middle operand.
                result=(MP if mode==1 else FP)+0x200
                execute(t,0x50100+index*stride,None if 0<=index<16 else 'bounds',result=result)
    # Nil remains rejected even when optional bounds checks are disabled.
    for op in ['IINDC','IINDB','IINDW','IINDL','IINDF','IINDX']:
        for mode in [0,1,3]:
            for checked in [0,1]:
                t=setup(op,mode,checked=checked);u,_,sa,ma,da,_=t
                u.mem_write(sa,struct.pack('<I',0xffffffff))
                result=da if op=='IINDC' else ma
                execute(t,0,'nil',result=result)
    # Valid unchecked strings retain both representation paths.
    for length in [4,-4]:
        for mode in [0,1,3,4]:
            for index in range(4):
                t=setup('IINDC',mode,index,checked=0);u,_,sa,ma,_,_=t
                u.mem_write(sa,struct.pack('<I',0x50000));u.mem_write(ma,struct.pack('<i',index))
                data=bytes(range(32));u.mem_write(0x50000,struct.pack('<iii',length,0,0)+data)
                execute(t,int.from_bytes(data[index*4:index*4+4],'little') if length<0 else data[index])
print(f'PASS ARM indexing: {checks} byte conversion, ASCII/Rune bounds and array address cases with native emitters and UBSan')
