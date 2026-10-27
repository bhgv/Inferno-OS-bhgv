#!/usr/bin/env python3
"""Execute FRAME/MFRAME preflight, carry and alias guards on AMD64 and ARM64."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc,UC_ARCH_X86,UC_ARCH_ARM64,UC_MODE_64,UC_HOOK_MEM_WRITE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_RDI,UC_X86_REG_RAX,UC_X86_REG_RSP,UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_LR,UC_ARM64_REG_FPCR,UC_ARM64_REG_FPSR

ROOT=Path(__file__).resolve().parents[2]  # Owning production emitters.
BASE=0x100000000  # Synthetic VM metadata above 4 GiB.
TOP=(1<<64)-0x10000  # High mapped prefixes for address-carry probes.
MASK=(1<<64)-1  # Native address modulus.
CODE=0x10000  # Guarded native body and stable return context.
STOP=CODE+0x1000  # Host caller return sentinel.


# All regression predicates remain active under Python -O.
def require(condition,detail):
    if not condition:raise RuntimeError(detail)


# Build bounded independent VM fields without silently wrapping oracle edits.
def put(image,offset,value,size=8):
    require(0<=offset<=len(image)-size,('oracle span',offset,size))
    image[offset:offset+size]=(value&((1<<(size*8))-1)).to_bytes(size,'little')


with tempfile.TemporaryDirectory(prefix='frame-allocation-jit-') as temporary:
    td=Path(temporary)
    for arch in os.environ.get('FRAME_ALLOCATION_ARCHES','amd64 arm64').split():
        require(arch in ('amd64','arm64'),arch)
        header=Path(os.environ.get('FRAME_ALLOCATION_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')
        source=td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output requests atomic sizing.
#include "isa.h"
#include "'''+str(header)+'''"


/* Independent shifted REG/module/frame/Type layouts and both Modl frame positions. */
int emit(u32int *out,int cap,int wb,int bias,int imported,int variant)
{
    LeafLayout a={0};
    a.wordbytes=wb;a.pc=bias;a.s=8+bias;a.d=16+bias;a.m=24+bias;
    a.module=40+bias;a.sp=48+bias;a.ts=56+bias;
    a.linkmodule=8+bias;a.moduletypes=32+bias;a.modulenty=40+bias;
    a.typesize=24+bias;a.typenp=28+bias;a.framesize=32+bias;
    a.frametype=16+bias;a.framemr=24+bias;
    a.linknlinks=48+bias;a.linklinks=56+bias;a.modlbytes=16;a.modlframe=variant*8;
    a.heapref=24;a.heapcolor=32;a.heaptype=16;
    return leafemit(out,cap,&a,imported ? IMFRAME : IFRAME);
}
''')
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*5
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x4000);machine.mem_map(BASE,0x10000);machine.mem_map(TOP,0x10000)
        highalias=TOP&((1<<52)-1) if arch=='amd64' else TOP
        if highalias!=TOP:machine.mem_map(highalias,0x10000)
        writes=[];faults=[]


        # A declined operation may not make even a same-value VM store.
        def write(machine,access,address,size,value,context):
            writes.append((address,size))


        # Parent-source probes report the precise address of an unsafe metadata read.
        def invalid(machine,access,address,size,value,context):
            faults.append((access,address,size))
            return False


        machine.hook_add(UC_HOOK_MEM_WRITE,write);machine.hook_add(UC_HOOK_MEM_INVALID,invalid)
        checks=0
        for imported in (0,1):
            if os.environ.get('FRAME_ALLOCATION_IMPORTED') and imported!=int(os.environ['FRAME_ALLOCATION_IMPORTED']):continue
            for variant in range(2 if imported else 1):
                for wb in (4,8):
                    for bias in (0,32):
                        out=(C.c_uint32*130)(*([0xa5a5a5a5]*130));n=lib.emit(out,128,wb,bias,imported,variant)
                        require(0<n<=128 and out[n]==0xa5a5a5a5,(arch,imported,variant,wb,bias,'sizing'))
                        for cap in [-1]+list(range(n)):
                            guard=(C.c_uint32*130)(*([0xa5a5a5a5]*130))
                            require(lib.emit(guard,cap,wb,bias,imported,variant)==0 and list(guard)==[0xa5a5a5a5]*130,'partial publication')
                        machine.mem_write(CODE,bytes(out)[:n*4]);machine.ctl_remove_cache(CODE,CODE+512)
                        r=dict(s=8+bias,d=16+bias,m=24+bias,module=40+bias,sp=48+bias,ts=56+bias)
                        for shift in range(8):
                            for alias in range(5):
                                image=bytearray(b'\xa5'*0x10000);high=bytearray(b'\x5a'*0x10000)
                                sourcecell=0x100+shift;middlecell=0x200+shift;destination=0x300+shift
                                link=0x500;module=0x800;table=0x1000;descriptor=0x1800;sp=0x4000+shift;ts=0x5000;size=64+bias
                                if alias==1:destination=sp+16+bias
                                if alias==2:sourcecell=sp+24+bias
                                if alias==3:destination=r['d']
                                if alias==4:sp=r['d']-(16+bias);ts=0x100
                                put(image,link+8+bias,BASE+module);put(image,link+48+bias,1,4)
                                put(image,link+56+bias+variant*8,BASE+descriptor)
                                put(image,module+32+bias,BASE+table);put(image,module+40+bias,1,4);put(image,table,BASE+descriptor)
                                put(image,descriptor+24+bias,size,4);put(image,descriptor+28+bias,0,4)
                                put(image,sourcecell,BASE+link if imported else 0,8 if imported else wb);put(image,middlecell,0,wb)
                                for key,value in [('s',BASE+sourcecell),('m',BASE+middlecell),('d',BASE+destination),('module',BASE+link),('sp',BASE+sp),('ts',BASE+ts)]:put(image,r[key],value)
                                expected=bytearray(image)
                                put(expected,r['sp'],BASE+sp+size);put(expected,sp+16+bias,BASE+descriptor);put(expected,sp+24+bias,0);put(expected,destination,BASE+sp)
                                cases=[('valid',image,high,expected,1)]
                                if alias==0:
                                    for field in (('s','m','d') if imported else ('s','d')):
                                        extent=8 if field=='d' or (imported and field=='s') else wb
                                        for bad in [0]+[MASK-k for k in range(extent)]:
                                            edited=bytearray(image);put(edited,r[field],bad);cases.append(('slot-'+field,edited,high,edited,0))
                                    pointers=[('Type',link+56+bias+variant*8 if imported else table,32+bias)]
                                    pointers += [('link',sourcecell,56+bias)] if imported else [('link',r['module'],16+bias),('module',link+8+bias,44+bias)]
                                    for label,cell,extent in pointers:
                                        for bad in [0]+[MASK-k for k in range(extent)]:
                                            edited=bytearray(image);put(edited,cell,bad);cases.append((label,edited,high,edited,0))
                                    for offset in range(1,8):
                                        edited=bytearray(image);put(edited,pointers[0][1],BASE+descriptor+offset);put(edited,descriptor+offset+24+bias,size,4);put(edited,descriptor+offset+28+bias,0,4);cases.append(('Type-align',edited,high,edited,0))
                                    for label,cell,bad in [('np',descriptor+28+bias,1),('np-negative',descriptor+28+bias,MASK),('size',descriptor+24+bias,31+bias)]:
                                        edited=bytearray(image);put(edited,cell,bad,4);cases.append((label,edited,high,edited,0))
                                    for distance in (size-1,size):
                                        edited=bytearray(image);put(edited,r['ts'],BASE+sp+distance);cases.append(('extent',edited,high,edited,0))
                                    if imported:
                                        for owner,index,count in [(MASK-bias-68,0,1),(MASK-0x1000,0x7ffffffe,0x7fffffff)]:
                                            edited=bytearray(image);upper=bytearray(high);put(edited,sourcecell,owner);put(edited,middlecell,index,wb)
                                            put(upper,owner+48+bias-TOP,count,4)
                                            address=owner+56+bias+variant*8
                                            if address<=MASK-8:put(upper,address-TOP,BASE+descriptor)
                                            cases.append(('import-wrap',edited,upper,edited,0))
                                    else:
                                        for owner,index,count in [(MASK-3,0,1),(MASK-0x1000,0x7ffffffe,0x7fffffff)]:
                                            edited=bytearray(image);put(edited,module+32+bias,owner);put(edited,module+40+bias,count,4);put(edited,sourcecell,index,wb)
                                            cases.append(('table-wrap',edited,high,edited,0))
                                for label,initial,upper,wanted,result in cases:
                                    if os.environ.get("FRAME_ALLOCATION_CASE") and label!=os.environ["FRAME_ALLOCATION_CASE"]:continue
                                    machine.mem_write(BASE,bytes(initial));machine.mem_write(TOP,bytes(upper))
                                    if highalias!=TOP:machine.mem_write(highalias,bytes(upper))
                                    writes.clear();faults.clear()
                                    if arch=='amd64':
                                        machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                        machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,0x1f84)
                                    else:
                                        machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                        machine.reg_write(UC_ARM64_REG_FPCR,0);machine.reg_write(UC_ARM64_REG_FPSR,2)
                                    detail=(arch,imported,variant,wb,bias,shift,alias,label)
                                    try:machine.emu_start(CODE,STOP,count=400)
                                    except Exception as error:raise RuntimeError((detail,faults)) from error
                                    actual=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                                    require(actual==result,(detail,'completion',actual))
                                    require(bytes(machine.mem_read(BASE,0x10000))==wanted and bytes(machine.mem_read(TOP,0x10000))==upper and bytes(machine.mem_read(highalias,0x10000))==upper,(detail,'image'))
                                    require(result or not writes,(detail,'decline wrote',writes))
                                    require(machine.reg_read(UC_X86_REG_MXCSR)==0x1f84 if arch=='amd64' else machine.reg_read(UC_ARM64_REG_FPCR)==0 and machine.reg_read(UC_ARM64_REG_FPSR)==2,(detail,'FP state'))
                                    checks+=1
        print('PASS',arch,'frame allocation span/carry/alias/FP:',checks,'cases',flush=True)
