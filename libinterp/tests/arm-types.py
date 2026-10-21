#!/usr/bin/env python3
"""Production ARM type-helper emitters, dynamic sizing and executed initializers.

Needs Unicorn. The host C harness models ARM32 scalars and the target constant
sizeof(WORD*)=4. Generated initializer code is executed in a separate ARM memory
space; C call ABI, native destruction and GC scheduling remain outside scope.
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
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_R3,UC_ARM_REG_LR
root=Path(__file__).resolve().parents[2]
source=(root/'libinterp/comp-arm.c').read_text()
def function(name,result):
    start=source.index(result+'\n'+name+'(');brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(name)
header=source[source.index('#define\tRESCHED'):source.index('static\tulong*\tcode;')]
constants=source[source.index('typedef struct Const'):source.index('static Con rcon;')+len('static Con rcon;')]
unit=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define ulong uint32_t
#define uchar uint8_t
#define H ((void*)(uintptr_t)0xffffffff)
#define nil NULL
#define USED(x) (void)(x)
#define O(t,f) 0
#include "isa.h"
HEADER
CONSTANTS
static ulong modulebase[16];
static ulong *armhelpertemp,*armhelpercode;
static ulong *code,*base=modulebase,*litpool;
static ulong macro[MacRELQ+1];
static int pass=1,codeoff,cflag;
int __lsan_is_turned_off(void) {return 1;}
static int live,fail_after=-1,flush_result;
static ulong lastbytes;
static void *testmallocz(size_t n,int zero) {
    void *p;(void)zero;
    if(fail_after==0)return nil;
    if(fail_after>0)fail_after--;
    p=calloc(1,n?n:1);if(p)live++;return p;
}
static void testfree(void *p) {if(p){live--;free(p);}}
#define mallocz testmallocz
#define free testfree
static void print(char *s,...) {(void)s;}
static void urk(char *s) {(void)s;assert(0);}
static int segflush(void *p,ulong n) {(void)p;lastbytes=n;return flush_result;}
typedef struct Type {int size,np;void *initialize,*destroy;uchar map[1];} Type;
static void flushchk(void);
static void gen(ulong w);
GENERATOR
HELPERS
static Type *current;
void drop(void) {if(current){free(current->initialize);free(current);current=nil;}assert(!live);}
int build(const uchar *map,int np,int failure,int cache_error) {
    drop();lastbytes=0;
    if(np<0 || np>100000)return 0;
    current=testmallocz(sizeof(Type)+np,1);assert(current);
    current->np=np;current->size=np*32;memcpy(current->map,map,np);
    fail_after=failure;flush_result=cache_error;
    typecom(current);fail_after=-1;
    assert(live==(current->initialize ? 2 : 1));
    return current->initialize!=nil;
}
void *initializer(void) {return current->initialize;}
ulong initbytes(void) {return (uchar*)current->destroy-(uchar*)current->initialize;}
ulong totalbytes(void) {return lastbytes;}
void encodings(void) {
    ulong probe[2],condition;
    int cc;
    for(cc=0;cc<16;cc++) {
        condition=(ulong)cc<<28;
        code=probe;LDW(cc,0,1,2);assert(probe[0]==(condition|0x05901002));
        code=probe;STW(cc,0,1,2);assert(probe[0]==(condition|0x05801002));
        code=probe;DPI(cc,Mov,0,1,0,1);assert(probe[0]==(condition|0x03a01001));
        assert(BRAW(cc,1)==(condition|0x0a000001));
    }
}
void badmaps(void) {
    Type t={0};t.np=-1;typecom(&t);assert(!t.initialize && !t.destroy);
    t.np=0x7fffffff;typecom(&t);assert(!t.initialize && !t.destroy);
    t.np=0x7fffffff/4/128;typecom(&t);assert(!t.initialize && !t.destroy);
}
#ifdef SELFTEST
int main(void) {
    static uchar map[4096];
    int sizes[]={0,1,127,512,513,1024,4096},i,pattern,n;
    encodings();badmaps();
    for(i=0;i<7;i++)for(pattern=0;pattern<3;pattern++) {
        n=sizes[i];memset(map,pattern==0 ? 0xff : pattern==1 ? 0 : 0x91,n);
        assert(build(map,n,-1,0));
        assert(!build(map,n,0,0));assert(!build(map,n,1,0));
        assert(!build(map,n,-1,-1));
    }
    drop();puts("PASS ARM Type helper native generation: large maps, allocation/cache failures and explicit allocation accounting");
    return 0;
}
#endif
'''.replace('HEADER',header).replace('CONSTANTS',constants)
generator='\n'.join(function(n,'static '+r) for n,r in [('gen','void'),('immrot','long'),('flushcon','void'),('flushchk','void'),('ccon','void'),('memc','void'),('con','void'),('mem','void')])
helpers='\n'.join(function(n,'void') for n in ['comi','comd','typecom'])
# Evaluate this target-specific sizeof as the actual ARM32 compiler does.
helpers=helpers.replace('sizeof(WORD*)','4')
unit=unit.replace('GENERATOR',generator).replace('HELPERS',helpers)
checks=0
rng=random.Random(0x7a9e)
with tempfile.TemporaryDirectory(prefix='arm-types-') as directory:
    td=Path(directory);path=td/'types.c';path.write_text(unit)
    compiler=shlex.split(os.environ.get('CC','cc'))
    flags=shlex.split(os.environ.get('CFLAGS','-O1 -g'))
    common=['-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-I'+str(root/'include')]
    environment=dict(os.environ);environment.setdefault('UBSAN_OPTIONS','halt_on_error=1')
    subprocess.run(compiler+flags+common+['-DSELFTEST',str(path),'-o',str(td/'native')],check=True)
    subprocess.run([str(td/'native')],check=True,env=environment)
    # ASan runs in the executable above; loading its runtime into Python is not
    # required. Keep UBSan for the shared library when it was requested.
    sharedflags=[]
    for flag in flags:
        if flag.startswith('-fsanitize='):
            modes=[m for m in flag.split('=',1)[1].split(',') if m!='address']
            if modes:sharedflags.append('-fsanitize='+','.join(modes))
        else:sharedflags.append(flag)
    subprocess.run(compiler+sharedflags+common+['-shared','-fPIC',str(path),'-o',str(td/'types.so')],check=True)
    lib=C.CDLL(str(td/'types.so'))
    lib.build.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int];lib.build.restype=C.c_int
    lib.initializer.restype=C.c_void_p
    lib.initbytes.restype=C.c_uint32;lib.totalbytes.restype=C.c_uint32
    lib.encodings();lib.badmaps()
    for np in [0,1,127,512,513,1024,4096]:
        for kind in ['dense','empty','random']:
            mapping=bytes([255]*np) if kind=='dense' else bytes(np) if kind=='empty' else bytes(rng.randrange(256) for _ in range(np))
            buf=C.create_string_buffer(mapping)
            assert lib.build(buf,np,-1,0)
            binary=C.string_at(lib.initializer(),lib.initbytes())
            if np>=1024 and kind=='dense':assert lib.totalbytes()>4096*4
            u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
            code_start=0x100000;code_size=(len(binary)+0xfff)&~0xfff
            u.mem_map(code_start,max(code_size,0x1000));u.mem_write(code_start,binary)
            data_start=0x400000;data_size=((np*32+64)+0xfff)&~0xfff
            u.mem_map(data_start,data_size);u.mem_write(data_start,bytes([0xa5])*(np*32+64))
            # RA2=R3 is the initializer's destination; RLINK=LR is its return PC.
            u.reg_write(UC_ARM_REG_R3,data_start+32);u.reg_write(UC_ARM_REG_LR,0x200000)
            u.emu_start(code_start,0x200000,count=1000000)
            actual=bytes(u.mem_read(data_start,np*32+64))
            expected=bytearray([0xa5])*(np*32+64)
            for i,byte in enumerate(mapping):
                for bit in range(8):
                    if byte&(0x80>>bit):struct.pack_into('<I',expected,32+i*32+bit*4,0xffffffff)
            assert actual==expected
            checks+=1
            for failure in [0,1]:
                assert not lib.build(buf,np,failure,0);checks+=1
            assert not lib.build(buf,np,-1,-1);checks+=1
    lib.drop()
print(f'PASS ARM Type helpers: {checks} dense/sparse/large map, executed initializer, buffer-canary and allocation/cache failure checks')
