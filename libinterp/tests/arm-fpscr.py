#!/usr/bin/env python3
"""Production ARM FPSCR assembly plus Linux/Android Math control/status bridges.

Needs host cc, Keystone, Capstone and Unicorn. Target OS/trap delivery are not
emulated. Unicorn's VFP trap enables are RAZ: requested VMSR values are checked
separately, and all Math trap-mask combinations use a native-register model.
"""
import ctypes as C
import os
from pathlib import Path
import random
import re
import shlex
import struct
import subprocess
import tempfile
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
from keystone import Ks, KS_ARCH_ARM, KS_MODE_ARM, KS_MODE_THUMB
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_MODE_THUMB, UC_HOOK_CODE
import unicorn.arm_const as reg
root=Path(__file__).resolve().parents[2]
cc=shlex.split(os.environ.get('CC','cc'))
flags=shlex.split(os.environ.get('CFLAGS','-O1 -g'))
control=0x00c01f00
status=0x1f
checks=0
rng=random.Random(0xf05c2)

def check(ok):
    global checks
    assert ok
    checks+=1

def preprocess(path,defines):
    return subprocess.check_output(cc+['-E','-P','-x','assembler-with-cpp']+
        ['-D'+d for d in defines]+[str(path)],text=True)

def bodies(text):
    entries=list(re.finditer(r'\.global\s+(\w+);(?:\s*\.thumb_func;)?\s*\1:',text))
    result={}
    for i,m in enumerate(entries):
        end=entries[i+1].start() if i+1<len(entries) else len(text)
        lines=text[m.end():end].splitlines()
        result[m.group(1)]='\n'.join(line.strip() for line in lines
            if line.strip() and not line.strip().startswith('.'))
    return result

class Machine:
    def __init__(self,code,thumb=False):
        self.thumb=thumb
        self.u=Uc(UC_ARCH_ARM,UC_MODE_THUMB if thumb else UC_MODE_ARM)
        self.u.mem_map(0x1000,0x2000)
        self.u.reg_write(reg.UC_ARM_REG_C1_C0_2,0xf<<20)
        self.u.reg_write(reg.UC_ARM_REG_FPEXC,1<<30)
        self.code=code
        self.writes=[]
        dis=Cs(CS_ARCH_ARM,CS_MODE_THUMB if thumb else CS_MODE_ARM)
        self.dis=dis
        def before(u,pc,size,_):
            ins=next(dis.disasm(bytes(u.mem_read(pc,size)),pc))
            if ins.mnemonic=='vmsr':
                target=ins.op_str.split(',')[1].strip()
                self.writes.append(u.reg_read(getattr(reg,'UC_ARM_REG_'+target.upper())))
        self.u.hook_add(UC_HOOK_CODE,before)
    def fpscr(self,value=None):
        if value is not None:self.u.reg_write(reg.UC_ARM_REG_FPSCR,value)
        return self.u.reg_read(reg.UC_ARM_REG_FPSCR)
    def call(self,name,arg=0):
        self.writes=[]
        self.u.mem_write(0x1000,self.code[name])
        # Replaced code at the same address needs a fresh translated block.
        self.u.ctl_remove_cache(0x1000,0x1100)
        self.u.reg_write(reg.UC_ARM_REG_R0,arg & 0xffffffff)
        self.u.reg_write(reg.UC_ARM_REG_LR,0x1801 if self.thumb else 0x1800)
        self.u.emu_start(0x1001 if self.thumb else 0x1000,0x1800,count=100)
        return self.u.reg_read(reg.UC_ARM_REG_R0)
    def divide(self,a,b):
        self.u.reg_write(reg.UC_ARM_REG_D0,struct.unpack('<Q',struct.pack('<d',a))[0])
        self.u.reg_write(reg.UC_ARM_REG_D1,struct.unpack('<Q',struct.pack('<d',b))[0])
        self.call('divide')
        return self.u.reg_read(reg.UC_ARM_REG_D0)

bridge=r'''
#include "lib9.h"
#include "mathi.h"
static ulong (*readcr)(void), (*readsr)(void);
static void (*writecr)(ulong), (*writesr)(ulong);
void bridge(ulong (*gcr)(void), ulong (*gsr)(void), void (*scr)(ulong), void (*ssr)(ulong))
{readcr=gcr;readsr=gsr;writecr=scr;writesr=ssr;}
ulong getfcr(void) {return readcr();}
ulong getfsr(void) {return readsr();}
void setfcr(ulong x) {writecr(x);}
void setfsr(ulong x) {writesr(x);}
'''
Get=C.CFUNCTYPE(C.c_ulong)
Set=C.CFUNCTYPE(None,C.c_ulong)

with tempfile.TemporaryDirectory(prefix='arm-fpscr-') as directory:
    td=Path(directory);unit=td/'bridge.c';unit.write_text(bridge)
    for arch in ['Linux','Android']:
        shared=td/(arch+'.so')
        subprocess.run(cc+flags+['-shared','-fPIC','-I'+str(root/arch/'arm/include'),
            '-I'+str(root/'include'),str(unit),str(root/'libmath'/('FPcontrol-'+arch+'.c')),
            '-o',str(shared)],check=True)
        lib=C.CDLL(str(shared))
        lib.bridge.argtypes=[Get,Get,Set,Set]
        for name in ['getFPcontrol','getFPstatus']:
            getattr(lib,name).restype=C.c_ulong
        for name in ['FPcontrol','FPstatus']:
            getattr(lib,name).argtypes=[C.c_ulong,C.c_ulong]
            getattr(lib,name).restype=C.c_ulong
        # Independent architecture reference, not the production header values.
        enables=[1<<8,1<<9,1<<10,1<<11,1<<12]
        rounds=[0,2<<22,1<<22,3<<22] # Math NR,NINF,PINF,Z
        native=[0]
        callbacks=[Get(lambda:native[0]&control),Get(lambda:native[0]&status),
            Set(lambda x:native.__setitem__(0,(native[0]&~control)|(x&control))),
            Set(lambda x:native.__setitem__(0,(native[0]&~status)|(x&status)))]
        lib.bridge(*callbacks)
        for rounding in range(4):
            for traps in range(32):
                initial=0xa900809f | rounds[rounding] | sum(enables[i] for i in range(5) if traps&(1<<i))
                native[0]=initial
                check(lib.getFPcontrol()==(rounding<<8)|traps)
                for mask in [0,1,2,4,8,16,31,0x100,0x200,0x300,0x31f]:
                    requested=(rng.randrange(4)<<8)|rng.randrange(32)
                    native[0]=initial
                    old=(rounding<<8)|traps
                    merged=(requested&mask)|(old&~mask)
                    check(lib.FPcontrol(requested,mask)==old&mask)
                    expected=rounds[(merged>>8)&3] | sum(enables[i] for i in range(5) if merged&(1<<i))
                    check(native[0]==(initial&~control)|expected)
        for old in range(32):
            for desired in range(32):
                for mask in [0,1,2,4,8,16,31]:
                    native[0]=0xffc09f80|old
                    check(lib.FPstatus(desired,mask)==old&mask)
                    check(native[0]==0xffc09f80|((desired&mask)|(old&~mask)))
        native[0]=0xffc09f9f
        lib.FPinit()
        check(native[0]==(0xffc09f9f&~(control|status))|0xf00)
        for defines,vfp in [([],False),(['__ARM_FP=0'],False),(['__ARM_FP=4'],True),
                            (['__ARM_FP=8'],True),(['__ARM_FP=12'],True),
                            (['USEVFP=1'],True),(['__ARM_FP=12','USEVFP=0'],False),
                            (['__ARM_FP=12','ARM_FPSCR_VFP=0'],False)]:
            for thumb in [False,True]:
                text=preprocess(root/'lib9'/('setfcr-'+arch+'-arm.S'),defines+(['__thumb__=1'] if thumb else []))
                functions=bodies(text)
                ks=Ks(KS_ARCH_ARM,KS_MODE_THUMB if thumb else KS_MODE_ARM)
                code={name:bytes(ks.asm(body,addr=0x1000)[0]) for name,body in functions.items()}
                code['divide']=bytes(ks.asm('vdiv.f64 d0,d0,d1; bx lr',addr=0x1000)[0])
                machine=Machine(code,thumb)
                for _ in range(30):
                    initial=machine.fpscr(rng.getrandbits(32))
                    arg=rng.getrandbits(32)
                    check(machine.call('getfcr',arg)==(initial&control if vfp else 0))
                    check(machine.call('getfsr',arg)==(initial&status if vfp else 0))
                    machine.call('setfcr',arg)
                    expected=(initial&~control)|(arg&control) if vfp else initial
                    if vfp:check(machine.writes==[expected])
                    # Unicorn models trap enables as read-as-zero.
                    check(machine.fpscr()==(expected&~0x9f00))
                    initial=machine.fpscr()
                    machine.call('setfsr',arg)
                    expected=(initial&~status)|(arg&status) if vfp else initial
                    check(machine.fpscr()==expected)
                if vfp:
                    callbacks=[Get(lambda:machine.call('getfcr')),Get(lambda:machine.call('getfsr')),
                               Set(lambda x:machine.call('setfcr',x)),Set(lambda x:machine.call('setfsr',x))]
                    lib.bridge(*callbacks)
                    # Real Math -> real assembly -> VFP arithmetic, including flags.
                    positive=[0x3fb999999999999a,0x3fb9999999999999,0x3fb999999999999a,0x3fb9999999999999]
                    negative=[0xbfb999999999999a,0xbfb999999999999a,0xbfb9999999999999,0xbfb9999999999999]
                    for rounding in range(4):
                        machine.fpscr(0)
                        lib.FPcontrol(rounding<<8,0x300)
                        check(lib.getFPcontrol()==rounding<<8)
                        lib.FPstatus(0,31)
                        check(machine.divide(1.,10.)==positive[rounding])
                        check(lib.getFPstatus()==16)
                        lib.FPstatus(0,31)
                        check(lib.getFPcontrol()==rounding<<8)
                        check(machine.divide(-1.,10.)==negative[rounding])
                    for a,b,flag in [(0.,0.,1),(1.,0.,2),(1e308,1e-308,4),(1e-308,1e308,8)]:
                        machine.fpscr(0);lib.FPstatus(0,31);machine.divide(a,b)
                        check(lib.getFPstatus()&flag==flag)
print(f'PASS ARM FPSCR: {checks} mask, merge, A32/Thumb, soft/VFP selection and Math-to-VFP arithmetic checks')
