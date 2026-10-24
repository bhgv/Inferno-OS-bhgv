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
ops = '''ILENC IMOVM INEGF ILENA IINDB IINDW IINDL IINDF IRUNT IMOVB IMOVW IMOVL IMOVF ICVTBW ICVTWB ICVTWS ICVTSW ILEA IJMP
IADDB IADDW ISUBB ISUBW IMULB IMULW IANDB IANDW IORB IORW IXORB IXORW
IADDL ISUBL IMULL IANDL IORL IXORL IBEQB IBEQW IBNEB IBNEW IBLTB IBLTW
IBLEB IBLEW IBGTB IBGTW IBGEB IBGEW ICVTWL ICVTLW ISHLB ISHLW ISHLL ISHRB ISHRW ISHRL ILSRW ILSRL IDIVB IDIVW IDIVL IMODB IMODW IMODL IBEQL IBNEL IBLTL IBLEL IBGTL IBGEL IMOVP IFRAME ICALL IRET IADDF'''.split()
class Layout(C.Structure):
    _fields_ = [(x, C.c_int) for x in ('wordbytes', 'pc', 's', 'd', 'm', 'arraylen', 'arraydata', 'stringlen')]
class RvLayout(C.Structure):
    _fields_ = [(x, C.c_int) for x in ('xlen', 'wordbytes', 'hasm', 'pc', 's', 'd', 'm', 'arraylen', 'arraydata', 'stringlen')]


def signed(value, bits):
    """Interpret a bit vector independently of the emitter's chosen machine instructions."""
    value &= (1 << bits)-1
    return value-(1 << bits) if value >> (bits-1) else value


case=os.environ.get('LEAF64_CASE','')
selected_arch=os.environ.get('LEAF64_ARCH','')
if case not in ('','guard','array','control','copy','string'):raise ValueError(case)
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
            layout.arraylen=8;layout.arraydata=32;layout.stringlen=12
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
            for name in ('IMOVP','IFRAME','ICALL','IRET','IADDF'):
                assert not blobs[name]
            code, registers, data, stack = 0x10000, 0x100020000, 0x100030000, 0x40000
            oldpc, target = 0x123456789, 0x987654321

            def run(name, a=0, b=0, alias=0, raw=None, expect=1, poison=False, array=None, arrayalias=0, copy=None, string=None):
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
                if alias == 1: d=s
                if alias == 2: d=m
                if alias == 3: s=d-4
                output=d
                metadata=None
                expected_copy=None
                if string is not None:
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
                    u.mem_write(data+0x500,bytes(metadata))
                before = bytes(u.mem_read(output-8,32))
                u.mem_write(registers,struct.pack('<QQQQ',oldpc,s,d,m))
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
                if expected_copy is not None:assert bytes(u.mem_read(data,0x1000))==expected_copy,(arch,wb,copy)
                assert u.reg_read(result) == expect,(arch,wb,name,a,b,u.reg_read(result),expect)
                assert bytes(u.mem_read(registers+8,24)) == struct.pack('<QQQ',s,d,m)
                if metadata is not None:assert bytes(u.mem_read(data+0x500,64))==metadata
                return bytes(u.mem_read(output,16)), int.from_bytes(u.mem_read(registers,8),'little'), before, bytes(u.mem_read(output-8,32)),s

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
                for suffix in ('B','W','L'):
                    for relation in ('EQ','NE','LT','LE','GT','GE'):
                        name='IBNEW' if relation=='NE' and suffix=='W' else 'IB'+relation+suffix
                        for a,b in ((0,0),(0,1),(1,0),(-1,1),(1,-1)):
                            run(name,a,b)
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
