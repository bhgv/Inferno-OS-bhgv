#!/usr/bin/env python3
"""Execute Type helpers and their permanent reclaim thunk after module unmap.

Production helper/preamble/thunk encoders run with projected 32-bit REG/Heap
layouts. Only the C destroy boundary is simulated; no native targets/calls are
rewritten. This does not boot Inferno or execute the host GC/pool allocator.
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
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EDX
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R5,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R12,UC_ARM_REG_LR,UC_ARM_REG_SP
root=Path(__file__).resolve().parents[2]
common=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define ulong uint32_t
typedef uint8_t uchar;
#define H ((void*)(uintptr_t)0xffffffff)
#define nil NULL
#define USED(x) (void)(x)
#define O(t,f) ((uint32_t)offsetof(t,f))
#include "isa.h"
typedef struct Heap {uint32_t color,ref,t,hprof;} Heap;
typedef struct REG {uint32_t PC,MP,FP,SP,TS,EX,M,IC,xpc,s,d,m,t,st,dt;} REG;
static REG R;
typedef struct Type {int size,np;void *initialize,*destroy;uchar map[1];} Type;
static void __attribute__((aligned(16))) rdestroy(void) {}
static int live;
static void *alloc(size_t n,int zero){
 uchar *p=calloc(1,n+sizeof(n)+32);(void)zero;if(!p)return nil;
 memcpy(p,&n,sizeof(n));memset(p+sizeof(n)+n,0xa5,32);live++;return p+sizeof(n);
}
static void release(void *v){
 uchar *p=v;size_t n;int i;if(!p)return;
 p-=sizeof(n);memcpy(&n,p,sizeof(n));for(i=0;i<32;i++)assert(p[sizeof(n)+n+i]==0xa5);
 live--;free(p);
}
#define malloc(n) alloc(n,0)
#define mallocz alloc
#define free release
static void print(char *s,...){(void)s;}
static void urk(char *s){(void)s;abort();}
static void (*comvec)(void);
static void *shared;
static unsigned sharedbytes,helperbytes;
static int segflush(void *p,ulong n){if(!comvec){shared=p;sharedbytes=n;}else helperbytes=n;return 0;}
'''
def function(source,name,result):
    a=source.index(result+'\n'+name+'(');brace=source.index('{',a);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[a:brace+token.end()]
    raise ValueError(name)
def unit(arch):
    s=(root/f'libinterp/comp-{arch}.c').read_text()
    if arch=='386':
        header=s[s.index('enum\n'):s.index('static\tuchar*\tcode;')]
        globals=r'''
#define DOT ((ulong)(uintptr_t)code)
static uchar *code,*base=(uchar*)0x6000000,*x86frp,*x86ret,*x86helpertemp,*x86helpercode;
static ulong macro[NMACRO];static int cflag;
'''
        globals+='static void retbody(void) { *code++=0xc3; }\n'
        names=[('bc','static int')]+[(n,'static void') for n in ['genb','gen2','genw','modrm','con','bra','cmpl','macfrp']]+[('preamble','static int')]+[(n,'void') for n in ['comi','comd','typecom']]
        setup=''
    else:
        header=s[s.index('#define\tRESCHED'):s.index('static\tulong*\tcode;')]
        globals=s[s.index('typedef struct Const'):s.index('static Con rcon;')+len('static Con rcon;')]+r'''
static ulong *code,*base=(ulong*)0x6000000,*armfrp,*armret,*armhelpertemp,*armhelpercode;
static ulong macro[MacRELQ+1];static int pass,codeoff,cflag;
static void flushchk(void);static void gen(ulong);
'''
        globals+='static void retbody(void) { *code++=0xe1a0f00e; }\n'
        names=[(n,'static '+r) for n,r in [('gen','void'),('immrot','long'),('flushcon','void'),('flushchk','void'),('ccon','void'),('con','void'),('armcall','void'),('memc','void'),('mem','void'),('macfrp','void'),('preamble','int')]]+[(n,'void') for n in ['comi','comd','typecom']]
        setup='pass=1;codeoff=0;rcon.ptr=0;'
    return common+header+globals+'\n'.join(function(s,n,r) for n,r in names).replace('sizeof(WORD*)','4').replace('sizeof(Heap)','16U')+r'''
static Type *current;
void drop(void){if(current){free(current->initialize);free(current);current=nil;}assert(live==(comvec!=nil));}
int build(const uchar *map,int np){
 drop();assert(preamble());SETUP
 current=alloc(sizeof(Type)+np,1);assert(current);current->np=np;current->size=np*32;memcpy(current->map,map,np);
 typecom(current);assert(current->initialize&&current->destroy&&live==3);return 1;
}
void *image(int which){return which==0?shared:current->initialize;}
unsigned imagesize(int which){return which==0?sharedbytes:helperbytes;}
unsigned destroyaddr(void){return (uint32_t)(uintptr_t)current->destroy;}
unsigned regaddr(void){return (uint32_t)(uintptr_t)&R;}
unsigned callback(void){return (uint32_t)(uintptr_t)rdestroy;}
void done(void){drop();free((void*)comvec);comvec=nil;assert(live==0);}
'''.replace('SETUP',setup)
def mapregions(u,regions):
    pages=set()
    for address,data in regions:
        pages.update(range(address&~4095,(address+len(data)+4095)&~4095,4096))
    for page in sorted(pages):u.mem_map(page,4096)
    for address,data in regions:u.mem_write(address,data)
if __name__ == '__main__':
    checks=0
    rng=random.Random(0x1fe71)
    with tempfile.TemporaryDirectory(prefix='jit-lifetime-') as td:
        td=Path(td)
        for arch in ['386','arm']:
            src=td/f'{arch}.c';src.write_text(unit(arch));binary=td/f'{arch}.so'
            flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-Wno-pointer-to-int-cast','-shared','-fPIC','-I'+str(root/'include'),str(src),'-o',str(binary)],check=True)
            lib=C.CDLL(str(binary));lib.build.argtypes=[C.c_void_p,C.c_int];lib.image.restype=C.c_void_p
            for np in [0,1,17,513]:
                for pattern in ['empty','dense','random']:
                    mapping=bytes(np) if pattern=='empty' else bytes([255]*np) if pattern=='dense' else bytes(rng.randrange(256) for _ in range(np))
                    buf=C.create_string_buffer(mapping);lib.build(buf,np)
                    codeimages=[(C.c_uint32(lib.image(i)).value,C.string_at(lib.image(i),lib.imagesize(i))) for i in range(2)]
                    shared,helper=codeimages[0][0],codeimages[1][0]
                    destroy=lib.destroyaddr()&0xffffffff;reg=lib.regaddr()&0xffffffff;callback=lib.callback()&0xffffffff
                    for moveframe in [False,True]:
                        FRAME,NEWFRAME,OBJECTS,STACK,STOP,MODULE=0x400000,0x440000,0x500000,0x800000,0xa00000,0x6000000
                        size=np*32+64
                        u=Uc(UC_ARCH_X86,UC_MODE_32) if arch=='386' else Uc(UC_ARCH_ARM,UC_MODE_ARM)
                        stub=b'\xc3' if arch=='386' else struct.pack('<II',0xe92d4000,0xe8bd8000)
                        regions=codeimages+[(reg,bytes(60)),(FRAME,b'\xa5'*size),(NEWFRAME,bytes(size)),(OBJECTS,bytes(4096)),(STACK,bytes(4096)),(STOP,bytes(16)),(callback,stub),(MODULE,bytes(4096))]
                        mapregions(u,regions);u.mem_unmap(MODULE,4096)
                        # Initializers must be independently executable as well.
                        if arch=='386':
                            u.reg_write(UC_X86_REG_ECX,FRAME+32);u.reg_write(UC_X86_REG_ESP,STACK+2048);u.mem_write(STACK+2048,struct.pack('<I',STOP))
                        else:
                            u.reg_write(UC_ARM_REG_R3,FRAME+32);u.reg_write(UC_ARM_REG_LR,STOP)
                        u.emu_start(helper,STOP,count=2000000)
                        expected=bytearray(b'\xa5'*size);slots=[]
                        for i,byte in enumerate(mapping):
                            for bit in range(8):
                                if byte&(0x80>>bit):
                                    off=32+i*32+bit*4;struct.pack_into('<I',expected,off,0xffffffff);slots.append(off)
                        assert bytes(u.mem_read(FRAME,size))==expected,(arch,np,pattern,'initializer')
                        checks+=1
                        counts={};targets=[]
                        for i,off in enumerate(slots):
                            # Include nil and aliasing children, exercising fast decrement
                            # followed by a slow destroy only for the last alias.
                            pointer=0xffffffff if i%5==0 else OBJECTS+16+(i%16)*32
                            struct.pack_into('<I',expected,off,pointer)
                            if pointer!=0xffffffff:counts[pointer]=counts.get(pointer,0)+1;targets.append(pointer)
                        for pointer,ref in counts.items():u.mem_write(pointer-12,struct.pack('<I',ref))
                        u.mem_write(FRAME,bytes(expected));state=bytearray(60)
                        struct.pack_into('<I',state,4,0x460000);struct.pack_into('<I',state,8,FRAME+32)
                        u.mem_write(reg,bytes(state));calls=[];moved=[False]
                        def called(uc,address,length,data):
                            hook=callback if arch=='386' else callback+4
                            if address!=hook:return
                            pointer=struct.unpack('<I',uc.mem_read(reg+36,4))[0]
                            assert pointer in counts and struct.unpack('<I',uc.mem_read(pointer-12,4))[0]==1,(arch,'destroy before final ref',pointer)
                            uc.mem_write(pointer-12,struct.pack('<I',0));calls.append(pointer)
                            if moveframe and not moved[0]:
                                uc.mem_write(NEWFRAME,bytes(uc.mem_read(FRAME,size)))
                                uc.mem_write(reg+8,struct.pack('<I',NEWFRAME+32));moved[0]=True
                            uc.mem_write(reg+4,struct.pack('<I',0x470000))
                            volatile=[UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX] if arch=='386' else [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R12,UC_ARM_REG_LR]
                            for register in volatile:uc.reg_write(register,0xdeadbeef)
                        u.hook_add(UC_HOOK_CODE,called)
                        if arch=='386':
                            u.reg_write(UC_X86_REG_ESI,FRAME+32);u.reg_write(UC_X86_REG_EDI,0x460000);u.reg_write(UC_X86_REG_ESP,STACK+2048);u.mem_write(STACK+2048,struct.pack('<I',STOP))
                        else:
                            u.reg_write(UC_ARM_REG_R5,reg);u.reg_write(UC_ARM_REG_R9,FRAME+32);u.reg_write(UC_ARM_REG_R8,0x460000);u.reg_write(UC_ARM_REG_SP,STACK+2048);u.reg_write(UC_ARM_REG_LR,STOP)
                        u.emu_start(destroy,STOP,count=2000000)
                        assert len(calls)==len(set(targets)),(arch,np,pattern,'missing slow frees',len(calls),len(set(targets)))
                        assert len(calls)==len(set(calls))
                        for pointer in set(targets):assert struct.unpack('<I',u.mem_read(pointer-12,4))[0]==0
                        active=NEWFRAME if moved[0] else FRAME
                        assert bytes(u.mem_read(active,size))==expected,(arch,'frame writes')
                        fp=u.reg_read(UC_X86_REG_ESI if arch=='386' else UC_ARM_REG_R9)
                        mp=u.reg_read(UC_X86_REG_EDI if arch=='386' else UC_ARM_REG_R8)
                        assert fp==active+32 and mp==(0x470000 if calls else 0x460000),(arch,'REG restore',fp,mp)
                        checks+=1
            lib.done()
    print(f'PASS JIT lifetime: {checks} executed x86/ARM initializer/destructor cases after module unmap, real permanent reclaim thunks, nil/shared children, refcounts, volatile clobbers and frame/MP reloads')
