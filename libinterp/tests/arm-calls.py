#!/usr/bin/env python3
"""Execute production conditional absolute ARM calls, including far/Thumb code.

Uses production constant pool/sizing/emission; callee code is a tiny return
stub rather than the host C ABI or an OS runtime.
"""
import ctypes as C
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_LR,UC_ARM_REG_CPSR
root=Path(__file__).resolve().parents[2]
s=(root/'libinterp/comp-arm.c').read_text()
def function(name,result):
    a=s.index(result+'\n'+name+'(');b=s.index('\n}',a)+2;return s[a:b]
header=s[s.index('#define\tRESCHED'):s.index('static\tulong*\tcode;')]
constants=s[s.index('typedef struct Const'):s.index('static Con rcon;')+len('static Con rcon;')]
unit=r'''
#include <stdint.h>
#include <assert.h>
#define ulong uint32_t
#include "isa.h"
'''+header+constants+r'''
static ulong *code,*base;static int pass,codeoff;
static void gen(ulong);static void flushchk(void);
static void print(char *s,...){(void)s;}
'''+ '\n'.join(function(n,r) for n,r in [('gen','static void'),('immrot','static long'),('flushcon','static void'),('flushchk','static void'),('ccon','static void'),('armcall','static void')])+r'''
int emit(ulong *out,int cc,ulong target){
 int counts[2];
 for(pass=0;pass<2;pass++){
  code=base=out;codeoff=0;rcon.ptr=0;
  armcall(cc,target);DPI(AL,Mov,0,R1,0,0x5a);flushcon(1);
  counts[pass]=code-out;
 }
 assert(counts[0]==counts[1]);return counts[1]*4;
}
'''
conditions=[lambda n,z,c,v:z,lambda n,z,c,v:not z,lambda n,z,c,v:c,lambda n,z,c,v:not c,
            lambda n,z,c,v:n,lambda n,z,c,v:not n,lambda n,z,c,v:v,lambda n,z,c,v:not v,
            lambda n,z,c,v:c and not z,lambda n,z,c,v:not c or z,lambda n,z,c,v:n==v,
            lambda n,z,c,v:n!=v,lambda n,z,c,v:not z and n==v,lambda n,z,c,v:z or n!=v,
            lambda n,z,c,v:True]
checks=0
with tempfile.TemporaryDirectory(prefix='arm-calls-') as td:
    td=Path(td);src=td/'calls.c';src.write_text(unit);binary=td/'calls.so'
    compilerflags=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for legacy,defines in [(False,[]),(True,['-D__ARM_ARCH=4'])]:
        binary=td/('legacy.so' if legacy else 'calls.so')
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+compilerflags+defines+['-shared','-fPIC','-I'+str(root/'include'),str(src),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.c_void_p,C.c_int,C.c_uint32]
        BASE=0x100000
        for cc,predicate in enumerate(conditions):
            for flags in range(16):
                n,z,c,v=[bool(flags&(1<<bit)) for bit in [3,2,1,0]]
                expected=predicate(n,z,c,v)
                for target in [0x101000,0x9000000,0xf1000000,0x1000]:
                    for thumb in ([False] if legacy else [False,True]):
                        out=(C.c_uint32*64)();size=lib.emit(out,cc,target|int(thumb));blob=bytes(out)[:size]
                        u=Uc(UC_ARCH_ARM,UC_MODE_ARM);u.mem_map(BASE,4096);u.mem_map(target,4096)
                        u.mem_write(BASE,blob)
                        u.mem_write(target,b'\x2a\x20\x70\x47' if thumb else struct.pack('<II',0xe3a0002a,0xe1a0f00e))
                        u.reg_write(UC_ARM_REG_CPSR,(flags<<28)|0x13);u.reg_write(UC_ARM_REG_R0,7);u.reg_write(UC_ARM_REG_LR,0xdeadbeef)
                        entered=[]
                        def called(uc,address,length,data):
                            if address==target:entered.append(address)
                        u.hook_add(UC_HOOK_CODE,called)
                        u.emu_start(BASE,BASE+size,count=100)
                        assert len(entered)==int(expected),(cc,flags,hex(target),thumb,'wrong condition')
                        assert u.reg_read(UC_ARM_REG_R0)==(42 if expected else 7)
                        assert u.reg_read(UC_ARM_REG_R1)==0x5a
                        assert not (u.reg_read(UC_ARM_REG_CPSR)&32),'did not return to A32'
                        if not expected:
                            assert u.reg_read(UC_ARM_REG_CPSR)>>28==flags
                            assert u.reg_read(UC_ARM_REG_LR)==0xdeadbeef
                        checks+=1
print(f'PASS ARM calls: {checks} executed predicates/flags, near/far/high addresses, A32/Thumb interworking, return PCs and two-pass sizing and pre-Thumb ARM checks')
