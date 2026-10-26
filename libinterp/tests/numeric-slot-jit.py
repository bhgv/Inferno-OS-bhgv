#!/usr/bin/env python3
"""Check nine numeric leaves and all normal exact power scales on AMD64/ARM64."""
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
OPS = ('ADDF','SUBF','MULF','DIVF','CVTXF','CVTFX','EXPW','EXPL','EXPF')  # Changed numeric commands.
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


with tempfile.TemporaryDirectory(prefix='numeric-slot-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('NUMERIC_SLOT_ARCHES','amd64 arm64').split():
        header = Path(os.environ.get('NUMERIC_SLOT_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')
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
                if os.environ.get('NUMERIC_SLOT_OP') and op!=int(os.environ['NUMERIC_SLOT_OP']):
                    continue
                sw=wb if op==4 or op>=6 else 8
                mw=wb if op==6 else 8
                dw=wb if op in (5,6) else 8
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
                cases=[('slot',slot,pointer) for slot,width in (('s',sw),('m',mw),('d',dw)) for pointer in [0,MASK]+[MASK-i for i in range(1,width)]]
                if op in (6,7):
                    cases += [('zero',e,pointer) for e in (-1,-(1<<(wb*8-1)))
                              for pointer in [0,MASK]+[MASK-i for i in range(1,8)]+[1]]
                    cases += [('power',e,b) for e in (-1,-2,-3,-(1<<(wb*8-1)),0,1,2,3,63,(1<<(wb*8-1))-1)
                              for b in (0,1,-1,2,-2,(1<<(mw*8-1))-1,-(1<<(mw*8-1))) if e>=0 or b!=0]
                elif op==5:
                    cases += [('scale',exponent,sign) for exponent in range(-1022,1024) for sign in range(4)]
                else:
                    cases += [('value',a,b) for a in (-2,-1,1,2) for b in (-2,-1,1,2)]
                for kind,a,b in cases:
                    # All alignments for slot guards; full aliases for the new native scale range.
                    for alignment in (range(8) if kind=='slot' else (1,)):
                        for alias in ((0,1,2,3) if kind in ('scale','power','value') else (0,)):
                            image=bytearray(b'\xa5'*0x4000)
                            s,m,d=BASE+0x100+alignment,BASE+0x200+alignment,BASE+0x300+alignment
                            if alias:
                                d=s if alias==1 else m if alias==2 else s+1
                            valid=kind not in ('slot','zero')
                            put(image,s-BASE,1,sw)
                            put(image,m-BASE,1,mw)
                            result=0
                            if kind=='power':
                                put(image,s-BASE,a,sw)
                                put(image,m-BASE,b,mw)
                                result=pow(b,a,1<<(mw*8)) if a>=0 else 1 if b==1 else (-1 if a&1 else 1) if b==-1 else 0
                            elif kind=='scale':
                                shift=max(-1022,min(1023,-a))
                                # Build exact normal powers from exponent bits; Python arithmetic is independent of emitters.
                                source=((shift+1023)<<52)|((b&1)<<63)
                                scale=((a+1023)<<52)|((bool(b&2))<<63)
                                put(image,s-BASE,source)
                                put(image,m-BASE,scale)
                                product=(-1 if bool(b&1)!=bool(b&2) else 1)*(2.**(shift+a))
                                result=int(product-.5 if product<0 else product+.5)
                            elif kind=='value':
                                if op==4:
                                    put(image,s-BASE,a,sw)
                                elif op==8:
                                    put(image,s-BASE,a,sw)
                                else:
                                    image[s-BASE:s-BASE+8]=struct.pack('<d',float(a))
                                image[m-BASE:m-BASE+8]=struct.pack('<d',float(b))
                                result=(b+a,b-a,b*a,b/a,float(a)*b,0,0,0,float(b)**a)[op]
                                result=int.from_bytes(struct.pack('<d',result),'little')
                            elif kind=='zero':
                                put(image,s-BASE,a,sw)
                                put(image,m-BASE,0,mw)
                                d=b
                            elif kind=='slot':
                                if a=='s': s,m=b,1
                                elif a=='m': m,s=b,1
                                else:
                                    d=b
                                    if op<6 or op==8: s=m=1
                            put(image,0,BASE+0x3900)
                            put(image,8,s)
                            put(image,16,d)
                            put(image,24,m)
                            expected=bytearray(image)
                            if valid:
                                put(expected,d-BASE,result,dw)
                            machine.mem_write(BASE,bytes(image))
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
                            if not valid:
                                require(not writes,(detail,'decline wrote'))
                                require((machine.reg_read(UC_X86_REG_MXCSR)==0x1f84) if arch=='amd64' else machine.reg_read(UC_ARM64_REG_FPSR)==2,(detail,'decline FP effects'))
                            checks+=1
        print('PASS',arch,'numeric spans/powers/all-normal-scales:',checks,'cases',flush=True)
