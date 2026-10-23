#!/usr/bin/env python3
"""Production 386 type-helper emitters, dynamic sizing and executed initializers.

Needs Unicorn. The host C harness models x86-32 scalars and the target constant
sizeof(WORD*)=4. Generated initializer code is executed in a separate 386 memory
space; C call ABI, destructor execution and GC scheduling remain outside scope.
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
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
root=Path(__file__).resolve().parents[2]
source=Path(os.environ.get('X86_TYPE_SOURCE', root/'libinterp/comp-386.c')).read_text()
def function(name,result):
    start=source.index(result+'\n'+name+'(');brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(name)
header=source[source.index('#define DOT'):source.index('static\tuchar*\tcode;')]

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
static uchar modulebase[16];
static uchar *x86helpertemp,*x86helpercode;
static uchar *code,*base=modulebase;
static ulong *litpool;
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
static char exNomem[]="memory";
static void error(char *s) {(void)s;assert(0);}
static int segflush(void *p,ulong n) {(void)p;lastbytes=n;return flush_result;}
typedef struct Type {int size,np;void *initialize,*destroy;uchar map[1];} Type;

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
/* Exercise word and large-displacement writes at every byte alignment. */
void words(void) {
    uchar probe[32]; int offset,i;
    for(offset=0;offset<8;offset++) {
        memset(probe,0xa5,sizeof(probe));code=probe+offset;genw(0x89abcdef);
        assert(code==probe+offset+4);
        assert(probe[offset]==0xef && probe[offset+1]==0xcd && probe[offset+2]==0xab && probe[offset+3]==0x89);
        for(i=0;i<32;i++)if(i<offset || i>=offset+4)assert(probe[i]==0xa5);
        memset(probe,0xa5,sizeof(probe));code=probe+offset;modrm(Oldw,0x12345678,RCX,RAX);
        assert(code==probe+offset+6 && probe[offset]==Oldw && probe[offset+1]==0x81);
        assert(probe[offset+2]==0x78 && probe[offset+3]==0x56 && probe[offset+4]==0x34 && probe[offset+5]==0x12);
        for(i=0;i<32;i++)if(i<offset || i>=offset+6)assert(probe[i]==0xa5);
    }
}
void badmaps(void) {
    Type t={0};t.np=-1;typecom(&t);assert(!t.initialize && !t.destroy);
    t.np=0x7fffffff;typecom(&t);assert(!t.initialize && !t.destroy);
    t.np=0x7fffffff/256;typecom(&t);assert(!t.initialize && !t.destroy);
}
#ifdef SELFTEST
int main(void) {
    static uchar map[4096];
    int sizes[]={0,1,127,512,513,1024,4096},i,pattern,n;
    if(getenv("X86_TYPE_CASES") && !strcmp(getenv("X86_TYPE_CASES"),"words")) {words();return 0;}
    if(getenv("X86_TYPE_CASES") && !strcmp(getenv("X86_TYPE_CASES"),"large")) {
        memset(map,0xff,sizeof(map));assert(build(map,4096,-1,0));drop();return 0;
    }
    words();badmaps();
    for(i=0;i<7;i++)for(pattern=0;pattern<3;pattern++) {
        n=sizes[i];memset(map,pattern==0 ? 0xff : pattern==1 ? 0 : 0x91,n);
        assert(build(map,n,-1,0));
        assert(!build(map,n,0,0));assert(!build(map,n,1,0));
        assert(!build(map,n,-1,-1));
    }
    drop();puts("PASS 386 Type helper native generation: large maps, allocation/cache failures and explicit allocation accounting");
    return 0;
}
#endif
'''.replace('HEADER',header)
generator='\n'.join(function(n,'static '+r) for n,r in [('bc','int'),('genb','void'),('gen2','void'),('genw','void'),('modrm','void'),('con','void'),('rbra','void')])
helpers='\n'.join(function(n,'void') for n in ['comi','comd','typecom'])
# Evaluate this target-specific sizeof as the actual x86-32 compiler does.
helpers=helpers.replace('sizeof(WORD*)','4')
unit=unit.replace('GENERATOR',generator).replace('HELPERS',helpers)
checks=0
rng=random.Random(0x7a9e)
with tempfile.TemporaryDirectory(prefix='x86-types-') as directory:
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
    lib.words();lib.badmaps()
    for np in [0,1,127,512,513,1024,4096]:
        for kind in ['dense','empty','random']:
            mapping=bytes([255]*np) if kind=='dense' else bytes(np) if kind=='empty' else bytes(rng.randrange(256) for _ in range(np))
            buf=C.create_string_buffer(mapping)
            assert lib.build(buf,np,-1,0)
            binary=C.string_at(lib.initializer(),lib.initbytes())
            if np>=1024 and kind=='dense':assert lib.totalbytes()>4096
            u=Uc(UC_ARCH_X86,UC_MODE_32)
            code_start=0x100000;code_size=(len(binary)+0xfff)&~0xfff
            u.mem_map(code_start,max(code_size,0x1000));u.mem_write(code_start,binary)
            data_start=0x400000;data_size=((np*32+64)+0xfff)&~0xfff
            u.mem_map(data_start,data_size);u.mem_write(data_start,bytes([0xa5])*(np*32+64))
            # ECX is the initializer destination; RET consumes the synthetic return PC.
            u.mem_map(0x300000,4096);u.mem_write(0x300000,struct.pack('<I',0x200000))
            u.reg_write(UC_X86_REG_ECX,data_start+32);u.reg_write(UC_X86_REG_ESP,0x300000)
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
print(f'PASS 386 Type helpers: {checks} dense/sparse/large map, executed initializer, buffer-canary and allocation/cache failure checks')
