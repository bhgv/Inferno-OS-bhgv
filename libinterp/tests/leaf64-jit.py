#!/usr/bin/env python3
"""Execute actual AMD64, AArch64 and RV64 leaf instructions with independent integer oracles."""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_X86, CS_ARCH_ARM64, CS_ARCH_RISCV, CS_MODE_64, CS_MODE_RISCV64
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM64, UC_ARCH_RISCV, UC_MODE_64, UC_MODE_RISCV64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RAX, UC_X86_REG_RSP
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_LR
from unicorn.riscv_const import UC_RISCV_REG_A0, UC_RISCV_REG_RA

root = Path(__file__).resolve().parents[2]
ops = '''IINDX IMULX IDIVX ICVTRF ICVTFR ICVTWF ICVTLF ICVTXX IBEQF IBNEF IBLTF IBLEF IBGTF IBGEF IHEADM IEXPW IEXPL IINDC ILENL IHEADB IHEADW IHEADL IHEADF ILENC IMOVM INEGF ILENA IINDB IINDW IINDL IINDF IRUNT IMOVB IMOVW IMOVL IMOVF ICVTBW ICVTWB ICVTWS ICVTSW ILEA IJMP
IADDB IADDW ISUBB ISUBW IMULB IMULW IANDB IANDW IORB IORW IXORB IXORW
IADDL ISUBL IMULL IANDL IORL IXORL IBEQB IBEQW IBNEB IBNEW IBLTB IBLTW
IBLEB IBLEW IBGTB IBGTW IBGEB IBGEW ICVTWL ICVTLW ISHLB ISHLW ISHLL ISHRB ISHRW ISHRL ILSRW ILSRL IDIVB IDIVW IDIVL IMODB IMODW IMODL IBEQL IBNEL IBLTL IBLEL IBGTL IBGEL IHEADP IHEADMP ITAIL IMOVP IFRAME ICALL IRET IADDF'''.split()
class Layout(C.Structure):
    _fields_ = [(x, C.c_int) for x in ('wordbytes', 'pc', 's', 'd', 'm', 'arraylen', 'arraydata', 'stringlen', 'stringdata', 'listdata', 'listtail', 'fp', 'fixedtmp', 'arraytype', 'typesize','framelr','framefp')]
class RvLayout(C.Structure):
    _fields_ = [(x, C.c_int) for x in ('xlen', 'wordbytes', 'hasm', 'pc', 's', 'd', 'm', 'arraylen', 'arraydata', 'stringlen', 'stringdata', 'listdata', 'listtail', 'fp', 'fixedtmp', 'arraytype', 'typesize','hasdiv','framelr','framefp')]


def signed(value, bits):
    """Interpret a bit vector independently of the emitter's chosen machine instructions."""
    value &= (1 << bits)-1
    return value-(1 << bits) if value >> (bits-1) else value


case=os.environ.get('LEAF64_CASE','')
selected_arch=os.environ.get('LEAF64_ARCH','')
if case not in ('','guard','array','control','copy','string','head','listlen','character','power','headcopy','realbranch','scalar','fixed','integerreal','resize','framefixed','frameabi','descriptor','call'):raise ValueError(case)
if selected_arch not in ('','amd64','arm64','riscv64'):raise ValueError(selected_arch)
checks = 0
rng = random.Random(0x6c696d626f)
with tempfile.TemporaryDirectory(prefix='leaf64-jit-') as directory:
    td = Path(directory)
    for arch in ('amd64', 'arm64', 'riscv64'):
        if selected_arch and arch!=selected_arch:continue
        rv = arch == 'riscv64'
        source = td/(arch+'.c')
        source.write_text('#include <stdint.h>\n#include <string.h>\n'
                          'typedef uint32_t u32int; typedef unsigned char uchar;\n#define nil 0\n'
                          '#include "isa.h"\n#include "'+(os.environ.get('LEAF64_RISCV_EMITTER','riscv-emit.h') if rv else arch+'-emit.h')+'"\n'
                          'int emit(u32int *p,int n,'+('RvLayout' if rv else 'LeafLayout')+' *l,int op){return '+('rvemit' if rv else 'leafemit')+'(p,n,l,op);}\n'
                          'int opid(int i){static int ops[]={'+','.join(ops)+'};return ops[i];}\n')
        binary = td/(arch+'.so')
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd+['-shared','-fPIC','-I'+str(root/'include'),'-I'+str(root/'libinterp'),str(source),'-o',str(binary)],check=True)
        lib = C.CDLL(str(binary))
        ids = {name:lib.opid(i) for i,name in enumerate(ops)}
        for wb in (4,8):
            layout = RvLayout(64,wb,1,0,8,16,24) if rv else Layout(wb,0,8,16,24)
            if rv:layout.hasdiv=1
            layout.framelr=0;layout.framefp=8
            layout.arraylen=8;layout.arraydata=32;layout.stringlen=12;layout.stringdata=32;layout.listdata=24;layout.listtail=8;layout.fp=32;layout.fixedtmp=28;layout.arraytype=16;layout.typesize=24
            dis = Cs(CS_ARCH_X86,CS_MODE_64) if arch == 'amd64' else Cs(CS_ARCH_ARM64,0) if arch == 'arm64' else Cs(CS_ARCH_RISCV,CS_MODE_RISCV64)
            blobs = {}
            for name in ops:
                out = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                n = lib.emit(out,64,C.byref(layout),ids[name])
                blobs[name] = bytes(out)[:n*4]
                assert lib.emit(None,64,C.byref(layout),ids[name]) == n
                if n:
                    assert sum(i.size for i in dis.disasm(blobs[name],0x10000)) == n*4
                    guard = (C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,n-1,C.byref(layout),ids[name]) == 0
                    assert list(guard)[n-1:] == [0xa5a5a5a5]*(65-n)
                checks += 1
            # Reject malformed frame descriptions before exposing any emitted output.
            for name in ('IMULX','IDIVX'):
                invalid=[('fp',-1),('fp',1),('fp',2048),('fixedtmp',-1),('fixedtmp',2048)]
                if rv:invalid.append(('fixedtmp',1))
                for field,value in invalid:
                    malformed=type(layout).from_buffer_copy(layout)
                    setattr(malformed,field,value)
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids[name])==0
                    assert list(guard)==[0xa5a5a5a5]*64
                    checks+=1
            if wb==8:
                for field in ('arraylen','arraydata','arraytype','typesize'):
                    for value in (-1,1,2048):
                        malformed=type(layout).from_buffer_copy(layout)
                        setattr(malformed,field,value)
                        guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                        assert lib.emit(guard,64,C.byref(malformed),ids['IINDX'])==0
                        assert list(guard)==[0xa5a5a5a5]*64
                        checks+=1
            for field in ('fp','framelr','framefp'):
                for value in (-1,1,2048):
                    malformed=type(layout).from_buffer_copy(layout);setattr(malformed,field,value)
                    guard=(C.c_uint32*64)(*[0xa5a5a5a5]*64)
                    assert lib.emit(guard,64,C.byref(malformed),ids['ICALL'])==0
                    assert list(guard)==[0xa5a5a5a5]*64
                    checks+=1
            malformed=type(layout).from_buffer_copy(layout);malformed.framefp=malformed.framelr
            assert lib.emit(None,64,C.byref(malformed),ids['ICALL'])==0
            checks+=1
            if case=='frameabi':continue
            for name in ('IHEADP','IHEADMP','ITAIL','IMOVP','IFRAME','IRET','IADDF'):
                assert not blobs[name]
            code, registers, data, stack = 0x10000, 0x100020000, 0x100030000, 0x40000
            oldpc, target = 0x123456789, 0x987654321

            if case in ('','call'):
                # Check complete REG/data images, including source/target/frame aliases.
                for sa in ((0,4) if rv else range(8)):
                    for da in ((0,4) if rv else range(8)):
                        for framealign in ((0,4) if rv else range(8)):
                            for alias in range(5):
                                if arch=='amd64':u=Uc(UC_ARCH_X86,UC_MODE_64);arg,result=UC_X86_REG_RDI,UC_X86_REG_RAX
                                elif arch=='arm64':u=Uc(UC_ARCH_ARM64,0);arg=result=UC_ARM64_REG_X0
                                else:u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64);arg=result=UC_RISCV_REG_A0
                                for address in (code,registers,data,stack):u.mem_map(address,0x1000)
                                u.mem_write(code,blobs['ICALL'])
                                frame=data+0x500+framealign;previous=data+0x900
                                sp=data+0x100+sa;dp=data+0x200+da
                                if alias in (1,2):dp=frame+(layout.framelr if alias==1 else layout.framefp)
                                if alias in (3,4):sp=frame+(layout.framelr if alias==3 else layout.framefp)
                                image=bytearray(b'\xa5'*0x1000)
                                image[sp-data:sp-data+8]=struct.pack('<Q',frame)
                                image[dp-data:dp-data+8]=struct.pack('<Q',target)
                                u.mem_write(data,bytes(image))
                                expected=bytearray(image)
                                expected[frame-data+layout.framelr:frame-data+layout.framelr+8]=struct.pack('<Q',oldpc)
                                expected[frame-data+layout.framefp:frame-data+layout.framefp+8]=struct.pack('<Q',previous)
                                u.mem_write(registers,struct.pack('<QQQQQ',oldpc,sp,dp,0xdeadbeef,previous))
                                u.reg_write(arg,registers);end=code+0x800
                                if arch=='amd64':u.reg_write(UC_X86_REG_RSP,stack+0x800);u.mem_write(stack+0x800,struct.pack('<Q',end))
                                else:u.reg_write(UC_ARM64_REG_LR if arch=='arm64' else UC_RISCV_REG_RA,end)
                                if rv:
                                    def aligned_call(uc,access,address,size,value,user):
                                        assert address%size==0,(hex(address),size)
                                    u.hook_add(UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,aligned_call)
                                u.emu_start(code,end,count=1000)
                                assert u.reg_read(result)==1,(arch,wb,alias)
                                assert bytes(u.mem_read(data,0x1000))==expected,(arch,wb,alias,framealign)
                                assert bytes(u.mem_read(registers,40))==struct.pack('<QQQQQ',target,sp,dp,0xdeadbeef,frame)
                                checks+=1
                # Rejected frames must not read even an unmapped target slot.
                for frame in ((0,(1<<64)-1,data+0x501,data+0x502,data+0x503) if rv else (0,(1<<64)-1)):
                    if arch=='amd64':u=Uc(UC_ARCH_X86,UC_MODE_64);arg,result=UC_X86_REG_RDI,UC_X86_REG_RAX
                    elif arch=='arm64':u=Uc(UC_ARCH_ARM64,0);arg=result=UC_ARM64_REG_X0
                    else:u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64);arg=result=UC_RISCV_REG_A0
                    for address in (code,registers,data,stack):u.mem_map(address,0x1000)
                    u.mem_write(code,blobs['ICALL']);u.mem_write(data+0x104,struct.pack('<Q',frame))
                    saved=struct.pack('<QQQQQ',oldpc,data+0x104,0x100060000,0xdeadbeef,data+0x900)
                    u.mem_write(registers,saved);u.reg_write(arg,registers);end=code+0x800
                    if arch=='amd64':u.reg_write(UC_X86_REG_RSP,stack+0x800);u.mem_write(stack+0x800,struct.pack('<Q',end))
                    else:u.reg_write(UC_ARM64_REG_LR if arch=='arm64' else UC_RISCV_REG_RA,end)
                    u.emu_start(code,end,count=1000)
                    assert u.reg_read(result)==0 and bytes(u.mem_read(registers,40))==saved
                    checks+=1
                if case=='call':continue

            def run(name, a=0, b=0, alias=0, raw=None, expect=1, poison=False, array=None, arrayalias=0, copy=None, string=None, head=None, chain=None, character=None, headcopy=None, badtarget=False, badsource=False, fixedframe=None, descriptor=None, badresult=False):
                """Exercise a leaf with high native pointers, four-byte alignment and exact aliases."""
                if arch == 'amd64':
                    u = Uc(UC_ARCH_X86,UC_MODE_64); arg, result = UC_X86_REG_RDI, UC_X86_REG_RAX
                elif arch == 'arm64':
                    u = Uc(UC_ARCH_ARM64,0); arg = result = UC_ARM64_REG_X0
                else:
                    u = Uc(UC_ARCH_RISCV,UC_MODE_RISCV64); arg = result = UC_RISCV_REG_A0
                for address in (code,registers,data,stack): u.mem_map(address,0x1000)
                u.mem_write(code,blobs[name])
                s,m,d = data+0x104,data+0x204,data+0x304
                if fixedframe is not None and fixedframe[2] is not None:d=fixedframe[0]+layout.fixedtmp+fixedframe[2]
                if alias == 1: d=s
                if alias == 2: d=m
                if alias == 3: s=d-4
                if alias == 4: d=m+4
                if alias == 5: s=d+4
                output=d
                metadata=None
                typedata=None
                expected_copy=None
                expected_head=None
                if headcopy is not None:
                    pointer,length,delta,countslot=headcopy
                    s=data+0x104;m=data+countslot;d=data+0x500+layout.listdata+delta
                    if alias==1:d=s
                    if alias==2:d=m
                    output=d
                    memory=bytearray((i*29+17)&255 for i in range(0x1000))
                    memory[0x104:0x10c]=struct.pack('<Q',pointer)
                    memory[countslot:countslot+wb]=(length&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    u.mem_write(data,bytes(memory))
                    if length>0 and expect:
                        memory[d-data:d-data+length]=bytes(memory[0x500+layout.listdata:0x500+layout.listdata+length])
                    expected_copy=bytes(memory)
                    if poison:
                        if pointer in (0,(1<<64)-1):m=0x100050000
                        d=0x100050000;output=data+0x100
                elif character is not None:
                    owner,length,index,values=character
                    if poison:owner=data+0x1000-layout.stringdata
                    if alias==1:d=s
                    if alias==2:d=m
                    if alias==3:d=data+0x500+layout.stringdata+4
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    metadata=bytearray(b'\x5a'*128)
                    metadata[layout.stringlen:layout.stringlen+4]=struct.pack('<i',length)
                    for i,bits in enumerate(values):
                        pos=layout.stringdata+i*(4 if length<0 else 1)
                        width=4 if length<0 else 1
                        metadata[pos:pos+width]=(bits&((1<<(width*8))-1)).to_bytes(width,'little')
                    u.mem_write(owner if poison else data+0x500,bytes(metadata[:layout.stringdata] if poison else metadata))
                    u.mem_write(s,struct.pack('<Q',owner))
                    u.mem_write(m,(index&((1<<(8*wb))-1)).to_bytes(wb,'little'))
                    expected_head=bytearray(u.mem_read(data,0x1000))
                    if expect:
                        value=signed(values[index],32) if length<0 else values[index]&255
                        expected_head[d-data:d-data+wb]=(value&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    metadata=None
                elif chain is not None:
                    length,terminal=chain
                    if alias:d=s
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    first=data+0x500 if length else terminal
                    u.mem_write(s,struct.pack('<Q',first))
                    for i in range(length):
                        node=data+0x500+i*16
                        tail=data+0x500+(i+1)*16 if i+1<length else terminal
                        u.mem_write(node+layout.listtail,struct.pack('<Q',tail))
                    expected_head=bytearray(u.mem_read(data,0x1000))
                    if expect:expected_head[d-data:d-data+wb]=length.to_bytes(wb,'little')
                elif head is not None:
                    owner,bits=head
                    if alias==1:d=s
                    if alias==2:d=data+0x500+layout.listdata-4
                    if alias==3:d=data+0x500+layout.listdata+4
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    metadata=bytearray(b'\x5a'*64)
                    metadata[layout.listdata:layout.listdata+8]=struct.pack('<Q',bits)
                    u.mem_write(data+0x500,bytes(metadata))
                    u.mem_write(s,struct.pack('<Q',owner))
                    expected_head=bytearray(u.mem_read(data,0x1000))
                    if expect:
                        width=1 if name=='IHEADB' else wb if name=='IHEADW' else 8
                        expected_head[d-data:d-data+width]=expected_head[0x500+layout.listdata:0x500+layout.listdata+width]
                elif string is not None:
                    owner,length=string
                    if alias:d=s
                    output=d
                    u.mem_write(d-8,b'\xa5'*32)
                    u.mem_write(s,struct.pack('<Q',owner))
                    metadata=bytearray(b'\x5a'*64)
                    metadata[layout.stringlen:layout.stringlen+4]=struct.pack('<i',length)
                    u.mem_write(data+0x500,bytes(metadata))
                elif copy is not None:
                    src,dst,length,countslot=copy
                    s=data+src;d=data+dst;m=data+countslot;output=d
                    memory=bytearray((i*29+17)&255 for i in range(0x1000))
                    memory[countslot:countslot+wb]=(length&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    u.mem_write(data,bytes(memory))
                    if length>0 and expect:memory[dst:dst+length]=bytes(memory[src:src+length])
                    expected_copy=bytes(memory)
                    if length<=0 or not expect:s=d=0x100050000;output=data+0x100
                elif array is None:
                    u.mem_write(d-8,b'\xa5'*32)
                    u.mem_write(s,raw if raw is not None else struct.pack('<Q',a & ((1<<64)-1)))
                    if not poison: u.mem_write(m,struct.pack('<Q',b & ((1<<64)-1)))
                    else: m=0x100050000
                    if name.startswith('IB') or name == 'IJMP': u.mem_write(d,struct.pack('<Q',target))
                else:
                    owner,length,index,backing=array
                    if name=='ILENA':
                        if arrayalias:d=s
                        output=d
                    else:
                        if arrayalias==1:m=d
                        if arrayalias==2:m=s
                        output=m
                    u.mem_write(output-8,b'\xa5'*32)
                    u.mem_write(m,struct.pack('<Q',123))
                    u.mem_write(s,struct.pack('<Q',owner))
                    if name!='ILENA':u.mem_write(d,struct.pack('<Q',index&((1<<64)-1)))
                    metadata=bytearray(b'\x5a'*64)
                    metadata[layout.arraylen:layout.arraylen+wb]=(length&((1<<(8*wb))-1)).to_bytes(wb,'little')
                    metadata[layout.arraydata:layout.arraydata+8]=struct.pack('<Q',backing)
                    if descriptor is not None:
                        typeptr,width=descriptor
                        metadata[layout.arraytype:layout.arraytype+8]=struct.pack('<Q',typeptr)
                        typedata=bytearray(b'\x6b'*64)
                        typedata[layout.typesize:layout.typesize+4]=struct.pack('<i',width)
                        u.mem_write(data+0x700,bytes(typedata))
                    u.mem_write(data+0x500,bytes(metadata))
                fp=data+0x900 if fixedframe is None else fixedframe[0]
                if fixedframe is not None and fp not in (0,(1<<64)-1):
                    u.mem_write(fp+layout.fixedtmp,(fixedframe[1]&((1<<(8*wb))-1)).to_bytes(wb,'little'))
                before = bytes(u.mem_read(output-8,32))
                if badresult:m=0x100060000
                if badtarget:d=0x100060000
                if badsource:s=0x100060000
                u.mem_write(registers,struct.pack('<QQQQQ',oldpc,s,d,m,fp))
                u.reg_write(arg,registers)
                end=code+0x800
                if arch == 'amd64':
                    u.reg_write(UC_X86_REG_RSP,stack+0x800); u.mem_write(stack+0x800,struct.pack('<Q',end))
                else: u.reg_write(UC_ARM64_REG_LR if arch == 'arm64' else UC_RISCV_REG_RA,end)
                if rv:
                    def aligned_access(uc, access, address, size, value, user):
                        assert address % size == 0, (name, hex(address), size)
                    u.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, aligned_access)
                u.emu_start(code,end,count=4000)
                if expected_head is not None:assert bytes(u.mem_read(data,0x1000))==bytes(expected_head),(arch,wb,name,head,alias)
                if expected_copy is not None:assert bytes(u.mem_read(data,0x1000))==expected_copy,(arch,wb,copy)
                assert u.reg_read(result) == expect,(arch,wb,name,a,b,u.reg_read(result),expect)
                assert bytes(u.mem_read(registers+8,32)) == struct.pack('<QQQQ',s,d,m,fp)
                if typedata is not None:assert bytes(u.mem_read(data+0x700,64))==bytes(typedata)
                if metadata is not None and head is None:assert bytes(u.mem_read(data+0x500,64))==metadata
                return bytes(u.mem_read(output,16)), int.from_bytes(u.mem_read(registers,8),'little'), before, bytes(u.mem_read(output-8,32)),s

            if case in ('','descriptor'):
                if wb==4:assert not blobs['IINDX']
                else:
                    assert blobs['IINDX'],(arch,wb,'missing descriptor leaf')
                    for width in (-1,0,1,3,7,8,16,0x7fffffff):
                        for length in (-1,0,1,8,0x80000001,(1<<63)-1):
                            for index in (-1,0,1,7,length-1,length):
                                for backing in (data+0x800,0,(1<<64)-1,(1<<64)-16):
                                    valid=0<=index<length and width>=0 and (width==0 or
                                          backing not in (0,(1<<64)-1) and index*width<=0x7fffffff and backing+index*width<(1<<64))
                                    for alias in (0,1,2):
                                        out,pc,before,after,_=run('IINDX',array=(data+0x500,length,index,backing),arrayalias=alias,
                                                                descriptor=(data+0x700,width),expect=int(valid))
                                        if valid:
                                            assert int.from_bytes(out[:8],'little')==backing+index*width,(arch,width,length,index,backing)
                                            assert before[:8]==after[:8] and before[16:]==after[16:]
                                        else:assert before==after
                                        assert pc==oldpc
                                        checks+=1
                                    if not valid:
                                        _,pc,before,after,_=run('IINDX',array=(data+0x500,length,index,backing),
                                                                descriptor=(data+0x700,width),expect=0,badresult=True)
                                        assert pc==oldpc and before==after
                                        checks+=1
                    for owner in (0,(1<<64)-1):
                        _,pc,before,after,_=run('IINDX',array=(owner,8,0,data+0x800),descriptor=(data+0x700,3),expect=0,badtarget=True,badresult=True)
                        assert pc==oldpc and before==after
                        checks+=1
                    for typeptr in (0,(1<<64)-1):
                        _,pc,before,after,_=run('IINDX',array=(data+0x500,8,3,data+0x800),descriptor=(typeptr,3),expect=0,badresult=True)
                        assert pc==oldpc and before==after
                        checks+=1
                if case=='descriptor':continue
            if case in ('','framefixed'):
                values=[0,1,-1,127,-128,0x7fffffff,-0x80000000,(1<<63)-1,-(1<<63)]
                pairs=[(a,b) for a in values for b in values]+[(signed(rng.getrandbits(64),64),signed(rng.getrandbits(64),64)) for _ in range(16)]
                counts=[-63,-33,-32,-1,0,1,31,32,62,63,-64,64,-(1<<(wb*8-1))]
                for name in ('IMULX','IDIVX'):
                    for a,b in pairs:
                        aa,bb=signed(a,wb*8),signed(b,wb*8)
                        for count in counts:
                            valid=-63<=count<=63 and (name=='IMULX' or aa not in (0,-1))
                            for alias in (0,1,2,3,5):
                                out,pc,before,after,_=run(name,a,b,alias=alias,expect=int(valid),fixedframe=(data+0x900,count,None))
                                if valid:
                                    intermediate=signed(aa*bb,64) if name=='IMULX' else bb
                                    intermediate=signed(intermediate<<count,64) if count>=0 else intermediate>>-count
                                    if name=='IDIVX':
                                        magnitude=abs(intermediate)//abs(aa)
                                        intermediate=-magnitude if (intermediate<0)!=(aa<0) else magnitude
                                    assert int.from_bytes(out[:wb],'little')==intermediate&((1<<(8*wb))-1),(arch,wb,name,aa,bb,count,alias)
                                    assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                                else:assert before==after
                                assert pc==oldpc
                                checks+=1
                    for count in range(-63,64):
                        a,b=3,-7
                        for delta in (-4,0,4):
                            out,_,before,after,_=run(name,a,b,fixedframe=(data+0x900,count,delta))
                            value=a*b if name=='IMULX' else b
                            value=signed(value<<count,64) if count>=0 else value>>-count
                            if name=='IDIVX':value=-(abs(value)//a) if value<0 else value//a
                            assert int.from_bytes(out[:wb],'little')==value&((1<<(8*wb))-1)
                            assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            checks+=1
                    if rv:
                        for delta in (1,2,3):
                            _,pc,before,after,_=run(name,expect=0,poison=True,badsource=True,fixedframe=(data+0x900+delta,0,None))
                            assert pc==oldpc and before==after
                            checks+=1
                    for frame in (0,(1<<64)-1):
                        _,pc,before,after,_=run(name,expect=0,poison=True,badsource=True,fixedframe=(frame,0,None))
                        assert pc==oldpc and before==after
                        checks+=1
                    for count in (-64,64,-(1<<(wb*8-1))):
                        _,pc,before,after,_=run(name,expect=0,poison=True,badsource=True,fixedframe=(data+0x900,count,None))
                        assert pc==oldpc and before==after
                        checks+=1
                for divisor in (0,-1):
                    _,pc,before,after,_=run('IDIVX',divisor,7,expect=0,poison=True,fixedframe=(data+0x900,0,None))
                    assert pc==oldpc and before==after
                    checks+=1
                if case=='framefixed':continue
            if case in ('','resize'):
                singles=[0,1<<31,1,(1<<31)|1,0x7fffff,0x800000,0x807fffff,0x80800000,
                         0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,
                         0x7fc00042,0x7f800042,0xffc00042,0xff800042]
                singles += [rng.getrandbits(32) for _ in range(128)]
                doubles=[0,1<<63,1,(1<<63)|1,0x10000000000000,0x7fefffffffffffff,
                         0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000042,0x7ff0000000000042]
                for bits in singles:
                    if (bits>>23)&255 != 255:
                        wide=int.from_bytes(struct.pack('<d',struct.unpack('<f',bits.to_bytes(4,'little'))[0]),'little')
                        doubles += [wide,(wide+1)&((1<<64)-1),(wide-1)&((1<<64)-1)]
                doubles += [rng.getrandbits(64) for _ in range(128)]
                for name,values,insize,outsize in [('ICVTRF',singles,4,8),('ICVTFR',doubles,8,4)]:
                    for bits in values:
                        magnitude=bits&((1<<(insize*8-1))-1)
                        exponent=(magnitude>>(23 if insize==4 else 52))
                        valid=(magnitude==0 or (0<exponent<255 if insize==4 else
                               897<=exponent<=1150 and (magnitude&((1<<29)-1))==0))
                        raw=bits.to_bytes(insize,'little')
                        if valid:
                            number=struct.unpack('<f' if insize==4 else '<d',raw)[0]
                            expected=struct.pack('<d' if outsize==8 else '<f',number)
                        for alias in (0,1,2,3,5):
                            out,pc,before,after,_=run(name,alias=alias,raw=raw,expect=int(valid),poison=True)
                            if valid:
                                assert out[:outsize]==expected,(arch,wb,name,hex(bits),alias,out,expected)
                                assert before[:8]==after[:8] and before[8+outsize:]==after[8+outsize:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                        if not valid:
                            _,pc,before,after,_=run(name,raw=raw,expect=0,poison=True,badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                if case=='resize':continue
            if case in ('','integerreal'):
                values=[0,1,-1,127,-128,0x7fffffff,-0x80000000,0xffffffff,
                        (1<<52)-1,1<<52,(1<<53)-1,1<<53,(1<<53)+1,
                        -((1<<53)-1),-(1<<53),-((1<<53)+1),(1<<63)-1,-(1<<63)]
                values += [rng.randrange(-(1<<53)+1,1<<53) for _ in range(96)]
                values += [signed(rng.getrandbits(64),64) for _ in range(64)]
                for name,width in (('ICVTWF',wb),('ICVTLF',8)):
                    for value in values:
                        number=signed(value,width*8)
                        valid=abs(number)<(1<<53)
                        for alias in (0,1,2,3,5):
                            out,pc,before,after,_=run(name,value,alias=alias,expect=int(valid),poison=True)
                            if valid:
                                assert out[:8]==struct.pack('<d',float(number)),(arch,wb,name,number,alias,out)
                                assert before[:8]==after[:8] and before[16:]==after[16:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                        if not valid:
                            _,pc,before,after,_=run(name,value,expect=0,poison=True,badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                if case=='integerreal':continue
            if case in ('','fixed'):
                # Fixed shifts use a signed 64-bit intermediate even in WORD32.
                values=[0,1,-1,127,-128,0x7fffffff,0x80000000,0xffffffff,
                        0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff]
                counts=list(range(-63,64))+[-64,64,65,-(1<<(wb*8-1)),(1<<(wb*8-1))-1]
                for value in values:
                    for count in counts:
                        valid=-63<=count<=63
                        for alias in (0,1,2,3,5):
                            out,pc,before,after,_=run('ICVTXX',value,count,alias=alias,expect=int(valid))
                            # d=m aliases preserve the same count; s=d-4/s=d+4 never overlaps m.
                            if valid:
                                expected=((signed(value,wb*8)<<count) if count>=0 else (signed(value,wb*8)>>-count))&((1<<(wb*8))-1)
                                assert int.from_bytes(out[:wb],'little')==expected,(arch,wb,value,count,alias,out)
                                assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                for count in (-64,64,65,-(1<<(wb*8-1))):
                    _,pc,before,after,_=run('ICVTXX',0,count,expect=0,badsource=True)
                    assert pc==oldpc and before==after
                    checks+=1
                if case=='fixed':continue
            if case in ('','scalar'):
                patterns=[0,1,127,128,255,256,0x8000,0xffff,0x7fffffff,0x80000000,
                          0xffffffff,0x100000000,0x8000000000000000,0xffffffffffffffff,
                          0x7ff0000000000042,0xfff8000000000042]
                for name,width in [('IMOVB',1),('IMOVW',wb),('IMOVL',8),('IMOVF',8),
                                   ('ICVTBW',wb),('ICVTWB',1),('ICVTWS',2),('ICVTSW',wb),
                                   ('ICVTWL',8),('ICVTLW',wb)]:
                    for bits in patterns:
                        raw=bits.to_bytes(8,'little')
                        wanted=(bits&255 if name=='ICVTBW' else
                                signed(bits,16) if name=='ICVTSW' else
                                signed(bits,wb*8) if name=='ICVTWL' else bits)
                        for alias in (0,1,3,5):
                            out,pc,before,after,_=run(name,alias=alias,raw=raw,poison=True)
                            assert int.from_bytes(out[:width],'little')==wanted&((1<<(8*width))-1),(arch,wb,name,bits,alias)
                            assert pc==oldpc and before[:8]==after[:8] and before[8+width:]==after[8+width:]
                            checks+=1
                if case=='scalar':continue
            if case in ('','realbranch'):
                floats=[0,1<<63,0x3ff0000000000000,0xbff0000000000000,
                        0x0010000000000000,0x8010000000000000,0x7fefffffffffffff,0xffefffffffffffff,
                        0x7ff0000000000000,0xfff0000000000000,1,0x8000000000000001,
                        0xfffffffffffff,0x800fffffffffffff,0x7ff8000000000042,0xfff8000000000042,
                        0x7ff0000000000042,0xfff0000000000042]
                def native_real(bits):
                    exponent=(bits>>52)&2047
                    fraction=bits&((1<<52)-1)
                    return fraction==0 or 0<exponent<2047
                realpairs=[(a,b) for a in floats for b in floats]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(64)]
                for name,relation in [('IBEQF',0),('IBNEF',1),('IBLTF',2),('IBLEF',3),('IBGTF',4),('IBGEF',5)]:
                    for a,b in realpairs:
                        left=struct.unpack('<d',struct.pack('<Q',a))[0]
                        right=struct.unpack('<d',struct.pack('<Q',b))[0]
                        take=[left==right,left!=right,left<right,left<=right,left>right,left>=right][relation]
                        valid=native_real(a) and native_real(b)
                        _,pc,before,after,_=run(name,a,b,expect=int(valid))
                        assert pc==(target if valid and take else oldpc),(arch,wb,name,hex(a),hex(b),pc)
                        assert before==after
                        checks+=1
                        if not valid or not take:
                            _,pc,before,after,_=run(name,a,b,expect=int(valid),badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                    for a in floats:
                        if not native_real(a):
                            _,pc,before,after,_=run(name,a,poison=True,expect=0,badtarget=True)
                            assert pc==oldpc and before==after
                            checks+=1
                if case=='realbranch':continue
            if case in ('','headcopy'):
                for length in (0,1,2,3,7,8,15,16,31,32,63,64,127,128):
                    for delta in (-17,-1,0,1,4,17,129):
                        for slot in (0x204,0x500+layout.listdata+4,0x500+layout.listdata+16):
                            _,pc,_,_,_=run('IHEADM',headcopy=(data+0x500,length,delta,slot))
                            assert pc==oldpc
                            checks+=1
                for pointer in (0,(1<<64)-1):
                    for length in (-1,0,1,128,129,-(1<<(wb*8-1)),(1<<(wb*8-1))-1):
                        for poison in (False,True):
                            _,pc,before,after,_=run('IHEADM',headcopy=(pointer,length,0,0x204),expect=0,poison=poison)
                            assert pc==oldpc and before==after
                            checks+=1
                for length in (0,-1,129,256,-(1<<(wb*8-1)),(1<<(wb*8-1))-1):
                    _,pc,before,after,_=run('IHEADM',headcopy=(data+0x500,length,0,0x204),expect=int(length==0),poison=True)
                    assert pc==oldpc and before==after
                    checks+=1
                for length in (1,7,8,31,128):
                    for alias in (1,2):
                        _,pc,_,_,_=run('IHEADM',headcopy=(data+0x500,length,0,0x204),alias=alias)
                        assert pc==oldpc
                        checks+=1
                if case=='headcopy':continue
            if case in ('','power'):
                exponents=[-1,-2,-3,-(1<<(wb*8-1)),0,1,2,3,7,31,32,63,64,127,(1<<(wb*8-1))-1]
                if wb==8:exponents += [1<<32,(1<<32)+1]
                bases=[0,1,-1,2,-2,3,-3,(1<<31)-1,-(1<<31),(1<<63)-1,-(1<<63),0x123456789abcdef]
                for name,width in (('IEXPW',wb),('IEXPL',8)):
                    for exponent in exponents:
                        for base in bases:
                            for alias in (0,1,2,3,4):
                                out,pc,before,after,_=run(name,exponent,base,alias,expect=int(exponent>=0))
                                if exponent>=0:
                                    wanted=pow(base,exponent,1<<(width*8))
                                    assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,exponent,base)
                                    assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                                else:assert before==after
                                assert pc==oldpc
                                checks+=1
                    for exponent in (-1,-(1<<(wb*8-1))):
                        _,pc,before,after,_=run(name,exponent,poison=True,expect=0)
                        assert pc==oldpc and before==after
                        checks+=1
                if case=='power':continue
            if case in ('','character'):
                values=[0,0x7f,0x80,0xff,0x10ffff,0x7fffffff,0x80000000,0xffffffff]
                for pointer in (0,(1<<64)-1,data+0x500):
                    for length in (-2147483648,-8,-1,0,1,8):
                        for index in [-1,0,1,7,8,(1<<(wb*8-1))-1]+([1<<32,(1<<32)+1,-(1<<63)] if wb==8 else []):
                            for alias in (0,1,2,3):
                                valid=pointer==data+0x500 and length!=-2147483648 and 0<=index<abs(length)
                                out,pc,before,after,_=run('IINDC',alias=alias,character=(pointer,length,index,values),expect=int(valid))
                                if not valid:assert before==after
                                assert pc==oldpc
                                checks+=1
                for length in (-8,8):
                    for index in (-1,8,(1<<(wb*8-1))-1):
                        _,pc,before,after,_=run('IINDC',character=(data+0x500,length,index,values),expect=0,poison=True)
                        assert before==after and pc==oldpc
                        checks+=1
                if case=='character':continue
            if case in ('','listlen'):
                for length in (0,1,2,7,31,127,128,129):
                    for terminal in (0,(1<<64)-1,data+0x500):
                        for alias in (0,1):
                            valid=length<=128 and terminal==(1<<64)-1
                            out,pc,before,after,_=run('ILENL',alias=alias,chain=(length,terminal),expect=int(valid))
                            if valid:
                                assert int.from_bytes(out[:wb],'little')==length
                                assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                if case=='listlen':continue
            if case in ('','head'):
                patterns=[0,1,(1<<64)-1,1<<63,0x7ff8000000000042,0x7ff0000000000042]+[rng.getrandbits(64) for _ in range(32)]
                for name in ('IHEADB','IHEADW','IHEADL','IHEADF'):
                    width=1 if name=='IHEADB' else wb if name=='IHEADW' else 8
                    for pointer in (0,(1<<64)-1,data+0x500):
                        for bits in patterns:
                            for alias in (0,1,2,3):
                                valid=pointer==data+0x500
                                out,pc,before,after,_=run(name,alias=alias,head=(pointer,bits),expect=int(valid))
                                if valid:assert int.from_bytes(out[:width],'little')==bits&((1<<(8*width))-1)
                                else:assert before==after
                                assert pc==oldpc
                                checks+=1
                if case=='head':continue
            if case in ('','string'):
                for pointer in (0,(1<<64)-1,data+0x500):
                    for length in (-2147483648,-2147483647,-65537,-1,0,1,65537,2147483647):
                        for alias in (0,1):
                            valid=pointer!=0 and (pointer==(1<<64)-1 or length!=-2147483648)
                            out,pc,before,after,_=run('ILENC',alias=alias,string=(pointer,length),expect=int(valid))
                            if valid:
                                wanted=0 if pointer==(1<<64)-1 else abs(length)
                                assert int.from_bytes(out[:wb],'little')==wanted,(arch,wb,pointer,length)
                                assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:]
                            else:assert before==after
                            assert pc==oldpc
                            checks+=1
                if case=='string':continue
            if case in ('','copy'):
                for length in (0,1,2,3,7,8,15,16,31,32,63,64,127,128):
                    for delta in (-129,-64,-17,-8,-1,0,1,3,8,17,64,129):
                        for countslot in (0x704,0x304,0x308):
                            _,pc,_,_,_=run('IMOVM',copy=(0x304,0x304+delta,length,countslot))
                            assert pc==oldpc
                            checks+=1
                for length in (-1,-(1<<(wb*8-1)),129,256,(1<<(wb*8-1))-1):
                    _,pc,_,_,_=run('IMOVM',copy=(0x304,0x305,length,0x704),expect=0)
                    assert pc==oldpc
                    checks+=1
                if case=='copy':continue
            if case=='control':
                # Exercise signed-width boundaries and ensure false branches never read targets.
                relations={'EQ':lambda a,b:a==b,'NE':lambda a,b:a!=b,'LT':lambda a,b:a<b,
                           'LE':lambda a,b:a<=b,'GT':lambda a,b:a>b,'GE':lambda a,b:a>=b}
                edges=[0,1,127,128,255,256,0x7fffffff,0x80000000,0xffffffff,
                       0x100000000,0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff]
                for suffix,width in (('B',1),('W',wb),('L',8)):
                    for relation,predicate in relations.items():
                        name='IBNEW' if relation=='NE' and suffix=='W' else 'IB'+relation+suffix
                        for a in edges:
                            for b in edges:
                                aa=a&255 if width==1 else signed(a,width*8)
                                bb=b&255 if width==1 else signed(b,width*8)
                                take=predicate(aa,bb)
                                _,pc,before,after,_=run(name,a,b,badtarget=not take)
                                assert pc==(target if take else oldpc),(arch,wb,name,aa,bb)
                                assert before==after
                                checks+=1
                assert run('IJMP')[1]==target
                checks+=1
                continue
            if case=='guard':
                counts=[-1,wb*8,-(1<<(wb*8-1))]+([1<<32] if wb==8 else [])
                for count in counts:
                    _,pc,before,after,_=run('ISHLW',count,poison=True,expect=0)
                    assert before==after and pc==oldpc
                    checks+=1
                continue
            owner=data+0x500
            for pointer in (0,(1<<64)-1,owner):
                for length in (-1,0,1,17,(1<<(wb*8-1))-1):
                    for alias in (0,1):
                        out,pc,before,after,_=run('ILENA',array=(pointer,length,0,0),arrayalias=alias)
                        wanted=(length if pointer==owner else 0)&((1<<(wb*8))-1)
                        assert int.from_bytes(out[:wb],'little')==wanted,(arch,wb,'ILENA',pointer,length,out,wanted)
                        assert before[:8]==after[:8] and before[8+wb:]==after[8+wb:] and pc==oldpc
                        checks+=1
            for name in ('IINDB','IINDW','IINDL','IINDF'):
                if wb==4:
                    assert not blobs[name]
                    continue
                stride=1 if name=='IINDB' else 8
                for pointer in (0,(1<<64)-1,owner):
                    for length in (-1,0,1,8,17,1<<40):
                        for index in (-1,0,1,7,8,16,17,1<<32,1<<39,-(1<<63)):
                            for backing in (0,(1<<64)-1,data+0x804,(1<<64)-2):
                                valid=(pointer==owner and 0<=index<length and index<=0x7fffffff//stride and
                                       backing not in (0,(1<<64)-1) and backing+index*stride<(1<<64))
                                for alias in (0,1,2):
                                    out,pc,before,after,_=run(name,expect=int(valid),array=(pointer,length,index,backing),arrayalias=alias)
                                    if valid:
                                        assert int.from_bytes(out[:8],'little')==backing+index*stride,(arch,name,length,index,backing,out)
                                        assert before[:8]==after[:8] and before[16:]==after[16:]
                                    else:assert before==after
                                    assert pc==oldpc
                                    checks+=1
            if case=='array':continue
            edges=[0,1,255,256,(1<<31)-1,1<<31,(1<<32)-1,1<<63,(1<<64)-1]
            pairs=[(a,b) for a in edges for b in edges]+[(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(64)]
            for suffix,width in (('B',1),('W',wb),('L',8)):
                mask=(1<<(8*width))-1
                for operation in ('ADD','SUB','MUL','AND','OR','XOR'):
                    name='I'+operation+suffix
                    for a,b in pairs:
                        wanted={'ADD':lambda:b+a,'SUB':lambda:b-a,'MUL':lambda:b*a,
                                'AND':lambda:b&a,'OR':lambda:b|a,'XOR':lambda:b^a}[operation]()&mask
                        for alias in (0,1,2):
                            out,pc,before,after,_=run(name,a,b,alias)
                            assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,a,b,out,wanted)
                            assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                            assert pc==oldpc
                            checks+=1
            relations={'EQ':lambda a,b:a==b,'NE':lambda a,b:a!=b,'LT':lambda a,b:a<b,
                       'LE':lambda a,b:a<=b,'GT':lambda a,b:a>b,'GE':lambda a,b:a>=b}
            for suffix,width in (('B',1),('W',wb),('L',8)):
                for relation,predicate in relations.items():
                    name='IBNEW' if relation=='NE' and suffix=='W' else 'IB'+relation+suffix
                    for a,b in pairs[:81]:
                        _,pc,before,after,_=run(name,a,b)
                        aa=a&255 if width==1 else signed(a,width*8)
                        bb=b&255 if width==1 else signed(b,width*8)
                        assert pc==(target if predicate(aa,bb) else oldpc),(arch,wb,name,aa,bb,pc)
                        assert before==after
                        checks+=1
            for bits in [0,1,0x8000000000000000,0x3ff0000000000000,
                         0x7ff0000000000000,0xfff0000000000000,
                         0x7ff8000000000042,0x7ff0000000000042] + [rng.getrandbits(64) for _ in range(128)]:
                for alias in (0,1,3):
                    out,pc,before,after,_=run('INEGF',alias=alias,raw=struct.pack('<Q',bits))
                    assert int.from_bytes(out[:8],'little')==bits^(1<<63)
                    assert pc==oldpc and before[:8]==after[:8] and before[16:]==after[16:]
                    checks+=1
            for name,width in [('IMOVB' ,1),('IMOVW',wb),('IMOVL',8),('IMOVF',8),('ICVTBW',wb),('ICVTWB',1),('ICVTWS',2),('ICVTSW',wb),('ICVTWL',8),('ICVTLW',wb)]:
                for raw in (bytes.fromhex('efcdab8967452301'),bytes.fromhex('00800000ffffffff')):
                    wanted=(int.from_bytes(raw[:1],'little') if name=='ICVTBW' else
                            signed(int.from_bytes(raw[:2],'little'),16) if name=='ICVTSW' else
                            signed(int.from_bytes(raw[:wb],'little'),wb*8) if name=='ICVTWL' else int.from_bytes(raw[:width],'little'))
                    for alias in (0,1,3):
                        out,_,before,after,_=run(name,alias=alias,raw=raw)
                        assert int.from_bytes(out[:width],'little')==wanted&((1<<(8*width))-1),(arch,wb,name,out)
                        assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                        checks+=1
            for suffix,width in (('B',1),('W',wb),('L',8)):
                bits=width*8; mask=(1<<bits)-1
                shiftops=['ISHL'+suffix,'ISHR'+suffix]+(['ILSR'+suffix] if suffix!='B' else [])
                for name in shiftops:
                    counts=[-1,0,1,7,8,31,32,63,64,65,(1<<(wb*8-1))-1]
                    if wb==8: counts += [1<<32,(1<<32)+1,-(1<<63)]
                    for count in counts:
                        for value in edges:
                            valid=0<=count<bits
                            for alias in (0,1,2):
                                out,pc,before,after,_=run(name,count,value,alias,expect=int(valid))
                                if valid:
                                    wanted=((value<<count) if name.startswith('ISHL') else
                                            signed(value,bits)>>count if name.startswith('ISHR') and suffix!='B' else
                                            (value&mask)>>count)&mask
                                    assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,count,value,out,wanted)
                                    assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                                else: assert before==after
                                assert pc==oldpc
                                checks+=1
                        if not (0<=count<bits):
                            _,pc,before,after,_=run(name,count,poison=True,expect=0)
                            assert before==after and pc==oldpc
                            checks+=1
                for name in ('IDIV'+suffix,'IMOD'+suffix):
                    for divisor,dividend in pairs:
                        a=divisor&255 if width==1 else signed(divisor,bits)
                        b=dividend&255 if width==1 else signed(dividend,bits)
                        native=a!=0 and not (arch=='amd64' and a==-1)
                        for alias in (0,1,2):
                            out,pc,before,after,_=run(name,divisor,dividend,alias,expect=int(native))
                            if native:
                                q=abs(b)//abs(a)
                                if (a<0)!=(b<0):q=-q
                                wanted=(q if name.startswith('IDIV') else b-q*a)&mask
                                assert int.from_bytes(out[:width],'little')==wanted,(arch,wb,name,a,b,out,wanted)
                                assert before[:8]==after[:8] and before[8+width:]==after[8+width:]
                            else: assert before==after
                            assert pc==oldpc
                            checks+=1
                    _,pc,before,after,_=run(name,0,poison=True,expect=0)
                    assert before==after and pc==oldpc
                    checks+=1
            assert run('IJMP')[1]==target
            if wb==8:
                out,_,_,_,s=run('ILEA'); assert int.from_bytes(out[:8],'little')==s
            else: assert not blobs['ILEA']
        print('PASS',arch,'native leaves',flush=True)
print('PASS leaf64 JIT:',checks,'instruction/ABI/alias/oracle checks')
