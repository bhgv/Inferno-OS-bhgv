#!/usr/bin/env python3
"""Run NEWA/NEWAZ with independent live-type/owner/span/zero/publication oracles."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT=Path(__file__).resolve().parents[2]
BASE,CODE,STOP=0x100000000,0x10000,0x11000
MASK=(1<<64)-1
HIGH=(1<<64)-0x1000


# Serialize VM/native cells without sharing instruction-selection logic.
def put(image, offset, value, width=8):
    image[offset:offset+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')


# Retain every oracle under optimized Python.
def require(condition, detail):
    if not condition: raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='array-create-jit-') as temporary:
    td=Path(temporary)
    for arch in os.environ.get('ARRAY_ARCHES','arm64 amd64').split():
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null sentinel for private emitter helpers.
#include "isa.h"
#include HEADER


/* Stage actual array constructors using independent private metadata offsets. */
int
emit(u32int *out, int cap, int wb, int imported)
{
    LeafLayout a={0};
    a.wordbytes=wb; a.s=8; a.d=16; a.m=24; a.module=40;
    a.linkmodule=8; a.moduletypes=32; a.modulenty=40;
    a.typeref=0; a.typesize=24; a.typenp=28; a.typefree=8; a.typemark=16;
    a.heapref=24; a.heaptype=16; a.heapcolor=32; a.refbytes=8;
    a.arraylen=0; a.arraytype=8; a.arrayroot=16; a.arraydata=24; a.arrayidentity=(void*)0x100004500ULL;
    a.plainfree=(void*)0x1122334455667700ULL; a.plainmark=(void*)0x8877665544332200ULL;
    return leafemit(out,cap,&a,imported ? INEWAZ : INEWA);
}
'''.replace('HEADER','"'+str(Path(os.environ.get('EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h'))+'"'))
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x8000);machine.mem_map(HIGH,0x1000)
        physical=(1<<52)-0x1000 if arch=='amd64' else HIGH
        if physical!=HIGH:machine.mem_map(physical,0x1000)
        writes=[]
        machine.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,address,size,value,context:writes.append((address,size)))
        total=0
        for wb in (4,8):
            for mode in (0,1):
                imported=mode==1
                clear=bool(mode)
                output=(C.c_uint32*130)(*[0xdeadbeef]*130);count=lib.emit(output,128,wb,mode)
                require(0<count<=128,(arch,wb,imported,'size',count))
                require(list(output)[128:]==[0xdeadbeef]*2,'output canary')
                require(lib.emit(None,128,wb,mode)==count,'sizing differs')
                for cap in range(count):
                    short=(C.c_uint32*130)(*[0xdeadbeef]*130)
                    require(lib.emit(short,cap,wb,mode)==0 and all(v==0xdeadbeef for v in short),'partial publication')
                    total+=1
                machine.mem_write(CODE,bytes(output)[:count*4]);machine.ctl_remove_cache(CODE,CODE+512)
                for size in (0,1,7,8,32,128,256,257):
                    for elements in (0,1,3,33):
                        for profile in range(58):
                            for align in range(8):
                                for alias in range(3):
                                    image=bytearray(b'\xa5'*0x8000);top=bytearray(b'\x5a'*0x1000)
                                    link,module,table,descriptor,old=BASE+0x1000,BASE+0x2000,BASE+0x3000,BASE+0x4000,BASE+0x5000
                                    identity=BASE+0x4500
                                    refs,arrefs,oldrefs,pointers,n,index,counttypes=1,1,1,0,size,2,4
                                    modulevalue,tablevalue,typevalue,oldtype=module,table,descriptor,identity
                                    length,countvalue,elementtype,root,data=elements,elements,descriptor,MASK,BASE+0x6000
                                    if profile==1:link=0
                                    elif profile==2:link=MASK
                                    elif profile==3:link=MASK-1
                                    elif profile==4:modulevalue=0
                                    elif profile==5:modulevalue=MASK
                                    elif profile==6:modulevalue=MASK-1
                                    elif profile==7:tablevalue=0
                                    elif profile==8:tablevalue=MASK
                                    elif profile==9:tablevalue=MASK-7
                                    elif profile==10:counttypes=0
                                    elif profile==11:counttypes=-1
                                    elif profile==12:index=-1
                                    elif profile==13:index=4
                                    elif profile==14:index=(1<<(wb*8-1))-1
                                    elif profile==15:typevalue=0
                                    elif profile==16:typevalue=MASK
                                    elif profile==17:typevalue=MASK-1
                                    elif profile==18:typevalue=descriptor+4
                                    elif profile==19:refs=0
                                    elif profile==20:refs=-1
                                    elif profile==21:refs=(1<<31)-2
                                    elif profile==22:pointers=1
                                    elif profile==23:pointers=-1
                                    elif profile==24:refs=(1<<31)-1
                                    elif profile==25:arrefs=(1<<31)-1
                                    elif profile==26:old=0
                                    elif profile==27:old=MASK
                                    elif profile==28:old=8
                                    elif profile==29:old=MASK-31
                                    elif profile==30:oldrefs=0
                                    elif profile==31:oldrefs=2
                                    elif profile==32:oldrefs=MASK
                                    elif profile==33:oldtype=descriptor
                                    elif profile==34:tablevalue=table+0x200
                                    elif profile==35:pointers=1
                                    elif profile==36:pointers=-1
                                    elif profile==37:elementtype=descriptor+0x200
                                    elif profile==38:length+=1
                                    elif profile==39:root=BASE+0x7000
                                    elif profile==40:data=0
                                    elif profile==41:data=MASK
                                    elif profile==42:data=MASK-1
                                    elif profile==43:n=-1
                                    elif profile==44:n=(1<<31)-1
                                    elif profile==45:arrefs=0
                                    elif profile==46:refs=-1
                                    elif profile==47:arrefs=-1
                                    elif profile==48:typevalue=identity
                                    elif profile==49:countvalue=-1
                                    elif profile==50:countvalue=(1<<31)-1;length=countvalue
                                    elif profile==51:countvalue=1<<32;length=countvalue
                                    elif profile==52:length=1<<32
                                    elif profile==53:old+=4
                                    elif profile==54:data=old
                                    elif profile==55:data=old+24
                                    elif profile==56:data=old+31
                                    elif profile==57:data=old+32
                                    put(image,0x1008,modulevalue);put(image,0x2020,tablevalue);put(image,0x2028,counttypes,4)
                                    put(image,0x3010,typevalue);put(image,0x3210,typevalue)
                                    put(image,0x1070,BASE+0x3400);put(image,0x3410,descriptor+0x200)
                                    if profile==34:put(image,0x3010,descriptor+0x200)
                                    put(image,0x4000,refs,4);put(image,0x4018,n,4);put(image,0x401c,pointers,4)
                                    put(image,0x4500,arrefs,4);put(image,0x4518,0,4);put(image,0x451c,0,4)
                                    if profile==18:
                                        put(image,0x4004,refs,4);put(image,0x401c,n,4);put(image,0x4020,pointers,4)
                                    area,offset=(top,old-HIGH) if old==MASK-31 else (image,old-BASE if BASE<=old<BASE+0x7000 else 0x5000)
                                    put(area,offset-24,oldrefs);put(area,offset-16,oldtype)
                                    put(area,offset,length,wb);put(area,offset+8,elementtype);put(area,offset+16,root);put(area,offset+24,data)
                                    src,mid,dst=0x100+align,0x180+align,0x200+align
                                    if alias==1:src=0x6000
                                    if alias==2:dst=0x6008
                                    put(image,src,countvalue,wb);put(image,mid,index,wb);put(image,dst,old)
                                    actualcount=int.from_bytes(image[src:src+wb],'little',signed=True)
                                    actuallen=int.from_bytes(area[offset:offset+wb],'little',signed=True)
                                    put(image,8,BASE+src);put(image,24,BASE+mid);put(image,16,BASE+dst);put(image,40,link)
                                    amount=n*actualcount
                                    accepted=(link==BASE+0x1000 and modulevalue==module and tablevalue in (table,table+0x200) and
                                              0<=index<counttypes and index==2 and typevalue==descriptor and 0<refs<(1<<31)-1 and
                                              0<arrefs<(1<<31)-1 and 0<=n<=256 and pointers==0 and 0<=actualcount<1<<31 and
                                              0<=amount<=256 and old in (BASE+0x5000,) and oldrefs==1 and oldtype==identity and
                                              actuallen==actualcount and elementtype==descriptor and root==MASK and
                                              (amount==0 or (data>=old+32 and data!=MASK and data+amount<=MASK)))
                                    expected,expectedtop=bytearray(image),bytearray(top)
                                    if accepted and clear:
                                        if data>=HIGH:expectedtop[data-HIGH:data-HIGH+amount]=b'\0'*amount
                                        elif amount:expected[data-BASE:data-BASE+amount]=b'\0'*amount
                                        put(expected,dst,old)
                                    if arch=='amd64':
                                        put(image,0x7000,STOP);put(expected,0x7000,STOP)
                                        machine.reg_write(xr.UC_X86_REG_RDI,BASE);machine.reg_write(xr.UC_X86_REG_RSP,BASE+0x7000);machine.reg_write(xr.UC_X86_REG_MXCSR,0x3f95)
                                        saved=[getattr(xr,'UC_X86_REG_'+v) for v in ('RBX','RBP','R12','R13','R14','R15')]
                                    else:
                                        machine.reg_write(ar.UC_ARM64_REG_X0,BASE);machine.reg_write(ar.UC_ARM64_REG_LR,STOP)
                                        machine.reg_write(ar.UC_ARM64_REG_FPCR,0x400000);machine.reg_write(ar.UC_ARM64_REG_FPSR,0x15)
                                        saved=[getattr(ar,'UC_ARM64_REG_X'+str(v)) for v in range(19,30)]
                                    for reg in saved:machine.reg_write(reg,0xabcdef000+reg)
                                    machine.mem_write(BASE,bytes(image));machine.mem_write(HIGH,bytes(top))
                                    if physical!=HIGH:machine.mem_write(physical,bytes(top))
                                    writes.clear();detail=(arch,wb,mode,size,elements,profile,align,alias)
                                    try:machine.emu_start(CODE,STOP,count=10000)
                                    except Exception as error:raise RuntimeError(('execution',detail)) from error
                                    status=machine.reg_read(xr.UC_X86_REG_RAX if arch=='amd64' else ar.UC_ARM64_REG_X0)
                                    require(status==int(accepted),('status',detail,status,accepted))
                                    require(bytes(machine.mem_read(BASE,len(image)))==bytes(expected),('image',detail))
                                    require(bytes(machine.mem_read(physical,len(top)))==bytes(expectedtop),('high image',detail))
                                    require(bool(writes)==bool(accepted),('writes',detail))
                                    if accepted:
                                        allowed=set(range(data,data+amount)) if clear else set();allowed.update(range(BASE+dst,BASE+dst+8))
                                        if clear and arch=='amd64' and data>=HIGH:allowed.update(range(data&((1<<52)-1),(data&((1<<52)-1))+amount))
                                        require(all(address+i in allowed for address,width in writes for i in range(width)),('write span',detail,writes))
                                    for reg in saved:require(machine.reg_read(reg)==0xabcdef000+reg,('saved register',detail,reg))
                                    if arch=='amd64':require(machine.reg_read(xr.UC_X86_REG_MXCSR)==0x3f95,'MXCSR')
                                    else:require(machine.reg_read(ar.UC_ARM64_REG_FPCR)==0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR)==0x15,'FP state')
                                    total+=1
                print('PASS',arch,'NEWA/NEWAZ:',total,'checks;',count,'units, WORD',wb*8,'clear',clear,flush=True)
