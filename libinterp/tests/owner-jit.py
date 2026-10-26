#!/usr/bin/env python3
"""Execute guarded owner leaves on AMD64 and ARM64 with independent memory images."""
import ctypes as C
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_MODE_64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT=Path(__file__).resolve().parents[2]
BASE=0x100000000
CODE=0x10000
STOP=0x11000
MASK=(1<<64)-1


def put(image, offset, value, size=8):
    """Store a complete little-endian field without depending on host alignment."""
    image[offset:offset+size]=int(value & ((1<<(8*size))-1)).to_bytes(size,'little')


# Keep fixture predicates active under optimized Python.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='owner-jit-') as directory:
    td=Path(directory)
    checks=0
    for arch in os.environ.get('OWNER_ARCHES', 'amd64 arm64').split():
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;typedef unsigned char uchar;
#define nil 0
#include "isa.h"
#include '''+json.dumps(str(Path(os.environ.get('OWNER_EMITTER_DIR', ROOT/'libinterp'))/(arch+'-emit.h')))+'''
/* Supply an independent native-pointer layout and malformed-layout probes. */
int emit(u32int *out,int cap,int wb,int op,int bad){
 LeafLayout a={0};a.wordbytes=wb;a.pc=0;a.s=8;a.d=16;a.m=24;a.module=40;
 a.heapref=24;a.heapcolor=32;a.refbytes=8;a.mutator=(void*)0x100002000ULL;
 a.listdata=24;a.listtail=8;a.nprop=(void*)0x100002008ULL;a.markcolor=3;
 switch(bad){
 case 1:a.heapref=0;break;case 2:a.heapref=256;break;case 3:a.heapref=7;break;
 case 4:a.heapcolor=0;break;case 5:a.heapcolor=256;break;case 6:a.heapcolor=2;break;
 case 7:a.refbytes=4;break;case 8:a.mutator=nil;break;
 case 9:a.module=-1;break;case 10:a.module=2048;break;case 11:a.module=1;break;
 case 12:a.nprop=nil;break;case 13:a.markcolor=-1;break;case 14:a.markcolor=256;break;
 case 15:a.listdata=-1;a.listtail=-1;break;
 }
 return leafemit(out,cap,&a,((int[]){IMOVP,ISELF,IHEADP,ITAIL})[op]);
}
''')
        binary=td/(arch+'.so')
        cmd=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary))
        lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*4
        dis=Cs(CS_ARCH_X86,CS_MODE_64) if arch=='amd64' else Cs(CS_ARCH_ARM64,0)
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x3000);machine.mem_map(BASE,0x4000)
        writes=[]
        probing={"enabled":False,"allowed":set(),"previous":False}
        def record_write(machine, access, address, size, value, context):
            """Record actual guest stores so a declined leaf cannot hide same-byte writes."""
            writes.append((address,size))
        # Rejected owners may read only the fields needed to establish their failure.
        def readprobe(machine, access, address, size, value, context):
            if not probing['enabled']:
                return
            if probing['previous'] and address < BASE+0x188 and address+size > BASE+0x180:
                raise RuntimeError((arch, 'previous owner read before source validation', address, size))
            if BASE <= address and address+size <= BASE+0x200 or address == CODE+0x2800:
                return
            if (address, size) not in probing['allowed']:
                raise RuntimeError((arch, 'unexpected owner metadata read', address, size))


        machine.hook_add(UC_HOOK_MEM_WRITE,record_write)
        machine.hook_add(UC_HOOK_MEM_READ,readprobe)
        for wb in (4,8):
            for op in range(4):
                out=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                n=lib.emit(out,128,wb,op,0)
                require(0<n<=128 and lib.emit(None,128,wb,op,0)==n, 'owner image/GC/width/FP/capacity check')
                blob=bytes(out)[:n*4]
                require(sum(i.size for i in dis.disasm(blob,CODE))==n*4, 'owner image/GC/width/FP/capacity check')
                for bad in range(1,16 if op>=2 else 15):
                    guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    require(lib.emit(guard,128,wb,op,bad)==0 and list(guard)==[0xa5a5a5a5]*128, 'owner image/GC/width/FP/capacity check')
                    checks+=1
                for cap in (0,n-1):
                    guard=(C.c_uint32*128)(*[0xa5a5a5a5]*128)
                    require(lib.emit(guard,cap,wb,op,0)==0 and list(guard)==[0xa5a5a5a5]*128, 'owner image/GC/width/FP/capacity check')
                machine.mem_write(CODE,blob);machine.ctl_remove_cache(CODE,CODE+512)
                if op>=2 and not os.environ.get("OWNER_PROBE_ONLY"):
                    for badowner in (0,MASK):
                        for shift in range(8):
                            image=bytearray([0xa5])*0x4000
                            put(image,8,BASE+0x100+shift);put(image,16,BASE+0x180+shift)
                            put(image,0x100+shift,badowner);put(image,0x180+shift,MASK)
                            if arch=='amd64':
                                machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,BASE+0x3f00)
                                put(image,0x3f00,STOP)
                            else:
                                machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                            machine.mem_write(BASE,bytes(image));writes.clear();machine.emu_start(CODE,STOP,count=150)
                            require(machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)==0, 'owner image/GC/width/FP/capacity check')
                            require(bytes(machine.mem_read(BASE,0x4000))==image and not writes, 'owner image/GC/width/FP/capacity check')
                            checks+=1
                for shift in (() if os.environ.get("OWNER_PROBE_ONLY") else range(8)):
                    for value in (0,MASK,BASE+0x540):
                        for old in (0,MASK,BASE+0x540,BASE+0x740):
                            for refs in (0,1,2,(1<<32)-1,1<<32,MASK-1,MASK):
                                for color in (-1,0,1,2,3,0x7fffffff):
                                    for livecolor in (0,1,2):
                                        image=bytearray([0xa5])*0x4000
                                        source=BASE+0x100+shift;dest=BASE+0x180+shift
                                        put(image,0,BASE+0x3000);put(image,8,source);put(image,16,dest)
                                        put(image,0x100+shift,BASE+0x900 if op>=2 else value)
                                        put(image,0x180+shift,old);put(image,40,value)
                                        put(image,0x540-24,refs);put(image,0x540-32,color,4)
                                        put(image,0x900+(24 if op==2 else 8),value)
                                        put(image,0x2000,livecolor,4);put(image,0x2008,livecolor&1,4)
                                        expected=bytearray(image)
                                        accepted=(old==MASK or old==value) and value!=0 and (value!=MASK or op!=1) and (value==MASK or (refs not in (0,MASK)))
                                        if accepted:
                                            if value!=MASK and old==MASK:put(expected,0x540-24,refs+1)
                                            if value!=MASK and color!=livecolor:
                                                put(expected,0x540-32,3,4);put(expected,0x2008,1,4)
                                            put(expected,0x180+shift,value)
                                            if op>=2:put(expected,8,BASE+0x900+(24 if op==2 else 8))
                                        machine.mem_write(BASE,bytes(image))
                                        if arch=='amd64':
                                            machine.reg_write(UC_X86_REG_RDI,BASE);machine.reg_write(UC_X86_REG_RSP,BASE+0x3f00)
                                            machine.mem_write(BASE+0x3f00,struct.pack('<Q',STOP));put(expected,0x3f00,STOP)
                                            machine.reg_write(UC_X86_REG_MXCSR,0x5fa5)
                                        else:
                                            machine.reg_write(UC_ARM64_REG_X0,BASE);machine.reg_write(UC_ARM64_REG_LR,STOP)
                                            machine.reg_write(UC_ARM64_REG_FPCR,0x800000);machine.reg_write(UC_ARM64_REG_FPSR,0x95)
                                        writes.clear();machine.emu_start(CODE,STOP,count=150)
                                        actual=machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                                        if not accepted:require(not writes, 'owner image/GC/width/FP/capacity check')
                                        if accepted and old==value and value!=MASK:
                                            require(all(not (address<BASE+0x540-16 and address+size>BASE+0x540-24) for address,size in writes), 'owner image/GC/width/FP/capacity check')
                                        require(actual==int(accepted), (arch,wb,op,shift,value,old,refs,color,livecolor,actual))
                                        require(bytes(machine.mem_read(BASE,0x4000))==expected, (arch,wb,op,shift,value,old,refs,color,livecolor))
                                        if arch=='amd64':require(machine.reg_read(UC_X86_REG_MXCSR)==0x5fa5, 'owner image/GC/width/FP/capacity check')
                                        else:require(machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x95, 'owner image/GC/width/FP/capacity check')
                                        checks+=1
        preflight = 0
        for wb in (4, 8):
            for op in range(4):
                output = (C.c_uint32*128)()
                units = lib.emit(output, 128, wb, op, 0)
                require(units > 0, (arch, wb, op, 'preflight leaf unavailable'))
                machine.mem_write(CODE, bytes(output)[:units*4]); machine.ctl_remove_cache(CODE, CODE+512)
                selected = BASE+0x900+(24 if op == 2 else 8)
                invalid = [0, MASK]+[MASK-i for i in range(1, 8)]
                probes = [(kind, pointer) for kind in ('source-slot', 'destination-slot') for pointer in invalid
                          if op != 1 or kind != 'source-slot']
                probes += [('source-owner', pointer) for pointer in [0]+list(range(1, 33))+([MASK] if op == 1 else [])]
                if op >= 2:
                    probes += [('list-span', MASK-i) for i in range(1, (24 if op == 2 else 8)+8)]
                probes += [('source-priority', mode) for mode in range(4) if mode != 1 or op == 1]
                probes += [('previous-owner', pointer) for pointer in [0]+list(range(1, 33))+[BASE+0x1800]]
                if op == 1:
                    probes += [('unused-source', pointer) for pointer in invalid]
                family = os.environ.get('OWNER_PROBE_FAMILY')
                for kind, parameter in probes:
                    if family and kind != family:
                        continue
                    image=bytearray(b'\xa5'*0x4000)
                    value, node, old, refs = BASE+0x540, BASE+0x900, MASK, 1
                    source, destination = BASE+0x100, BASE+0x180
                    if kind == 'source-slot':source = parameter
                    if kind == 'destination-slot':destination = parameter
                    if kind == 'source-owner':value = parameter
                    if kind == 'list-span':node = parameter
                    if kind == 'source-priority':
                        destination = 1
                        if parameter < 2:value = 0 if parameter == 0 else MASK
                        else:refs = 0 if parameter == 2 else MASK
                    if kind == 'previous-owner':old = parameter
                    if kind == 'unused-source':source = parameter
                    put(image, 0, BASE+0x3000); put(image, 8, source); put(image, 16, destination); put(image, 40, value)
                    put(image, 0x100, node if op >= 2 else value); put(image, 0x180, old)
                    put(image, 0x540-24, refs); put(image, 0x540-32, 0, 4)
                    put(image, 0x900+(24 if op == 2 else 8), value)
                    put(image, 0x1800-24, 0)
                    put(image, 0x2000, 1, 4); put(image, 0x2008, 7, 4)
                    expected=bytearray(image)
                    accepted = kind == 'unused-source'
                    if accepted:
                        put(expected, 0x540-24, 2); put(expected, 0x540-32, 3, 4)
                        put(expected, 0x2008, 1, 4); put(expected, 0x180, value)
                    probing['allowed'] = set()
                    if kind in ('source-priority', 'previous-owner') and value == BASE+0x540:
                        probing['allowed'].add((BASE+0x540-24, 8))
                    if op >= 2 and kind not in ('source-slot', 'destination-slot', 'list-span'):
                        probing['allowed'].add((selected, 8))
                    probing['previous'] = kind == 'source-owner'
                    machine.mem_write(BASE, bytes(image))
                    if arch == 'amd64':
                        machine.reg_write(UC_X86_REG_RDI, BASE); machine.reg_write(UC_X86_REG_RSP, CODE+0x2800)
                        machine.mem_write(CODE+0x2800, STOP.to_bytes(8,'little'))
                        machine.reg_write(UC_X86_REG_MXCSR, 0x5fa5)
                    else:
                        machine.reg_write(UC_ARM64_REG_X0, BASE); machine.reg_write(UC_ARM64_REG_LR, STOP)
                        machine.reg_write(UC_ARM64_REG_FPCR, 0x800000); machine.reg_write(UC_ARM64_REG_FPSR, 0x95)
                    writes.clear(); probing['enabled'] = not accepted
                    machine.emu_start(CODE, STOP, count=1000)
                    probing['enabled'] = False
                    actual=machine.reg_read(UC_X86_REG_RAX if arch == 'amd64' else UC_ARM64_REG_X0)
                    require(actual == accepted and bytes(machine.mem_read(BASE,0x4000)) == expected,
                            (arch, wb, op, kind, parameter, actual, 'preflight state'))
                    require(accepted or not writes, (arch, op, kind, parameter, 'declined owner wrote memory'))
                    if arch == 'amd64':
                        require(machine.reg_read(UC_X86_REG_MXCSR)==0x5fa5, 'owner preflight changed MXCSR')
                    else:
                        require(machine.reg_read(UC_ARM64_REG_FPCR)==0x800000 and machine.reg_read(UC_ARM64_REG_FPSR)==0x95,
                                'owner preflight changed ARM64 FP state')
                    preflight += 1
        print('PASS', arch, 'owner preflight:', preflight, 'slot/span/lifetime/priority/GC/FP cases', flush=True)
        print('PASS',arch,'guarded owner leaves, cumulative memory/GC/width/alignment/FP checks',checks)
