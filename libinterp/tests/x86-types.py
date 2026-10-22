#!/usr/bin/env python3
"""Production IA-32 Type emitters: large maps, failures and executed helpers.

Pointer size is evaluated as the IA-32 compiler does. Native CALL targets are
kept intact; the permanent reclaim target is a recording return stub.
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
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_EBX
root=Path(__file__).resolve().parents[2]
s=(root/'libinterp/comp-386.c').read_text()
def function(name,result):
    a=s.index(result+'\n'+name+'(');b=s.index('\n}',a)+2
    return s[a:b]
header=s[s.index('enum\n'):s.index('static\tuchar*\tcode;')]
unit=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define ulong uint32_t
typedef unsigned char uchar;
#define DOT ((ulong)(uintptr_t)code)
#define nil NULL
#define H ((void*)(uintptr_t)0xffffffff)
#include "isa.h"
'''+header+r'''
static uchar *code,*base=(uchar*)0x600000,*x86helpertemp,*x86helpercode;
static uchar *x86frp=(uchar*)0x900000;
static ulong macro[NMACRO];
static int cflag;
static size_t bytes;
static int live,fail_after=-1,flush_error;
static void *alloc(size_t n,int zero){
 uchar *p;(void)zero;
 if(fail_after==0)return nil;
 if(fail_after>0)fail_after--;
 p=malloc(n+sizeof(size_t)+32);if(!p)return nil;
 memcpy(p,&n,sizeof(n));memset(p+sizeof(n),0,n);memset(p+sizeof(n)+n,0xa5,32);live++;
 return p+sizeof(n);
}
static void release(void *v){
 uchar *p=v;size_t n;int i;if(!v)return;
 p-=sizeof(n);memcpy(&n,p,sizeof(n));for(i=0;i<32;i++)assert(p[sizeof(n)+n+i]==0xa5);
 live--;free(p);
}
#define mallocz alloc
#define free release
static int segflush(void *p,ulong n){assert(p);bytes=n;return flush_error;}
static void print(char *p,...){(void)p;}
typedef struct Type {int size,np;void *initialize,*destroy;uchar map[1];} Type;
'''+ '\n'.join(function(n,r) for n,r in [('bc','static int'),('genb','static void'),('gen2','static void'),('genw','static void'),('modrm','static void'),('con','static void'),('bra','static void'),('comi','void'),('comd','void'),('typecom','void')]).replace('sizeof(WORD*)','4')+r'''
static Type *current;
void drop(void){if(current){free(current->initialize);free(current);current=nil;}assert(live==0);}
int build(const uchar *map,int np,int failure,int cacheerror){
 drop();bytes=0;
 current=alloc(sizeof(Type)+np,1);assert(current);current->np=np;current->size=np*32;memcpy(current->map,map,np);
 fail_after=failure;flush_error=cacheerror;typecom(current);fail_after=-1;
 assert(live==(current->initialize?2:1));
 assert(!x86helpertemp && !x86helpercode);
 return current->initialize!=nil;
}
void *initializer(void){return current->initialize;}
unsigned initbytes(void){return (uchar*)current->destroy-(uchar*)current->initialize;}
unsigned totalbytes(void){return bytes;}
void badmaps(void){Type t={0};t.np=-1;typecom(&t);assert(!t.initialize&&!t.destroy);t.np=0x7fffffff;typecom(&t);assert(!t.initialize&&!t.destroy);t.np=0x7fffffff/128;typecom(&t);assert(!t.initialize&&!t.destroy);}
'''
rng=random.Random(0x3867)
checks=0
with tempfile.TemporaryDirectory(prefix='x86-types-') as td:
    td=Path(td);src=td/'types.c';src.write_text(unit);binary=td/'types.so'
    flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-Wno-pointer-to-int-cast','-shared','-fPIC','-I'+str(root/'include'),str(src),'-o',str(binary)],check=True)
    lib=C.CDLL(str(binary));lib.build.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int];lib.initializer.restype=C.c_void_p
    lib.badmaps()
    for np in [0,1,15,16,127,512,513,1024,4096]:
        for pattern in ['empty','dense','random']:
            mapping=bytes(np) if pattern=='empty' else bytes([255]*np) if pattern=='dense' else bytes(rng.randrange(256) for _ in range(np))
            buf=C.create_string_buffer(mapping);assert lib.build(buf,np,-1,0)
            init=lib.initbytes();total=lib.totalbytes();blob=bytearray(C.string_at(lib.initializer(),total))
            if np>=512 and pattern=='dense':assert total>4096
            BASE=C.c_uint32(lib.initializer()).value
            DATA,STACK,HOOK,STOP=0x400000,0x800000,0x900000,0xa00000
            # Execute at the encoded virtual address without rewriting calls.
            # The former module block is deliberately unmapped before destruction.
            u=Uc(UC_ARCH_X86,UC_MODE_32)
            for addr,size in [(BASE&~4095,max(4096,(total+(BASE&4095)+4095)&~4095)),(DATA,(np*32+64+4095)&~4095),(STACK,4096),(HOOK,4096),(STOP,4096)]:u.mem_map(addr,size)
            u.mem_map(0x600000,4096);u.mem_unmap(0x600000,4096)
            u.mem_write(BASE,bytes(blob));u.mem_write(HOOK,b'\xc3');u.mem_write(DATA,b'\xa5'*(np*32+64))
            u.reg_write(UC_X86_REG_ECX,DATA+32);u.reg_write(UC_X86_REG_ESP,STACK+2048)
            u.mem_write(STACK+2048,struct.pack('<I',STOP));u.emu_start(BASE,STOP,count=1000000)
            expected=bytearray(b'\xa5'*(np*32+64));slots=[]
            for i,byte in enumerate(mapping):
                for bit in range(8):
                    if byte&(0x80>>bit):
                        off=32+i*32+bit*4;struct.pack_into('<I',expected,off,0xffffffff);slots.append(off)
            assert bytes(u.mem_read(DATA,len(expected)))==expected
            checks+=1
            calls=[]
            for off in slots:struct.pack_into('<I',expected,off,0x10000000+off)
            u.mem_write(DATA,bytes(expected));u.reg_write(UC_X86_REG_ESI,DATA+32)
            u.reg_write(UC_X86_REG_ESP,STACK+2048);u.mem_write(STACK+2048,struct.pack('<I',STOP))
            def called(uc,address,size,data):
                if address==HOOK:
                    calls.append(uc.reg_read(UC_X86_REG_EAX))
                    for r in [UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EBX]:uc.reg_write(r,0xdeadbeef)
            u.hook_add(UC_HOOK_CODE,called)
            u.emu_start(BASE+init,STOP,count=1000000)
            assert calls==[0x10000000+off for off in slots]
            assert bytes(u.mem_read(DATA,len(expected)))==expected
            checks+=1
            for failure in [0,1]:assert not lib.build(buf,np,failure,0);checks+=1
            assert not lib.build(buf,np,-1,-1);checks+=1
    lib.drop()
print(f'PASS x86 Type helpers: {checks} large-map, executed initializer/destructor, canary, allocation/cache failure checks')
