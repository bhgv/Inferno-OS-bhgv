#!/usr/bin/env python3
"""Execute production SLICEA leaves against independent view/geometry images."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR
ROOT = Path(__file__).resolve().parents[2]
BASE, CODE, STOP = 0x100000000, 0x10000, 0x11000
MASK = (1 << 64)-1


# Encode independent native/VM images, preserving unaligned neighboring bytes.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1 << (width*8))-1)).to_bytes(width,'little')


# Decode signed WORD operands independently of the emitter's machine arithmetic.
def signed(image, offset, width):
    return int.from_bytes(image[offset:offset+width],'little',signed=True)


with tempfile.TemporaryDirectory(prefix='array-view-jit-') as directory:
    td = Path(directory)
    for arch in ('amd64','arm64'):
        source = td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output requests sizing only.
#include "isa.h"
#include "'''+arch+'''-emit.h"


/* Use independent layouts and malformed field variants for bounded publication. */
int
emit(u32int *out,int cap,int wb,int bad)
{
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;
 a.arraylen=0;a.arraytype=8;a.arrayroot=16;a.arraydata=24;
 a.typesize=24;a.typeref=0;a.typenp=28;a.heapref=24;a.refbytes=8;
 switch(bad){case 1:a.arraylen=1;break;case 2:a.arraytype=-1;break;
 case 3:a.arrayroot=1;break;case 4:a.arraydata=2048;break;case 5:a.typesize=1;break;
 case 6:a.typeref=2048;break;case 9:a.typenp=1;break;case 10:a.typenp=a.typeref;break;case 7:a.heapref=0;break;case 8:a.refbytes=4;break;}
 return leafemit(out,cap,&a,ISLICEA);
}
''')
        library = td/(arch+'.so')
        command = shlex.split(os.environ.get('CC','cc'))
        command += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(library)],check=True)
        lib = C.CDLL(str(library))
        lib.emit.argtypes = [C.POINTER(C.c_uint32)]+[C.c_int]*3
        dis = Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine = Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000)
        machine.mem_map(BASE,0x4000)
        writes = []


        # A decline must perform no stores, including writes of unchanged values.
        def write(machine, access, address, size, value, context):
            writes.append((address,size))


        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        checks = 0
        for wb in (4,8):
            out = (C.c_uint32*128)()
            n = lib.emit(out,128,wb,0)
            if not 0<n<=128 or lib.emit(None,128,wb,0)!=n:
                raise RuntimeError((arch,wb,'publication size',n))
            blob = bytes(out)[:n*4]
            if sum(i.size for i in dis.disasm(blob,CODE))!=n*4:
                raise RuntimeError('Invalid instruction boundaries')
            for cap in range(n):
                guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                if lib.emit(guard,cap,wb,0) or list(guard)!=[0xa5a5a5a5]*128:
                    raise RuntimeError('Partial output publication')
            for bad in range(1,11):
                guard = (C.c_uint32*128)(*[0xa5a5a5a5]*128)
                if lib.emit(guard,128,wb,bad) or list(guard)!=[0xa5a5a5a5]*128:
                    raise RuntimeError(('Malformed layout',bad))
            machine.mem_write(CODE,blob)
            machine.ctl_remove_cache(CODE,CODE+512)
            for profile in range(50):
                for width in (0,1,4,8):
                    for fa in range(8):
                        for alignment in range(8):
                            for alias in range(3):
                                image = bytearray(b'\xa5'*0x4000)
                                obj = 0x1800+fa
                                array, root, descriptor = BASE+obj, BASE+0x800, BASE+0xb00
                                length, first, last, refs, rootrefs, typerefs = 12,2,8,1,5,3
                                size, data, np = width,BASE+0x2400,0
                                if profile==1:first,last=0,12
                                if profile==2:first,last=3,3
                                if profile==3:first,last=0,0
                                if profile==4:first=-1
                                if profile==5:first,last=8,2
                                if profile==6:last=13
                                if profile==7:array=0
                                if profile==8:array=MASK
                                if profile==9:length=-1
                                if profile==10:refs=0
                                if profile==11:refs=2
                                if profile==12:refs=MASK
                                if profile==13:root=0
                                if profile==14:root=MASK
                                if profile==15:rootrefs=0
                                if profile==16:rootrefs=MASK
                                if profile==17:descriptor=0
                                if profile==18:descriptor=MASK
                                if profile==19:typerefs=0
                                if profile==20:typerefs=-1
                                if profile==21:typerefs=(1<<31)-1
                                if profile==22:size=-1
                                if profile==23:data=0
                                if profile==24:data=MASK
                                if profile==25:data=MASK-3
                                if profile==26:data=MASK-12*width
                                if profile==27:size=(1<<31)-1
                                if profile==28:length=last=(1<<(wb*8-1))-1;first=last-1
                                if profile==29:size=0;data=0
                                if profile==30:size=0;data=MASK
                                if profile==31:length=last=(1<<32)+2 if wb==8 else 12;first=last-1;size=0
                                if profile==32:root=BASE+obj
                                if profile==33:np=1
                                if profile==34:first=0;last=1;size=(1<<31)-1
                                if profile==35:first=0;last=2;size=(1<<31)-1
                                if profile==36:first=0;last=1;size=8;data=MASK-8
                                if profile==37:first=1;last=1;size=8;data=MASK-8
                                if profile==38:first=0;last=1;size=8;data=MASK-7
                                if profile==39:size=0;typerefs=(1<<31)-1;rootrefs=MASK
                                if profile>=40:root=MASK
                                if profile==40:np=1
                                if profile==41:np=-1
                                if profile==42:np=(1<<31)-1
                                if profile==43:typerefs=(1<<31)-1
                                if profile==44:rootrefs=0
                                if profile==45:first=0;last=1;size=8;data=MASK-8
                                if profile==46:length=last=(1<<(wb*8-1))-1;first=last-1;size=0
                                if profile==47:refs=2
                                if profile==48:refs=MASK
                                if profile==49:descriptor=MASK
                                put(image,obj,length,wb);put(image,obj+8,descriptor)
                                put(image,obj+16,root);put(image,obj+24,data)
                                put(image,obj-24,refs);put(image,0x800-24,rootrefs)
                                put(image,0xb00,typerefs,4);put(image,0xb18,size,4);put(image,0xb1c,np,4)
                                sa, ma, da = 0x100+alignment,0x200+alignment,0x300+alignment
                                put(image,sa,first,wb);put(image,ma,last,wb);put(image,da,array)
                                if alias==1:sa=obj
                                if alias==2:ma=obj
                                put(image,8,BASE+sa);put(image,24,BASE+ma);put(image,16,BASE+da)
                                first,last=signed(image,sa,wb),signed(image,ma,wb)
                                actualrootrefs=refs if root==BASE+obj else rootrefs
                                valid=array==BASE+obj and array%8==0 and refs==1 and (np==0 if root==MASK else root!=0 and actualrootrefs!=0) and descriptor not in (0,MASK) and typerefs>0 and size>=0 and 0<=first<=last<=length
                                if size>0:
                                    valid=valid and 0<last<=((1<<31)-1)//size and data not in (0,MASK) and data<=MASK-last*size
                                expected=bytearray(image)
                                expectedwrites=[]
                                if valid:
                                    put(expected,obj,last-first,wb)
                                    put(expected,obj+24,data+first*size)
                                    expectedwrites=[(BASE+obj,wb),(BASE+obj+24,8)]
                                machine.mem_write(BASE,bytes(image));writes.clear()
                                if arch=='amd64':
                                    machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                    machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,0x3f80)
                                else:
                                    machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                    machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x11)
                                machine.emu_start(CODE,STOP,count=200)
                                actual=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                                detail=(arch,wb,profile,width,fa,alignment,alias)
                                if actual!=valid or bytes(machine.mem_read(BASE,0x4000))!=expected or writes!=expectedwrites:
                                    raise RuntimeError((detail,actual,valid,writes,expectedwrites))
                                if arch=='amd64':
                                    if machine.reg_read(UC_X86_REG_MXCSR)!=0x3f80:raise RuntimeError('MXCSR')
                                elif machine.reg_read(UC_ARM64_REG_FPCR)!=0x800000 or machine.reg_read(UC_ARM64_REG_FPSR)!=0x11:
                                    raise RuntimeError('FP state')
                                checks+=1
        print('PASS',arch,'SLICEA view image/write/geometry/alias/FP cases:',checks,flush=True)
