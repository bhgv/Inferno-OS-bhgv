#!/usr/bin/env python3
"""Check six array leaves and lazy metadata/span contracts on AMD64/ARM64."""
import ctypes as C
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]  # Production repository root.
OPS = ('INDB','INDW','INDL','INDF','INDX','LENA')  # Six address/length commands.
BASE = 0x100000000  # Independent guest register/data base.
CODE = 0x10000  # Executable guest mapping.
STOP = CODE+0x1000  # Return sentinel outside emitted leaves.
MASK = (1<<64)-1  # Native pointer limit.


# Require a once-evaluated invariant, including with Python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Write modulo-width bytes to the owned guest image without alignment assumptions.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1<<(width*8))-1)).to_bytes(width,'little')


with tempfile.TemporaryDirectory(prefix='array-slot-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('ARRAY_SLOT_ARCHES','amd64 arm64').split():
        header = Path(os.environ.get('ARRAY_SLOT_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output selects sizing only.
#include "isa.h"
#include "'''+str(header)+'''"


/* Borrow out for cap code units; return size, or zero without publication. */
int emit(u32int *out,int cap,int wb,int op)
{
    LeafLayout a={0};
    static int ops[]={'''+','.join('I'+x for x in OPS)+'''};
    a.wordbytes=wb;
    a.pc=0;
    a.s=8;
    a.d=16;
    a.m=24;
    a.arraylen=0;
    a.arraytype=8;
    a.arraydata=24;
    a.typesize=24;
    return leafemit(out,cap,&a,ops[op]);
}
''')
        library = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(library)],check=True)
        lib = C.CDLL(str(library))
        lib.emit.argtypes=[C.POINTER(C.c_uint32),C.c_int,C.c_int,C.c_int]
        machine = Uc(UC_ARCH_X86 if arch=='amd64' else UC_ARCH_ARM64,UC_MODE_64 if arch=='amd64' else 0)
        machine.mem_map(CODE,0x3000)
        machine.mem_map(BASE,0x4000)
        # Unicorn x86 masks physical addresses to 52 bits; virtual arithmetic stays 64-bit.
        top=(1<<52)-4096 if arch=='amd64' else (1<<64)-4096
        machine.mem_map(top,4096)
        reads, writes = [], []


        # Record guest reads so poisoned siblings expose premature operand access.
        def read_hook(machine,access,address,size,value,context):
            reads.append((address,size))


        # Record all stores, including same-value writes on native decline.
        def write_hook(machine,access,address,size,value,context):
            writes.append((address,size))


        machine.hook_add(UC_HOOK_MEM_READ,read_hook)
        machine.hook_add(UC_HOOK_MEM_WRITE,write_hook)
        checks=0
        for wb in (4,8):
            for op,name in enumerate(OPS):
                if os.environ.get('ARRAY_SLOT_OP') and op!=int(os.environ['ARRAY_SLOT_OP']):
                    continue
                sw=8
                mw=dw=wb
                out=(C.c_uint32*257)(*([0xa5a5a5a5]*257))
                n=lib.emit(out,256,wb,op)
                require(0<n<=128 and lib.emit(None,256,wb,op)==n,(arch,wb,name,'sizing'))
                exact=(C.c_uint32*(n+1))(*([0xa5a5a5a5]*(n+1)))
                require(lib.emit(exact,n,wb,op)==n and exact[n]==0xa5a5a5a5 and bytes(exact)[:n*4]==bytes(out)[:n*4],'exact capacity/canary')
                for cap in range(n):
                    short=(C.c_uint32*257)(*([0xa5a5a5a5]*257))
                    require(lib.emit(short,cap,wb,op)==0 and lib.emit(None,cap,wb,op)==0 and list(short)==[0xa5a5a5a5]*257,'short capacity publication')
                machine.mem_write(CODE,bytes(out)[:n*4])
                machine.ctl_remove_cache(CODE,CODE+1024)
                maximum=(1<<(wb*8-1))-1
                cases=[('slot',slot,pointer) for slot,width in ((('s',8),('d',wb)) if op==5 else (('s',8),('d',wb),('m',wb)))
                       for pointer in [0,MASK]+[MASK-i for i in range(1,width)]]
                cases += [('owner',pointer,0) for pointer in [0,MASK]+[MASK-i for i in range(1,wb)]]
                cases += [('type',pointer,0) for pointer in [0,MASK]+[MASK-i for i in range(1,28)]]
                cases += [('negative-owner',0,0)] if op!=5 else []
                cases += [('top',length,index) for length in (-3,1,8) for index in (-1,0,1,maximum)]
                cases += [('value',(length,index,stride),data) for length in (-1,0,1,8,maximum)
                          for index in ((0,) if op==5 else (-1,0,1,7,8,1<<31,maximum-1,maximum))
                          for stride in ((-1,0,1,3,8,16) if op==4 else (0,))
                          for data in (0,MASK,BASE+0x2800,MASK-3)]
                selected=os.environ.get('ARRAY_SLOT_CASE')
                if selected:
                    require(selected in ('slot','owner','type','top','negative-owner','value'),'unknown ARRAY_SLOT_CASE')
                    cases=[case for case in cases if case[0]==selected]
                for kind,a,b in cases:
                    # All alignments for slot guards; full aliases for the new native scale range.
                    for alignment in (range(8) if kind in ('slot','owner','type') else (1,)):
                        for alias in ((0,1,2,3,4) if kind=='value' else (0,)):
                            image=bytearray(b'\xa5'*0x4000)
                            topimage=bytearray(b'\xa5'*4096)
                            s,d,m=BASE+0x100+alignment,BASE+0x200+alignment,BASE+0x300+alignment
                            owner,descriptor=BASE+0x1000+alignment,BASE+0x1800+alignment
                            length,index,stride,backing=3,0,8,BASE+0x2800
                            valid=True
                            if kind=='value':
                                length,index,stride=a
                                backing=b
                            width=1 if op==0 else wb if op==1 else stride if op==4 else 8
                            put(image,owner-BASE,length,wb)
                            put(image,owner-BASE+8,descriptor)
                            put(image,owner-BASE+24,backing)
                            put(image,descriptor-BASE+24,stride,4)
                            if alias:
                                result_slot=s if alias==1 else d if alias==2 else s+1 if alias==3 else owner
                                if op==5: d=result_slot
                                else: m=result_slot
                            put(image,s-BASE,owner)
                            if op!=5: put(image,d-BASE,index,wb)
                            if kind=='owner':
                                owner=a
                                put(image,s-BASE,owner)
                                valid=op==5 and owner in (0,MASK)
                                length=0
                            elif kind=='type':
                                put(image,owner-BASE+8,a)
                                valid=op!=4
                            elif kind=='top':
                                length,index=a,b
                                owner=MASK-wb
                                put(image,s-BASE,owner)
                                if op!=5: put(image,d-BASE,index,wb)
                                put(topimage,4095-wb,length,wb)
                                valid=op==5
                            elif kind=='negative-owner':
                                owner=1
                                index=-1
                                put(image,s-BASE,owner)
                                put(image,d-BASE,index,wb)
                                valid=False
                            elif kind=='slot':
                                if a=='s': s,d=b,1
                                elif a=='d': d,s=b,1
                                else: m,s,d=b,1,1
                                valid=False
                            if kind=='value':
                                valid=op==5 or (index>=0 and index<length and width>=0 and
                                      (width==0 or (backing not in (0,MASK) and index<=0x7fffffff//width and backing<=MASK-index*width)))
                            result=length if op==5 else backing+(index*width if width>0 else 0)
                            if op==5: m=1  # LENA must ignore the unused middle field.
                            put(image,0,BASE+0x3900)
                            put(image,8,s)
                            put(image,16,d)
                            put(image,24,m)
                            expected=bytearray(image)
                            if valid:
                                put(expected,(d if op==5 else m)-BASE,result,dw)
                            machine.mem_write(BASE,bytes(image))
                            machine.mem_write(top,bytes(topimage))
                            if arch=='amd64':
                                machine.reg_write(UC_X86_REG_RDI,BASE)
                                machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'))
                                machine.reg_write(UC_X86_REG_MXCSR,0x1f84)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0,BASE)
                                machine.reg_write(UC_ARM64_REG_LR,STOP)
                                machine.reg_write(UC_ARM64_REG_FPCR,0)
                                machine.reg_write(UC_ARM64_REG_FPSR,2)
                            reads.clear()
                            writes.clear()
                            machine.emu_start(CODE,STOP,count=2000)
                            completed=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                            detail=(arch,wb,name,kind,a,b,alignment,alias)
                            require(completed==int(valid) and bytes(machine.mem_read(BASE,0x4000))==expected,detail)
                            require(bytes(machine.mem_read(top,4096))==topimage,(detail,'top page changed'))
                            if kind=='top':
                                require([record for record in reads if top <= record[0] < top+4096]==([] if op!=5 and index<0 else [(top+4095-wb,wb)]),(detail,'non-lazy top metadata read'))
                            if not valid:
                                require(not writes,(detail,'decline wrote'))
                                require((machine.reg_read(UC_X86_REG_MXCSR)==0x1f84) if arch=='amd64' else machine.reg_read(UC_ARM64_REG_FPSR)==2,(detail,'decline FP effects'))
                            checks+=1
        print('PASS',arch,'array slots/headers/lazy metadata/aliases:',checks,'cases',flush=True)
