#!/usr/bin/env python3
"""Execute generic/compact AMD64 field loads with low and extended destinations."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
from unicorn.x86_const import (
    UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RSI,
    UC_X86_REG_RDI, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_R10,
    UC_X86_REG_R11, UC_X86_REG_RSP,
)

ROOT = Path(__file__).resolve().parents[2]
CODE, STOP, DATA = 0x10000, 0x11000, 0x100000000
REGISTERS = {
    0: UC_X86_REG_RAX, 1: UC_X86_REG_RCX, 2: UC_X86_REG_RDX,
    6: UC_X86_REG_RSI, 7: UC_X86_REG_RDI, 8: UC_X86_REG_R8,
    9: UC_X86_REG_R9, 10: UC_X86_REG_R10, 11: UC_X86_REG_R11,
}

with tempfile.TemporaryDirectory(prefix='amd64-fields-') as directory:
    td = Path(directory)
    source = td/'fields.c'
    source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // No runtime heap dependencies in the instruction fixture.
#include "isa.h"
#include "amd64-emit.h"


/* Query the production field helpers without repeating their REX/ModRM encoding. */
int
emit(uchar *out,int result,int owner,int offset,int bytes,int compact)
{
 X64Buf b;
 b.n=0;
 if(compact)x64modulefield(&b,result,owner,offset,bytes);
 else x64field(&b,result,owner,offset,bytes);
 x64byte(&b,0xc3);
 memcpy(out,b.bytes,b.n);
 return b.n;
}
''')
    binary = td/'fields.so'
    command = shlex.split(os.environ.get('CC','cc'))
    command += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
    lib = C.CDLL(str(binary))
    lib.emit.argtypes = [C.POINTER(C.c_ubyte)]+[C.c_int]*5
    machine = Uc(UC_ARCH_X86,UC_MODE_64)
    machine.mem_map(CODE,0x3000)
    machine.mem_map(DATA,0x2000)
    dis = Cs(CS_ARCH_X86,CS_MODE_64)
    checks = 0
    for compact in (0,1):
        for result in REGISTERS:
            for owner in REGISTERS:
                for offset in (-256,-129,-128,-1,0,1,7,8,127,128,255,256,2040):
                    for width in (4,8):
                        out = (C.c_ubyte*64)()
                        n = lib.emit(out,result,owner,offset,width,compact)
                        blob = bytes(out)[:n]
                        if not 0<n<=64 or sum(i.size for i in dis.disasm(blob,CODE))!=n:
                            raise RuntimeError('Invalid field-load instruction boundaries')
                        machine.mem_write(CODE,blob)
                        machine.ctl_remove_cache(CODE,CODE+64)
                        values = (0,1,0x7fffffff,0x80000000,0xffffffff) if width==4 else (0,1,0xffffffff,1<<63,(1<<64)-1)
                        for value in values:
                            for register in REGISTERS.values():
                                machine.reg_write(register,0xa5a5a5a5a5a5a5a5)
                            machine.reg_write(REGISTERS[owner],DATA+0x800)
                            machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                            machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'))
                            machine.mem_write(DATA+0x800+offset,value.to_bytes(width,'little'))
                            machine.emu_start(CODE,STOP,count=10)
                            expected = value | (0xffffffff00000000 if width==4 and value&(1<<31) else 0)
                            actual = machine.reg_read(REGISTERS[result])
                            if actual!=expected:
                                raise RuntimeError((compact,result,owner,offset,width,value,actual,expected))
                            checks += 1
    print('PASS AMD64 field-load register/displacement/sign checks:',checks)
