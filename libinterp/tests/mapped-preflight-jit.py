#!/usr/bin/env python3
"""Exercise mapped copy slot/metadata/payload preflight with independent guest layouts."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP, UC_X86_REG_MXCSR
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR, UC_ARM64_REG_FPCR, UC_ARM64_REG_FPSR

ROOT = Path(__file__).resolve().parents[2]  # Owning source tree.
BASE = 0x100000000  # Guest registers, independent metadata and data above 4 GiB.
CODE = 0x10000  # Emitted instructions and borrowed return stack.
STOP = CODE+0x1000  # Return sentinel outside every staged leaf.
MASK = (1<<64)-1  # Native pointer modulus.
PATTERN = bytes((i*37+11)&255 for i in range(0x4000))  # Immutable full-image canary.


# Keep all oracles active even under Python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Write an owned image using modulo-width little-endian values at any alignment.
def put(image, offset, value, width=8):
    image[offset:offset+width] = (value & ((1<<(width*8))-1)).to_bytes(width,'little')


with tempfile.TemporaryDirectory(prefix='mapped-preflight-jit-') as temporary:
    td = Path(temporary)
    for arch in os.environ.get('MAPPED_PREFLIGHT_ARCHES','amd64 arm64').split():
        header = Path(os.environ.get('MAPPED_PREFLIGHT_EMITTER_DIR',ROOT/'libinterp'))/(arch+'-emit.h')
        source = td/'emit.c'
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // A null output requests sizing only.
#include "isa.h"
#include "'''+str(header)+'''"


/* Borrow output for cap units; retain the production fixed staging limit. */
int emit(u32int *out,int cap,int wb,int head)
{
    LeafLayout a={0};
    a.wordbytes=wb;
    a.pc=0;
    a.s=8;
    a.d=16;
    a.m=24;
    a.module=40;
    a.linkmodule=8;
    a.moduletypes=32;
    a.modulenty=40;
    a.typesize=24;
    a.typenp=28;
    a.listdata=24;
    a.heapref=24;
    a.heapcolor=32;
    a.refbytes=8;
    return leafemit(out,cap,&a,head ? IHEADMP : IMOVMP);
}
''')
        command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        library = td/(arch+'.so')
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(library)],check=True)
        lib = C.CDLL(str(library))
        lib.emit.argtypes = [C.POINTER(C.c_uint32),C.c_int,C.c_int,C.c_int]
        machine = Uc(UC_ARCH_X86 if arch=='amd64' else UC_ARCH_ARM64,UC_MODE_64 if arch=='amd64' else 0)
        machine.mem_map(CODE,0x3000)
        machine.mem_map(BASE,0x4000)
        # Unicorn x86 masks physical pages to 52 bits; virtual pointer arithmetic stays 64-bit.
        top = (1<<52)-4096 if arch=='amd64' else (1<<64)-4096
        machine.mem_map(top,4096)
        reads, writes = [], []


        # Observe exact metadata reads to enforce empty-record and error short circuits.
        def read_hook(machine,access,address,size,value,context):
            reads.append((address,size))


        # Detect even same-value writes on a native decline.
        def write_hook(machine,access,address,size,value,context):
            writes.append((address,size))


        machine.hook_add(UC_HOOK_MEM_READ,read_hook)
        machine.hook_add(UC_HOOK_MEM_WRITE,write_hook)
        checks = 0
        for wb in (4,8):
            for head in (0,1):
                if os.environ.get('MAPPED_PREFLIGHT_HEAD') and head!=int(os.environ['MAPPED_PREFLIGHT_HEAD']):
                    continue
                out = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                n = lib.emit(out,128,wb,head)
                require(0<n<=128 and out[n]==0xa5a5a5a5 and lib.emit(None,128,wb,head)==n,(arch,wb,head,'sizing'))
                for cap in range(n+1):
                    exact = (C.c_uint32*129)(*([0xa5a5a5a5]*129))
                    emitted = lib.emit(exact,cap,wb,head)
                    require(emitted==(n if cap==n else 0),(arch,wb,head,cap,'capacity'))
                    require(bytes(exact)[:n*4]==bytes(out)[:n*4] if emitted else list(exact)==[0xa5a5a5a5]*129,'partial publication')
                    require(exact[n]==0xa5a5a5a5,'output canary')
                machine.mem_write(CODE,bytes(out)[:n*4])
                machine.ctl_remove_cache(CODE,CODE+512)
                cases = [('index-slot',pointer,0) for pointer in [0,MASK]+[MASK-i for i in range(1,wb)]]
                if head:
                    cases += [('owner-slot',pointer,0) for pointer in [0,MASK]+[MASK-i for i in range(1,8)]]
                for kind,extent in (('link',16),('module',44),('table',16),('type',28)):
                    cases += [(kind,pointer,0) for pointer in [0,MASK]+[MASK-i for i in range(1,extent)]]
                cases += [('index',index,0) for index in (-1,2,1<<31,(1<<(wb*8-1))-1)]
                cases += [('size',size,0) for size in (-1,257,0x7fffffff)]
                cases += [('map',np,0) for np in (-1,1,0x7fffffff)]
                cases += [('unaligned-type',alignment,size) for alignment in range(1,8) for size in (0,1,8,256)]
                cases += [('top-size',size,0) for size in (0,1,256)]
                cases += [('top-cell',0,0)]
                if head:
                    cases += [('head-owner',owner,size) for owner in [1,8,16,24,32,MASK-7,MASK-15,MASK-23,MASK-31] for size in (0,1,256)]
                    cases += [('reference',ref,0) for ref in (0,1,2,MASK-1,MASK)]
                # Complete accepted/rejected payload domains, aliases and both overlap directions.
                cases += [('data',size,variant) for size in range(257) for variant in range(14)]
                selected = os.environ.get('MAPPED_PREFLIGHT_CASE')
                if selected:
                    require(selected in {case[0] for case in cases},'unknown MAPPED_PREFLIGHT_CASE')
                    cases = [case for case in cases if case[0]==selected]
                for kind,a,b in cases:
                    for alignment in range(8):
                        image = bytearray(PATTERN)
                        topimage = bytearray(b'\xa5'*4096)
                        s,m,d = BASE+0x100+alignment,BASE+0x200+alignment,BASE+0x2800
                        link,module,table,typ = BASE+0x400,BASE+0x500,BASE+0x600,BASE+0x700
                        owner,src = BASE+0x1000,BASE+0x1800+alignment
                        index,size,np,ref = 1,8,0,2
                        valid = True
                        if head: src=owner+24
                        if kind=='index': index=a;valid=False
                        if kind=='size': size=a;valid=False
                        if kind=='map': np=a;valid=False
                        if kind=='unaligned-type': typ+=a;size=b;valid=not size
                        if kind=='reference': ref=a;valid=ref not in (0,MASK)
                        if kind=='head-owner':
                            owner=a;src=owner+24;size=b
                            valid=not size or (owner>32 and owner%8==0 and src<=MASK-size)
                        if kind=='data':
                            size=a
                            if b==0: src=0
                            elif b==1: src=MASK
                            elif b==2: src=MASK-max(size-1,0)
                            elif b==3: d=0
                            elif b==4: d=MASK
                            elif b==5: d=MASK-max(size-1,0)
                            elif b==6: d=src+1
                            elif b==7: d=src-1
                            elif b==8: d=src
                            elif b==9: d=m
                            elif b==10: d=s
                            elif b==11: d=table+8
                            elif b==12: src=MASK-size
                            else: d=MASK-size
                            if head and (b<3 or b==12):
                                owner=src-24 if src>=24 else 1
                                valid=not size or (b==12 and owner>32 and owner%8==0 and src<=MASK-size)
                            else:
                                valid=not size or (src not in (0,MASK) and d not in (0,MASK) and src<=MASK-size and d<=MASK-size)
                        # Base identities are installed before selectively poisoning one link.
                        put(image,link-BASE+8,module)
                        put(image,module-BASE+32,table)
                        put(image,module-BASE+40,2,4)
                        put(image,table-BASE+8,typ)
                        put(image,typ-BASE+24,size,4)
                        put(image,typ-BASE+28,np,4)
                        put(image,BASE+0x1000-24-BASE,ref)
                        put(image,s-BASE,owner)
                        put(image,m-BASE,index,wb)
                        if kind=='link': link=a;valid=False
                        if kind=='module': put(image,link-BASE+8,a);valid=False
                        if kind=='table': put(image,module-BASE+32,a);valid=False
                        if kind=='type': put(image,table-BASE+8,a);valid=False
                        if kind=='index-slot': m=a;link=1;valid=False
                        if kind=='owner-slot': s=a;m=1;link=1;valid=False
                        if kind=='top-size':
                            size=a;typ=MASK-28
                            put(image,table-BASE+8,typ)
                            put(topimage,4091,size,4)
                            valid=not size
                        if kind=='top-cell':
                            # The complete pointer cell ends at MAX-1; no following bytes exist.
                            put(image,module-BASE+32,MASK-16)
                            put(topimage,4087,BASE+0x700)
                        put(image,0,BASE+0x3900)
                        put(image,8,s if head else src)
                        put(image,16,d)
                        put(image,24,m)
                        put(image,40,link)
                        expected = bytearray(image)
                        expectedtop = bytearray(topimage)
                        if valid and size:
                            payload = topimage[src-(MASK-4095):src-(MASK-4095)+size] if src>=MASK-4095 else image[src-BASE:src-BASE+size]
                            if d>=MASK-4095: expectedtop[d-(MASK-4095):d-(MASK-4095)+size] = payload
                            else: expected[d-BASE:d-BASE+size] = payload
                            if head: put(expected,8,src)
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
                        detail = (arch,wb,head,kind,a,b,alignment)
                        try:
                            machine.emu_start(CODE,STOP,count=3000)
                        except Exception as error:
                            raise RuntimeError((detail,reads[-8:])) from error
                        result = machine.reg_read(UC_X86_REG_RAX if arch=='amd64' else UC_ARM64_REG_X0)
                        require(result==int(valid),(detail,'result',result,valid))
                        require(bytes(machine.mem_read(BASE,0x4000))==expected,(detail,'memory/register image'))
                        require(bytes(machine.mem_read(top,4096))==expectedtop,(detail,'top metadata changed'))
                        require((machine.reg_read(UC_X86_REG_MXCSR)==0x1f84) if arch=='amd64' else (machine.reg_read(UC_ARM64_REG_FPCR)==0 and machine.reg_read(UC_ARM64_REG_FPSR)==2),(detail,'FP state'))
                        if not valid: require(not writes,(detail,'decline stores'))
                        if kind=='top-size': require([r for r in reads if top<=r[0]<top+4096]==[(top+4091,4)],(detail,'non-lazy metadata'))
                        if kind in ('index-slot','owner-slot'): require(not any(BASE+0x400<=r[0]<BASE+0x800 for r in reads),(detail,'premature metadata'))
                        checks+=1
        print('PASS',arch,'mapped slot/metadata/payload preflight:',checks,'cases',flush=True)
