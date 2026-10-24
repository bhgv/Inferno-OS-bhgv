#!/usr/bin/env python3
"""Exercise the production RV32/RV64 encoder with host C + Unicorn/Capstone.

No target OS or SDK is needed. This checks native leaf operations, their ABI
and the runtime integration separately; it does not boot FreeRTOS/Inferno.
"""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile

from capstone import Cs, CS_ARCH_RISCV, CS_MODE_RISCV32, CS_MODE_RISCV64
from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32, UC_MODE_RISCV64, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
import unicorn.riscv_const as regs

ROOT = Path(__file__).resolve().parents[2]
cc = shlex.split(os.environ.get('CC', 'cc'))
flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g'))
checks = 0
rng = random.Random(0x1f3e)
names = '''ICVTXX IBEQF IBNEF IBLTF IBLEF IBGTF IBGEF IHEADM IEXPW IEXPL IEXPF INEGF IRUNT IMOVB IMOVW IMOVL IMOVF ICVTBW ICVTWB ICVTWS ICVTSW ILEA IJMP
IADDB IADDW ISUBB ISUBW IMULB IMULW IANDB IANDW IORB IORW IXORB IXORW
ISHLB ISHLW ISHRB ISHRW ILSRW IBEQB IBEQW IBNEB IBNEW IBLTB IBLTW IBLEB
IBLEW IBGTB IBGTW IBGEB IBGEW IDIVW IMODW IMOVP IFRAME ICALL IRET IADDF'''.split()

class Layout(C.Structure):
    _fields_ = [(name, C.c_int) for name in ('xlen','wordbytes','hasm','pc','s','d','m','arraylen','arraydata','stringlen','stringdata','listdata','listtail')]


def signed(n, bits):
    n &= (1 << bits)-1
    return n - (1 << bits) if n >> (bits-1) else n


with tempfile.TemporaryDirectory(prefix='riscv-jit-') as td:
    td = Path(td)
    source = td/'emit.c'
    source.write_text('#include <stdint.h>\ntypedef uint32_t u32int;\n#include "isa.h"\n'
                      '#include "riscv-emit.h"\n'
                      'int emit(u32int *out,int cap,RvLayout *abi,int op){return rvemit(out,cap,abi,op);}\n'
                      'int opid(int n){static int ops[]={' + ','.join(names) + '};return ops[n];}\n')
    binary = td/'emit.so'
    subprocess.run(cc+flags+['-shared','-fPIC','-I'+str(ROOT/'include'),
                            '-I'+str(ROOT/'libinterp'),str(source),'-o',str(binary)], check=True)
    lib = C.CDLL(str(binary))
    lib.emit.argtypes = [C.POINTER(C.c_uint32),C.c_int,C.POINTER(Layout),C.c_int]
    ops = {name: lib.opid(i) for i,name in enumerate(names)}

    def emit(name, abi, cap=64):
        out=(C.c_uint32*64)(*[0xdeadbeef]*64)
        n=lib.emit(out,cap,C.byref(abi),ops[name])
        assert all(v == 0xdeadbeef for v in list(out)[cap:]), 'encoder wrote past capacity'
        return n,bytes(out)[:n*4]

    for xlen, wb in [(32,4),(64,4),(64,8)]:
        pb=xlen//8
        abi=Layout(xlen,wb,1,0,pb,pb*2,pb*3)
        dis=Cs(CS_ARCH_RISCV,CS_MODE_RISCV64 if xlen==64 else CS_MODE_RISCV32)
        BASE=0x10000
        REG=0x20000 if xlen==32 else 0x100020000
        DATA=0x30000 if xlen==32 else 0x100030000
        S,M,D=DATA+0x104,DATA+0x204,DATA+0x304 # deliberately only 4-byte aligned
        PC=0x45678 if xlen==32 else 0x23456789abc
        OLDPC=0x43210 if xlen==32 else 0x3456789abcd
        ptrfmt='<I' if xlen==32 else '<Q'
        mask=(1<<(wb*8))-1
        code={name:emit(name,abi)[1] for name in names}

        def run(name, source=b'\x00'*8, middle=b'\x00'*8, destination=b'\xa5'*16,
                sptr=S, mptr=M, dptr=D):
            blob=code[name]
            assert blob, name
            u=Uc(UC_ARCH_RISCV,UC_MODE_RISCV64 if xlen==64 else UC_MODE_RISCV32)
            for address in (BASE,REG,DATA): u.mem_map(address,0x1000)
            u.mem_write(BASE,blob)
            u.mem_write(REG,struct.pack(ptrfmt,OLDPC))
            for offset,value in [(abi.s,sptr),(abi.m,mptr),(abi.d,dptr)]:
                u.mem_write(REG+offset,struct.pack(ptrfmt,value))
            u.mem_write(D-8,b'\xa5'*32)
            u.mem_write(dptr,destination)
            u.mem_write(sptr,source)
            u.mem_write(mptr,middle)
            preserved=[getattr(regs,'UC_RISCV_REG_'+n) for n in
                       ['SP','GP','TP']+['S'+str(i) for i in range(12)]]
            for r in preserved: u.reg_write(r,0xabc000+r*16)
            u.reg_write(regs.UC_RISCV_REG_A0,REG)
            u.reg_write(regs.UC_RISCV_REG_RA,BASE+len(blob))
            def aligned_access(uc, access, address, size, value, user):
                assert address % size == 0, (name, hex(address), size)
            u.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, aligned_access)
            u.emu_start(BASE,BASE+len(blob),count=100)
            for r in preserved: assert u.reg_read(r)==0xabc000+r*16, (name,'ABI clobber',r)
            return u,bytes(u.mem_read(dptr,16))

        for field in ('pc','s','d','m'):
            invalid=Layout(xlen,wb,1,0,pb,pb*2,pb*3)
            setattr(invalid,field,getattr(invalid,field)+1)
            assert emit('IADDW',invalid)[0]==0
            checks+=1
        for bits in (0,1,0x8000000000000000,0x3ff0000000000000,
                     0x7ff0000000000000,0xfff0000000000000,
                     0x7ff8000000000042,0x7ff0000000000042):
            for destination in (D,S,S+4):
                u,result=run('INEGF',struct.pack('<Q',bits),dptr=destination)
                assert int.from_bytes(result[:8],'little')==bits^(1<<63)
                assert result[8:]==b'\xa5'*8
                assert int.from_bytes(u.mem_read(REG,pb),'little')==OLDPC
                checks+=1
        arithmetic=['IADDW' ,'ISUBW','IMULW','IANDW','IORW','IXORW',
                    'IADDB','ISUBB','IMULB','IANDB','IORB','IXORB']
        vectors=[(0,0),(1,-1),(-1,1),(2**31-1,1),(-2**31,-1),
                 (2**63-1,-2**63),(0x1122334455667788,0x8877665544332211)]
        vectors += [(rng.getrandbits(64),rng.getrandbits(64)) for _ in range(30)]
        for name in arithmetic:
            bits=8 if name.endswith('B') else wb*8
            width=bits//8
            for a,b in vectors:
                u,result=run(name,struct.pack('<Q',a&((1<<64)-1)),struct.pack('<Q',b&((1<<64)-1)))
                a&=(1<<bits)-1; b&=(1<<bits)-1
                operation=name[1:-1]
                want={'ADD':lambda:b+a,'SUB':lambda:b-a,'MUL':lambda:b*a,
                      'AND':lambda:b&a,'OR':lambda:b|a,'XOR':lambda:b^a}[operation]()&((1<<bits)-1)
                assert int.from_bytes(result[:width],'little')==want,(xlen,wb,name,a,b,result,want)
                assert result[width:]==b'\xa5'*(16-width), 'store crossed value boundary'
                assert u.reg_read(regs.UC_RISCV_REG_A0)==1
                checks+=1
        # Word-width opcode selection and byte unsigned-load encoding oracle.
        mnemonics=[i.mnemonic for i in dis.disasm(code['IADDW'],BASE)]
        assert ('addw' if xlen==64 and wb==4 else 'add') in mnemonics, mnemonics
        assert 'lbu' in [i.mnemonic for i in dis.disasm(code['IADDB'],BASE)]
        for name in ('ISHLW','ISHRW','ILSRW','ISHLB','ISHRB'):
            width=1 if name.endswith('B') else wb
            bits=8 if width==1 else wb*8
            countbits=bits
            for count in (-1,0,1,7,8,31,32,63,64,65):
                b=0xf123456789abcdef&((1<<bits)-1)
                u,result=run(name,struct.pack('<Q',count&mask),struct.pack('<Q',b))
                success=0<=count<countbits
                assert u.reg_read(regs.UC_RISCV_REG_A0)==int(success),(name,count)
                if success:
                    want=(b<<count if 'SHL' in name else
                          signed(b,bits)>>count if name=='ISHRW' else b>>count)&((1<<bits)-1)
                    assert int.from_bytes(result[:width],'little')==want,(xlen,wb,name,count,result,want)
                    assert result[width:]==b'\xa5'*(16-width)
                else: assert result==b'\xa5'*16, 'fallback changed memory'
                checks+=1
        relations={'EQ':lambda a,b:a==b,'NE':lambda a,b:a!=b,'LT':lambda a,b:a<b,
                   'LE':lambda a,b:a<=b,'GT':lambda a,b:a>b,'GE':lambda a,b:a>=b}
        for suffix in ('B','W'):
            bits=8 if suffix=='B' else wb*8
            for rel,predicate in relations.items():
                name=('IBNEW' if rel=='NE' and suffix=='W' else 'IB'+rel+suffix)
                for a,b in vectors[:7]:
                    u,_=run(name,struct.pack('<Q',a&((1<<64)-1)),struct.pack('<Q',b&((1<<64)-1)),
                            struct.pack(ptrfmt,PC)+b'\xa5'*(16-pb))
                    a=a&255 if bits==8 else signed(a,bits)
                    b=b&255 if bits==8 else signed(b,bits)
                    pc=int.from_bytes(u.mem_read(REG,pb),'little')
                    assert pc==(PC if predicate(a,b) else OLDPC),(xlen,wb,name,a,b,hex(pc))
                    checks+=1
        for name,width in [('IMOVB',1),('IMOVW',wb),('IMOVL',8),('IMOVF',8),
                           ('ICVTBW',wb),('ICVTWB',1),('ICVTWS',2),('ICVTSW',wb)]:
            raw=bytes.fromhex('efcdab8967452301')
            u,result=run(name,raw)
            want=(raw[:1]+b'\0'*(wb-1) if name=='ICVTBW' else
                  (int.from_bytes(raw[:2],'little',signed=True)&mask).to_bytes(wb,'little')
                  if name=='ICVTSW' else raw[:width])
            assert result[:width]==want,(xlen,wb,name,result,want)
            assert result[width:]==b'\xa5'*(16-width)
            checks+=1
        # Complete capture before overlapping 8-byte stores.
        for name in ('IMOVL','IMOVF'):
            raw=bytes.fromhex('efcdab8967452301')
            u,result=run(name,raw,sptr=D-4)
            assert result[:8]==raw,(name,'partial overlap',result)
            checks+=1
        u,_=run('IJMP',destination=struct.pack(ptrfmt,PC)+b'\xa5'*(16-pb))
        assert int.from_bytes(u.mem_read(REG,pb),'little')==PC
        checks+=1
        for name in names:
            n,_=emit(name,abi)
            if n:
                assert len(list(dis.disasm(code[name],BASE)))==n,(name,'invalid instruction')
                assert emit(name,abi,cap=n-1)[0]==0,(name,'capacity check')
                assert lib.emit(None,64,C.byref(abi),ops[name])==n
                checks+=1
        for name in ('IMOVP','IFRAME','ICALL','IRET','IADDF'):
            assert not code[name], name
            checks+=1
        assert not code['IEXPF']
        checks+=1
        if xlen==32:
            for name in ('IBEQF','IBNEF','IBLTF','IBLEF','IBGTF','IBGEF'):
                assert not code[name]
                checks+=1
            assert not code['IHEADM']
            checks+=1
            assert not code['IEXPW'] and not code['IEXPL']
            checks+=1
            assert not code['IDIVW'] and not code['IMODW']
        no_m=Layout(xlen,wb,0,0,pb,pb*2,pb*3)
        assert emit('IMULW',no_m)[0]==emit('IMULB',no_m)[0]==0
        assert emit('IDIVW',no_m)[0]==emit('IMODW',no_m)[0]==0
        for name in ('IBEQF','IBNEF','IBLTF','IBLEF','IBGTF','IBGEF'):
            assert bool(emit(name,no_m)[0])==(xlen==64)
            checks+=1
        assert bool(emit('IHEADM',no_m)[0])==(xlen==64)
        checks+=1
        assert bool(emit('ICVTXX',no_m)[0]) == (xlen==64)
        assert emit('IEXPW',no_m)[0]==emit('IEXPL',no_m)[0]==0
        checks+=1
        # In-place source/destination keeps both operands until the store.
        u,result=run('ISUBW',struct.pack('<Q',3),struct.pack('<Q',7),sptr=D)
        assert int.from_bytes(result[:wb],'little')==4
        checks+=1
    bad=Layout(32,8,1,0,4,8,12)
    assert emit('IADDW',bad)[0]==0
print(f'PASS RISC-V leaf JIT: {checks} checks across RV32/word32, RV64/word32, RV64/legacy-word64')
