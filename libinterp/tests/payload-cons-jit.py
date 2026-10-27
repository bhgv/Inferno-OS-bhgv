#!/usr/bin/env python3
"""Execute raw and pointer list JITs with an ABI-hostile allocator and independent GC oracle."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_MODE_64, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
import unicorn.x86_const as xr
import unicorn.arm64_const as ar

ROOT=Path(__file__).resolve().parents[2]
BASE,CODE,STOP,HELPER=0x100000000,0x10000,0x11000,0x123456780000
TAIL,NODE,SOURCE,TYPE,PTR,MUT,PROP,STACK=(BASE+x for x in (0x3000,0x5000,0x6000,0x4000,0x4100,0x4020,0x4024,0x7800))
MASK=(1<<64)-1;HIGH=(1<<64)-0x1000
BASELINE=bytes((i*17+29)&255 for i in range(0x8000))


# Encode native cells without sharing machine instruction selection.
def put(image, offset, value, width=8):
    image[offset:offset+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')


# Retain all independent oracles under optimized Python.
def require(condition, detail):
    if not condition:raise RuntimeError(detail)


with tempfile.TemporaryDirectory(prefix='payload-cons-jit-') as temporary:
    td=Path(temporary)
    for arch in os.environ.get('PAYLOAD_ARCHES','amd64 arm64').split():
        source=td/(arch+'.c')
        source.write_text('''#include <stdint.h>
#include <string.h>
typedef uint32_t u32int;
typedef unsigned char uchar;
#define nil 0 // Native null identity for bounded emission helpers.
#include "isa.h"
#include HEADER


/* Emit real constructors with explicit independent offsets and live high addresses. */
int
emit(u32int *output, int cap, int wb, int pointer)
{
    LeafLayout a={0};
    a.wordbytes=wb; a.pc=0; a.s=8; a.d=16; a.m=24;
    a.listtail=0; a.listtype=8; a.listdata=16; a.typeref=0;
    a.heapref=24; a.heapcolor=32; a.refbytes=8; a.markcolor=3;
    a.consentry=(void*)0x123456780000ULL; a.ptrtype=(void*)0x100004100ULL;
    a.mutator=(void*)0x100004020ULL; a.nprop=(void*)0x100004024ULL;
    return leafemit(output,cap,&a,pointer?ICONSP:ICONSM);
}
'''.replace('HEADER','"'+arch+'-emit.h"'))
        binary=td/(arch+'.so')
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared','-fPIC','-I'+str(ROOT/'include'),'-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.emit.argtypes=[C.POINTER(C.c_uint32)]+[C.c_int]*3
        machine=Uc(UC_ARCH_X86,UC_MODE_64) if arch=='amd64' else Uc(UC_ARCH_ARM64,0)
        machine.mem_map(CODE,0x2000);machine.mem_map(HELPER,0x1000);machine.mem_map(BASE,0x8000);machine.mem_map(HIGH,0x1000)
        physical=(1<<52)-0x1000 if arch=='amd64' else HIGH
        if physical!=HIGH:machine.mem_map(physical,0x1000)
        machine.mem_write(HELPER,b'\xc3' if arch=='amd64' else bytes.fromhex('c0035fd6'))
        state={};writes=[]


        # Model only allocation/tail transfer, then destroy all ordinary caller-saved registers.
        def allocator(uc,address,size,context):
            if address!=HELPER:return
            state['calls']+=1;require(state['calls']==1,('duplicate call',state['detail']))
            width=uc.reg_read(xr.UC_X86_REG_RDI if arch=='amd64' else ar.UC_ARM64_REG_X0)
            slot=uc.reg_read(xr.UC_X86_REG_RSI if arch=='amd64' else ar.UC_ARM64_REG_X1)
            sp=uc.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)
            require(width==state['amount'] and sp%16==(8 if arch=='amd64' else 0),('call ABI',state['detail'],width,hex(sp)))
            wanted=state['dest'] if state['pointer'] else sp+(8 if arch=='amd64' else 0)
            require(slot==wanted,('allocator owner slot',state['detail'],hex(slot),hex(wanted)))
            if not state['helpervalid']:
                uc.emu_stop();return
            require(int.from_bytes(uc.mem_read(slot,8),'little')==state['tail'],('old tail',state['detail']))
            if not state['pointer']:
                require(slot!=state['dest'] and int.from_bytes(uc.mem_read(state['dest'],8),'little')==state['tail'],('early publication',state['detail']))
            image=state['expected'];old=state['tail']
            if state['change']:
                put(image,0x4020,11,4);put(image,0x4100,5,4)
            color=int.from_bytes(image[0x4020:0x4024],'little')
            put(image,0x5000-32,color,4);put(image,0x5000-24,1);put(image,0x5000-16,TYPE)
            put(image,0x5000,old);put(image,0x5008,0)
            put(image,0x4000,state['listrefs']+1,4)
            if old!=MASK and state['tailcolor']!=color:
                put(image,0x3000-32,3,4);put(image,0x4024,1,4)
            for start,length in ((0x5000-32,48),(0x4000,4),(0x3000-32,4),(0x4020,8),(0x4100,4)):
                uc.mem_write(BASE+start,bytes(image[start:start+length]))
            uc.mem_write(slot,NODE.to_bytes(8,'little'))
            if state['pointer']:put(image,slot-BASE,NODE)
            if arch=='amd64':
                for name in ('RCX','RDX','RSI','RDI','R8','R9','R10','R11'):uc.reg_write(getattr(xr,'UC_X86_REG_'+name),0xbad00000+len(name))
                uc.reg_write(xr.UC_X86_REG_RAX,NODE)
            else:
                for reg in range(1,19):uc.reg_write(getattr(ar,'UC_ARM64_REG_X'+str(reg)),0xbad00000+reg)
                uc.reg_write(ar.UC_ARM64_REG_X0,NODE)


        machine.hook_add(UC_HOOK_CODE,allocator)
        machine.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,address,size,value,context:writes.append((address,size)))
        checks=0
        for wb in (4,8):
            for pointer in (0,1):
                output=(C.c_uint32*130)(*[0xdeadbeef]*130);units=lib.emit(output,128,wb,pointer)
                require(0<units<=128,('size',arch,wb,pointer,units))
                require(lib.emit(None,128,wb,pointer)==units and list(output)[128:]==[0xdeadbeef]*2,'sizing/canary')
                for cap in range(units):
                    short=(C.c_uint32*130)(*[0xdeadbeef]*130)
                    require(lib.emit(short,cap,wb,pointer)==0 and list(short)==[0xdeadbeef]*130,'partial publication');checks+=1
                machine.mem_write(CODE,bytes(output)[:units*4]);machine.ctl_remove_cache(CODE,CODE+512)
                for length in ((8,) if pointer else (0,1,7,8,32,128,256,257,-1,1<<32)):
                    for profile in range(25 if pointer else 15):
                        for sa in range(8):
                            for da in range(8):
                                for alias in range(2 if pointer else 3):
                                    image=bytearray(BASELINE);top=bytearray(b'\x5a'*0x1000)
                                    image[0x4fe0:0x5200]=b'\xa5'*0x220
                                    src,dst,mid=BASE+0x100+sa,BASE+0x200+da,BASE+0x180+sa
                                    tail,sv=TAIL,SOURCE;href,prefs,lrefs=2,1,3
                                    tcolor,scolor,seed,change,failed=7,7,0,False,False
                                    if pointer:
                                        if alias:src=dst
                                        if profile==1:sv=MASK
                                        elif profile==2:sv=0
                                        elif profile==3:sv=8
                                        elif profile==4:sv=32
                                        elif profile==5:sv=SOURCE+1
                                        elif profile==6:href=0
                                        elif profile==7:href=MASK
                                        elif profile==8:prefs=0
                                        elif profile==9:prefs=-1
                                        elif profile==10:prefs=(1<<31)-1
                                        elif profile==11:prefs=(1<<31)-2
                                        elif profile==12:failed=True
                                        elif profile==13:tail=0
                                        elif profile==14:lrefs=0
                                        elif profile==15:tcolor=scolor=73
                                        elif profile==16:change=True
                                        elif profile==17:seed=1
                                        elif profile==18:href=MASK-1
                                        elif profile==19:sv=TAIL
                                        elif profile==20:sv=MASK-7;scolor=73
                                        elif profile==21:tail=MASK
                                        elif profile==22:dst=0
                                        elif profile==23:dst=MASK
                                        elif profile==24:dst=MASK-3
                                        put(image,0x100+sa,sv);put(image,0x200+da,tail)
                                        if alias:put(image,0x200+da,tail)
                                        sv=int.from_bytes(image[src-BASE:src-BASE+8],'little')
                                        amount=8
                                    else:
                                        src=SOURCE+16+sa
                                        if alias==1:src=dst
                                        if alias==2:src=mid
                                        if profile==1:src=0
                                        elif profile==2:src=MASK
                                        elif profile==3:src=MASK-7
                                        elif profile==4:dst=0
                                        elif profile==5:dst=MASK
                                        elif profile==6:dst=MASK-3
                                        elif profile==7:failed=True
                                        elif profile==8:href=0
                                        elif profile==9:lrefs=0
                                        elif profile==10:src=TAIL-32;tcolor=73
                                        elif profile==11:tcolor=73
                                        elif profile==12:change=True
                                        elif profile==13:tail=MASK
                                        elif profile==14:tail=0
                                        put(image,mid-BASE,length,wb);put(image,0x200+da,tail)
                                        amount=int.from_bytes(image[mid-BASE:mid-BASE+wb],'little',signed=True)
                                    put(image,0x3000-32,tcolor,4);put(image,0x3000-24,href if not pointer else 2);put(image,0x3000-16,TYPE)
                                    put(image,0x6000-32,scolor,4);put(image,0x6000-24,href);put(image,0x6000-16,TYPE)
                                    if pointer and profile==20:
                                        put(top,(MASK-7)-HIGH-32,scolor,4);put(top,(MASK-7)-HIGH-24,href);put(top,(MASK-7)-HIGH-16,TYPE)
                                    put(image,0x4000,lrefs,4);put(image,0x4100,prefs,4);put(image,0x4020,7,4);put(image,0x4024,seed,4)
                                    put(image,0,0x12345678);put(image,8,src);put(image,16,dst);put(image,24,mid)
                                    destvalid=dst not in (0,MASK) and dst+8<=MASK
                                    if pointer:
                                        srcrefs=2 if sv==TAIL else href
                                        preflight=(destvalid and 0<prefs<(1<<31)-1 and (sv==MASK or (sv>32 and sv%8==0 and srcrefs not in (0,MASK))))
                                    else:preflight=0<=amount<=256 and destvalid and (amount==0 or (src not in (0,MASK) and src+amount<=MASK))
                                    helpervalid=not failed and lrefs>0 and tail!=0 and (tail==MASK or (href!=0 if not pointer else True)) and destvalid
                                    valid=preflight and helpervalid
                                    expected,expectedtop=bytearray(image),bytearray(top)
                                    detail=(arch,wb,pointer,length,profile,sa,da,alias)
                                    state.clear();state.update(calls=0,amount=amount,pointer=pointer,dest=dst,tail=tail,tailcolor=tcolor,listrefs=lrefs,
                                                               change=change,helpervalid=helpervalid,expected=expected,detail=detail)
                                    entrysp=STACK+(8 if arch=='amd64' else 0)
                                    frame=(32 if pointer else 64) if arch=='amd64' else (16 if pointer else 48)
                                    if arch=='amd64':
                                        put(image,entrysp-BASE,STOP);put(expected,entrysp-BASE,STOP)
                                        machine.reg_write(xr.UC_X86_REG_RDI,BASE);machine.reg_write(xr.UC_X86_REG_RSP,entrysp);machine.reg_write(xr.UC_X86_REG_MXCSR,0x3f95)
                                        saved=[getattr(xr,'UC_X86_REG_'+v) for v in ('RBX','RBP','R12','R13','R14','R15')]
                                    else:
                                        machine.reg_write(ar.UC_ARM64_REG_X0,BASE);machine.reg_write(ar.UC_ARM64_REG_SP,entrysp);machine.reg_write(ar.UC_ARM64_REG_LR,STOP)
                                        machine.reg_write(ar.UC_ARM64_REG_FPCR,0x400000);machine.reg_write(ar.UC_ARM64_REG_FPSR,0x15)
                                        saved=[getattr(ar,'UC_ARM64_REG_X'+str(v)) for v in range(19,30)]
                                    for reg in saved:machine.reg_write(reg,0xabcdef000+reg)
                                    machine.mem_write(BASE,bytes(image));machine.mem_write(HIGH,bytes(top))
                                    if physical!=HIGH:machine.mem_write(physical,bytes(top))
                                    writes.clear()
                                    try:machine.emu_start(CODE,STOP,count=10000)
                                    except Exception as error:raise RuntimeError(('execution',detail)) from error
                                    require(state['calls']==int(preflight),('call count',detail,state['calls'],preflight))
                                    if valid:
                                        if pointer:
                                            before=int.from_bytes(expected[0x4100:0x4104],'little');put(expected,0x4100,before+1,4)
                                            put(expected,0x5008,PTR);put(expected,0x5010,sv)
                                            if sv!=MASK:
                                                area,at=(expectedtop,sv-HIGH) if sv>=HIGH else (expected,sv-BASE)
                                                before=int.from_bytes(area[at-24:at-16],'little');put(area,at-24,before+1)
                                                color=int.from_bytes(area[at-32:at-28],'little');mutator=int.from_bytes(expected[0x4020:0x4024],'little')
                                                if color!=mutator:put(area,at-32,3,4);put(expected,0x4024,1,4)
                                        else:
                                            if amount:
                                                copied=expectedtop[src-HIGH:src-HIGH+amount] if src>=HIGH else expected[src-BASE:src-BASE+amount]
                                                expected[0x5010:0x5010+amount]=copied
                                            put(expected,dst-BASE,NODE)
                                    if valid or not preflight:
                                        status=machine.reg_read(xr.UC_X86_REG_RAX if arch=='amd64' else ar.UC_ARM64_REG_X0)
                                        require(status==int(valid),('status',detail,status,valid))
                                        sp=machine.reg_read(xr.UC_X86_REG_RSP if arch=='amd64' else ar.UC_ARM64_REG_SP)
                                        require(sp==entrysp+(8 if arch=='amd64' else 0),('SP restoration',detail,hex(sp)))
                                        for reg in saved:require(machine.reg_read(reg)==0xabcdef000+reg,('saved register',detail,reg))
                                        if arch=='arm64':require(machine.reg_read(ar.UC_ARM64_REG_LR)==STOP,('LR restoration',detail))
                                    actual=bytearray(machine.mem_read(BASE,len(image)))
                                    if preflight:actual[entrysp-BASE-frame:entrysp-BASE]=expected[entrysp-BASE-frame:entrysp-BASE]
                                    require(actual==expected,('image',detail))
                                    require(bytes(machine.mem_read(physical,len(top)))==bytes(expectedtop),('high image',detail))
                                    allowed=set(range(entrysp-frame,entrysp)) if preflight else set()
                                    if valid:
                                        allowed.update(range(NODE+16,NODE+16+amount));allowed.update(range(dst,dst+8))
                                        if pointer:
                                            allowed.update(range(NODE+8,NODE+16));allowed.update(range(PTR,PTR+4));allowed.update(range(PROP,PROP+4))
                                            if sv!=MASK:
                                                allowed.update(range(sv-24,sv-16));allowed.update(range(sv-32,sv-28))
                                                if arch=='amd64' and sv>=HIGH:
                                                    low=sv&((1<<52)-1);allowed.update(range(low-24,low-16));allowed.update(range(low-32,low-28))
                                    require(all(address+i in allowed for address,count in writes for i in range(count)),('write span',detail,writes))
                                    if arch=='amd64':require(machine.reg_read(xr.UC_X86_REG_MXCSR)==0x3f95,'MXCSR')
                                    else:require(machine.reg_read(ar.UC_ARM64_REG_FPCR)==0x400000 and machine.reg_read(ar.UC_ARM64_REG_FPSR)==0x15,'FP state')
                                    checks+=1
                print('PASS',arch,'payload cons:',checks,'span/ABI/GC/alias/FP checks, WORD',wb*8,'pointer',pointer,'units',units,flush=True)
