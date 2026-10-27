#!/usr/bin/env python3
"""Execute byte-prefix leaves with independent array, span and whole-memory oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR
ROOT=Path(__file__).resolve().parents[2]
BASE=0x100000000;CODE=0x10000;STOP=0x11000;MASK=(1<<64)-1


# Serialize independently modelled fields at arbitrary byte offsets.
def put(image, offset, value, size=8):
    """Write independently modelled little-endian fields at arbitrary byte offsets."""
    image[offset:offset+size]=(value & ((1<<(size*8))-1)).to_bytes(size,'little')


# Keep the independent byte and status oracles active under optimized Python.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='slice-copy-jit-') as directory:
    td=Path(directory);checks=0
    for arch in os.environ.get('ARRAY_ARCHES', 'amd64 arm64').split():
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0 // Native null identity for sizing helpers.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Use native pointers and independent array/type layouts for actual staged code. */
int
emit(u32int *out,int cap,int wb,int field,int value)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.arraylen=0;a.arraytype=8;a.arraydata=24;a.typesize=24;a.typenp=28;
 switch(field){case 1:a.arraylen=value;break;case 2:a.arraytype=value;break;
 case 3:a.arraydata=value;break;case 4:a.typesize=value;break;case 5:a.typenp=value;break;}
 return leafemit(out,cap,&a,ISLICELA);
}
''')
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(Path(os.environ.get('SLICE_COPY_EMITTER_DIR',ROOT/'libinterp'))),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*4
        dis=Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[];reads=[]


        # Detect every guest store, including unchanged values on a declined path.
        def write(machine,access,address,size,value,context):
            """A declined or empty leaf must not hide even same-value guest stores."""
            writes.append((address,size))
        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        machine.hook_add(UC_HOOK_MEM_READ,lambda uc,access,address,size,value,context:reads.append((address,size)))
        for wb in (4,8):
            out=(C.c_uint32*128)(*[0xa5a5a5a5]*128);n=lib.emit(out,128,wb,0,0)
            require(0<n<=128 and lib.emit(None,128,wb,0,0)==n, 'slice-copy oracle')
            blob=bytes(out)[:n*4];require(sum(i.size for i in dis.disasm(blob,CODE))==n*4, 'slice-copy oracle')
            for cap in range(n):
                guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                require(lib.emit(guard,cap,wb,0,0)==0 and list(guard)==[0xa5a5a5a5]*128, 'slice-copy oracle');checks+=1
            for field in range(1,6):
                for value in (-1,1,2048):
                    guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    require(lib.emit(guard,128,wb,field,value)==0 and list(guard)==[0xa5a5a5a5]*128, 'slice-copy oracle');checks+=1
            machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+512)

            def run(length,sa,da,delta,fault=0,offset=0):
                """Compute prefix validity and memmove results without emitter arithmetic."""
                image=bytearray(bytes((i*37+length*11)&255 for i in range(256))*64)
                ma=(sa^da)&7;src=0x1200+sa;dest=src+delta+da if delta not in (999,1000) else 0x200+ma if delta==999 else 0x100+sa
                source=BASE+0x500;target=BASE+0x700;count=length;capacity=300;start=offset
                typeptr=BASE+0x900;width=1;np=0;sp=BASE+src;dp=BASE+dest
                if fault==1:source=0
                if fault==2:source=MASK
                if fault==3:target=0
                if fault==4:target=MASK
                if fault==5:count=-1
                if fault==6:count=257
                if fault==7:capacity=-1
                if fault==8:capacity=count-1
                if fault==9:start=-1
                if fault==10:start=1
                if fault==11:start=(1<<(wb*8-1))-1
                if fault==12:typeptr=0
                if fault==13:typeptr=MASK
                if fault==14:width=0
                if fault==15:width=2
                if fault==16:width=-1
                if fault==17:np=1
                if fault==18:np=-1
                if fault==19:sp=0
                if fault==20:sp=MASK
                if fault==21:dp=0
                if fault==22:dp=MASK
                if fault==23:sp=MASK-64
                if fault==24:dp=MASK-64
                if fault==25:sp=MASK-127
                if fault==26:capacity=(1<<(wb*8-1))-1
                if fault==27:source=MASK;target=MASK;typeptr=0;start=-1
                if fault==28:count=(1<<32)+1 if wb==8 else 0x7fffffff
                if fault==29:start=(1<<32) if wb==8 else 1
                if fault==30:capacity=(1<<32) if wb==8 else 0x7fffffff
                if fault==31:start=0x7ffffffe;capacity=0x7fffffff;dp=BASE+dest-start
                if fault==32:start=0x7fffffff;capacity=0x7fffffff+128 if wb==8 else 0x7fffffff;dp=BASE+dest-start
                if fault==33:start=0x7fffffff-127;capacity=0x80000000 if wb==8 else 0x7fffffff;dp=BASE+dest-start
                if fault==34:start=1;dp=MASK
                if fault==35:start=1;dp=MASK-64
                if fault==36:start=(1<<32) if wb==8 else 0x7fffffff;capacity=start;dp=MASK;typeptr=0
                put(image,0,BASE+0x3000);put(image,8,BASE+0x100+sa);put(image,16,BASE+0x300+da);put(image,24,BASE+0x200+ma)
                put(image,0x100+sa,source);put(image,0x300+da,target);put(image,0x200+ma,start,wb)
                put(image,0x500,count,wb);put(image,0x500+24,sp)
                put(image,0x700,capacity,wb);put(image,0x700+8,typeptr);put(image,0x700+24,dp)
                put(image,0x900+24,width,4);put(image,0x900+28,np,4)
                absent=source in (0,MASK)
                accepted=absent or (target not in (0,MASK) and 0<=count<=256 and capacity>=0 and 0<=start<=capacity and count<=capacity-start and
                    (count==0 or (start+count<=0x7fffffff and typeptr not in (0,MASK) and width==1 and np==0 and sp not in (0,MASK) and dp not in (0,MASK) and sp<=MASK-count and dp<=MASK-start-count)))
                expected=bytearray(image)
                if accepted and not absent and count:expected[dp+start-BASE:dp+start-BASE+count]=bytes(image[src:src+count])
                if arch=='amd64':
                    put(image,0x3f00,STOP);put(expected,0x3f00,STOP)
                    machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,BASE+0x3f00);machine.reg_write(UC_X86_REG_MXCSR,0x5fa5)
                else:
                    machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                    machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x95)
                machine.mem_write(BASE,bytes(image));writes.clear();reads.clear();machine.emu_start(CODE,STOP,count=2200)
                status=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                require(status==int(accepted), (arch,wb,length,sa,da,delta,fault,status,accepted))
                # Permit exact metadata cells, source blocks and the native return cell.
                allowedreads=set(range(BASE+8,BASE+32))
                allowedreads.update(range(BASE+0x3f00,BASE+0x3f08))
                for owner,offset,widthbytes in ((BASE+0x100+sa,0,8),(BASE+0x300+da,0,8),(BASE+0x200+ma,0,wb),(source,0,wb),(source,24,8),(target,0,wb),(target,8,8),(target,24,8),(typeptr,24,4),(typeptr,28,4)):
                    allowedreads.update(range(owner+offset,owner+offset+widthbytes))
                if accepted and not absent and count>0:
                    allowedreads.update(range(sp,sp+count))
                    allowedwrites=set(range(dp+start,dp+start+count))
                    require(all(address+i in allowedwrites for address,widthbytes in writes for i in range(widthbytes)),('write extent',arch,wb,length,delta,fault))
                    if count>=8 and sp!=dp+start:
                        require(any(widthbytes==8 for address,widthbytes in writes),('missing qword copy',arch,wb,length,delta,fault))
                require(all(address+i in allowedreads for address,widthbytes in reads if BASE<=address<BASE+0x4000 for i in range(widthbytes)),('read extent',arch,wb,length,delta,fault))

                require(bytes(machine.mem_read(BASE,0x4000))==expected, (arch,wb,length,delta,fault))
                if not accepted or absent or count==0 or (arch=='amd64' and sp==dp+start):require(not writes, 'slice-copy oracle')
                if arch=='amd64':require(machine.reg_read(UC_X86_REG_MXCSR)==0x5fa5, 'slice-copy oracle')
                else:require(machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x95, 'slice-copy oracle')

            for length in (0,1,2,7,8,15,16,31,32,63,64,127,128,129,255,256,257):
                for sa in range(8):
                    for da in range(8):
                        for delta in (-32,-1,0,1,7,8,31,63,127,128,256,999,1000):
                            for offset in (0,1,7,31):run(length,sa,da,delta,offset=offset);checks+=1
            for fault in range(1,37):
                for length in ((0,128) if fault in (23,24,25,35) else (0,1,128)):
                    for shift in range(8):run(length,shift,shift,256,fault);checks+=1
        print('PASS',arch,'slice prefixes, cumulative span/metadata/alias/FP checks',checks)
