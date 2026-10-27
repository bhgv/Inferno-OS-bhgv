#!/usr/bin/env python3
"""Execute CALL/MCALL/RET numeric-span guards on both priority architectures."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc,UC_ARCH_X86,UC_ARCH_ARM64,UC_MODE_64,UC_HOOK_MEM_WRITE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_RDI,UC_X86_REG_RAX,UC_X86_REG_RSP,UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0,UC_ARM64_REG_LR,UC_ARM64_REG_FPCR,UC_ARM64_REG_FPSR

ROOT=Path(__file__).resolve().parents[2]  # Production interpreter and emitter tree.
BASE=0x100000000  # REG, operand cells and live synthetic VM objects above 4 GiB.
TOP=(1<<64)-0x10000  # Mapped high-address metadata for entry-addition overflow.
MASK=(1<<64)-1  # Native address modulus.
CODE=0x10000  # Native leaf and original host return context.
STOP=CODE+0x1000  # Stable host return sentinel.


# Keep all predicates active when the driver is run with Python -O.
def require(condition,detail):
    if not condition: raise RuntimeError(detail)


# Write a bounded independent little-endian field, never a wrapped synthetic edit.
def put(image,offset,value,size=8):
    require(0<=offset<=len(image)-size,('oracle span',offset,size))
    image[offset:offset+size]=(value&((1<<(size*8))-1)).to_bytes(size,'little')


with tempfile.TemporaryDirectory(prefix='control-transfer-jit-') as temporary:
    td=Path(temporary)
    for arch in os.environ.get('CONTROL_ARCHES','amd64 arm64').split():
        require(arch in ('amd64','arm64'),arch)
        header=Path(os.environ.get('CONTROL_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')
        source=td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Null output requests bounded sizing.
#include "isa.h"
#include "'''+str(header)+'''"


/* Shift independent REG, frame, Type and module offsets without copying host structs. */
int emit(u32int *out,int cap,int wb,int bias,int operation)
{
    LeafLayout a={0};
    int ops[]={ICALL,IMCALL,IRET};
    a.wordbytes=wb;a.pc=bias;a.s=8+bias;a.d=16+bias;a.m=24+bias;
    a.fp=32+bias;a.module=40+bias;a.sp=48+bias;a.regmp=64+bias;a.regtemp=72+bias;
    a.framelr=bias;a.framefp=8+bias;a.frametype=16+bias;a.framemr=24+bias;
    a.linkmp=bias;a.linkcompiled=16+bias;a.linkprog=24+bias;
    a.linknlinks=48+bias;a.linklinks=56+bias;a.modlbytes=16;a.modlpc=0;
    a.typenp=28+bias;a.heapref=24;a.refbytes=8;
    return leafemit(out,cap,&a,ops[operation]);
}
''')
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*4
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x4000);machine.mem_map(BASE,0x10000);machine.mem_map(TOP,0x10000)
        # Unicorn x86 translates canonical high addresses to its 52-bit physical bus.
        highalias=TOP&((1<<52)-1) if arch=='amd64' else TOP
        if highalias!=TOP:machine.mem_map(highalias,0x10000)
        writes=[]


        # A declined control transfer must not perform even a same-value VM store.
        def write(machine,access,address,size,value,context):
            writes.append((address,size))


        faults=[]


        # Preserve the actual failing address when a parent emitter touches a bad span.
        def invalid(machine,access,address,size,value,context):
            faults.append((access,address,size))
            return False


        machine.hook_add(UC_HOOK_MEM_INVALID,invalid)
        machine.hook_add(UC_HOOK_MEM_WRITE,write)
        checks=0
        for wb in (4,8):
            for bias in (0,32):
                r=dict(pc=bias,s=8+bias,d=16+bias,m=24+bias,fp=32+bias,module=40+bias,sp=48+bias,mp=64+bias,t=72+bias)
                for op in range(3):
                    if os.environ.get("CONTROL_GUEST_OP") and op!=int(os.environ["CONTROL_GUEST_OP"]):continue
                    out=(C.c_uint32*130)(*([0xa5a5a5a5]*130))
                    n=lib.emit(out,128,wb,bias,op)
                    require(0<n<=128 and out[n]==0xa5a5a5a5,(arch,wb,bias,op,'sizing'))
                    for cap in [-1]+list(range(n)):
                        canary=(C.c_uint32*130)(*([0xa5a5a5a5]*130))
                        require(lib.emit(canary,cap,wb,bias,op)==0 and list(canary)==[0xa5a5a5a5]*130,'partial emission')
                    machine.mem_write(CODE,bytes(out)[:n*4]);machine.ctl_remove_cache(CODE,CODE+512)
                    for shift in range(8):
                        sourcecell=0x100+shift;middlecell=0x200+shift;destcell=0x300+shift
                        callee=0x2000;caller=0x3000;frame=0x4000+shift;descriptor=0x1000+shift
                        image=bytearray(b'\xa5'*0x10000);high=bytearray(b'\x5a'*0x10000)
                        for owner,mp in ((callee,BASE+0x8200),(caller,BASE+0x8000)):
                            put(image,owner-24,3);put(image,owner+bias,mp)
                            put(image,owner+16+bias,0,4);put(image,owner+24+bias,BASE+0x6000)
                            put(image,owner+48+bias,1,4);put(image,owner+56+bias,BASE+0x6100)
                        put(image,descriptor+28+bias,0,4)
                        put(image,frame+bias,BASE+0x6100);put(image,frame+8+bias,BASE+0x7000)
                        put(image,frame+16+bias,BASE+descriptor);put(image,frame+24+bias,BASE+caller)
                        put(image,sourcecell,BASE+frame);put(image,middlecell,0,wb)
                        put(image,destcell,BASE+callee if op==1 else BASE+0x6100)
                        for key,value in [('pc',BASE+0x6000),('s',BASE+sourcecell),('m',BASE+middlecell),('d',BASE+destcell),('fp',BASE+frame if op==2 else BASE+0x7000),('module',BASE+callee if op==2 else BASE+caller),('sp',BASE+0x9000),('mp',BASE+0x8800)]:put(image,r[key],value)
                        put(image,r['t'],17,wb)
                        expected=bytearray(image)
                        if op==2:
                            for key,value in [('fp',BASE+0x7000),('pc',BASE+0x6100),('sp',BASE+frame),('module',BASE+caller),('mp',BASE+0x8000)]:put(expected,r[key],value)
                            put(expected,callee-24,2)
                        else:
                            put(expected,frame+bias,BASE+0x6000);put(expected,frame+8+bias,BASE+0x7000)
                            put(expected,r['fp'],BASE+frame);put(expected,r['pc'],BASE+0x6100)
                            if op==1:
                                put(expected,frame+24+bias,BASE+caller);put(expected,r['module'],BASE+callee)
                                put(expected,r['mp'],BASE+0x8200);put(expected,r['t'],1,wb);put(expected,callee-24,4)
                        cases=[('valid',image,high,expected,1)]
                        for field in (('s','d') if op==0 else ('s','m','d') if op==1 else ()):
                            extent=wb if field=='m' else 8
                            for bad in [0]+[MASK-k for k in range(extent)]:
                                edited=bytearray(image);put(edited,r[field],bad)
                                cases.append(('slot-'+field,edited,high,edited,0))
                        extent=bias+(16 if op==0 else 32)
                        for bad in [0]+[MASK-k for k in range(extent)]:
                            edited=bytearray(image);put(edited,r['fp'] if op==2 else sourcecell,bad)
                            cases.append(('frame',edited,high,edited,0))
                        if op in (1,2):
                            fields=[('current',r['module'],20+bias)]
                            if op==1:fields += [('callee',destcell,56+bias)]
                            else:fields += [('saved',frame+24+bias,20+bias),('type',frame+16+bias,32+bias)]
                            for name,cell,extent in fields:
                                for bad in [MASK-k for k in range(extent)]:
                                    edited=bytearray(image);put(edited,cell,bad)
                                    cases.append((name,edited,high,edited,0))
                            cell=destcell if op==1 else r['module']
                            for bad in range(1,25):
                                edited=bytearray(image);put(edited,cell,bad)
                                cases.append(('heap-origin',edited,high,edited,0))
                        if op==1:
                            for owner,index,count in [(MASK-bias-60,0,1),(MASK-0x1000,0x7ffffffe,0x7fffffff)]:
                                edited=bytearray(image);top=bytearray(high)
                                put(edited,destcell,owner);put(edited,middlecell,index,wb)
                                for offset,value,size in [(-24,3,8),(bias,BASE+0x8200,8),(bias+16,0,4),(bias+24,BASE+0x6000,8),(bias+48,count,4)]:put(top,owner+offset-TOP,value,size)
                                cases.append(('entry-wrap-'+str(index),edited,top,edited,0))
                        if op==2:
                            for label,field,bad in [('root',frame+8+bias,0),('bad-pc',frame+bias,0),('bad-saved',frame+24+bias,MASK),('nil-current',r['module'],0),('H-current',r['module'],MASK)]:
                                edited=bytearray(image);put(edited,field,bad)
                                put(edited,frame+16+bias,BASE+0x20000)
                                cases.append(('early-'+label,edited,high,edited,0))
                        for label,initial,upper,wanted,result in cases:
                            machine.mem_write(BASE,bytes(initial));machine.mem_write(TOP,bytes(upper));
                            if highalias!=TOP:machine.mem_write(highalias,bytes(upper))
                            writes.clear();faults.clear()
                            if arch=='amd64':
                                machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,CODE+0x2800)
                                machine.mem_write(CODE+0x2800,STOP.to_bytes(8,'little'));machine.reg_write(UC_X86_REG_MXCSR,0x1f84)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                machine.reg_write(UC_ARM64_REG_FPCR,0);machine.reg_write(UC_ARM64_REG_FPSR,2)
                            detail=(arch,wb,bias,op,shift,label)
                            try:machine.emu_start(CODE,STOP,count=300)
                            except Exception as error:raise RuntimeError((detail,faults)) from error
                            actual=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                            require(actual==result,(detail,'completion',actual))
                            require(bytes(machine.mem_read(BASE,0x10000))==wanted and bytes(machine.mem_read(TOP,0x10000))==upper,(detail,'image'))
                            require(bytes(machine.mem_read(highalias,0x10000))==upper,(detail,'physical alias'))
                            require(result or not writes,(detail,'decline wrote',writes))
                            require(machine.reg_read(UC_X86_REG_MXCSR)==0x1f84 if arch=='amd64' else machine.reg_read(UC_ARM64_REG_FPCR)==0 and machine.reg_read(UC_ARM64_REG_FPSR)==2,(detail,'FP state'))
                            checks+=1
        print('PASS',arch,'control span/overflow/image/FP:',checks,'cases',flush=True)
