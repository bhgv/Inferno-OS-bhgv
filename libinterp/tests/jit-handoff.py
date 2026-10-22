#!/usr/bin/env python3
"""Execute production native->helper handoff with operand/PC/exit checks.

Projected 32-bit REG, real emitters/literal pools and native instructions; C
callback and exception range selection are boundaries, not a VM/OS boot.
"""
import ctypes as C
import importlib.util
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE,UcError
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_EBX
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R5,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R12,UC_ARM_REG_LR,UC_ARM_REG_SP
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('lifetime',Path(__file__).with_name('jit-lifetime.py'))
model=importlib.util.module_from_spec(spec)
previous_bytecode=sys.dont_write_bytecode
sys.dont_write_bytecode=True
try:spec.loader.exec_module(model)
finally:sys.dont_write_bytecode=previous_bytecode

def unit(arch):
    source=(root/f'libinterp/comp-{arch}.c').read_text()
    if arch=='386':
        header=source[source.index('enum\n'):source.index('static\tuchar*\tcode;')]
        state=r'''
#define DOT ((ulong)(uintptr_t)code)
static uchar *code,*base;static ulong *litpool,*patch;
static int pass,nlit;
'''
        names=[('bc','static int')]+[(n,'static void') for n in ['genb','gen2','genw','modrm','con','bra','opwld','opwst','literal','punt']]
        initialize='code=base+0x100;litpool=(ulong*)(base+2048);'
        finish=''
    else:
        header=source[source.index('#define\tRESCHED'):source.index('static\tulong*\tcode;')]
        state=source[source.index('typedef struct Const'):source.index('static Con rcon;')+len('static Con rcon;')]+r'''
static ulong *code,*base,*patch,*litpool;static int pass,nlit,codeoff;
static void flushchk(void);static void gen(ulong);
'''
        names=[(n,'static '+r) for n,r in [('gen','void'),('immrot','long'),('flushcon','void'),('flushchk','void'),('ccon','void'),('con','void'),('armcall','void'),('memc','void'),('mem','void'),('opx','void'),('opwld','void'),('opwst','void'),('literal','void'),('punt','void')]]
        initialize='code=base+0x100/4;litpool=base+2048/4;codeoff=0;rcon.ptr=0;'
        finish='flushcon(1);'
    program=r'''
typedef uintptr_t uintptr;
#define urk(...) abort()
typedef union Adr {int32_t imm,ind;struct {uint16_t f,s;} i;struct Inst *ins;} Adr;
typedef struct Inst {uint8_t op,add;uint16_t reg;Adr s,d;} Inst;
typedef struct Module {Inst *prog;int nprog;} Module;
static Module module,*mod=&module;
static Inst original[2];
static union {ulong align;uchar bytes[4096];} out;
static ulong offsets[3];
'''
    validator=(root/'libinterp/jit-validate.h').read_text()
    code=model.function(validator,'jitvalidpc','static int')
    code+='\n'+'\n'.join(model.function(source,n,r) for n,r in names)
    if arch=='arm':offsets='offsets[0]=0;offsets[1]=0x100/4;offsets[2]=0x300/4;'
    else:offsets='offsets[0]=0;offsets[1]=0x100;offsets[2]=0x300;'
    return model.common+header+state+program+code+r'''
unsigned build(int mode,int mask,int synthetic){
 Inst local,*i=synthetic?&local:&original[1];int sizes[2];
 memset(i,0,sizeof(*i));i->op=IDIVW;
 i->add=mode==1?SRC(AMP)|DST(AFP)|AXINM:
        mode==2?SRC(AIMM)|DST(AFP)|AXIMM:
        mode==3?SRC(AFP)|DST(AFP)|AXNON:
        mode>=4?SRC(AIND|AFP)|DST(AIND|AMP)|AXINF:SRC(AFP)|DST(AFP)|AXINF;
 i->s.ind=mode==2?-129:mode>=4?(0x20<<16)|(mode==5?0xfffc:0x40):0x100;
 i->d.ind=mode>=4?(0x30<<16)|0x48:0x300;i->reg=mode==2?(uint16_t)-17:0x200;
 module.prog=original;module.nprog=2;patch=offsets;OFFSETS
 base=(void*)out.bytes;
 for(pass=0;pass<2;pass++){
  memset(out.bytes,0,sizeof(out.bytes));nlit=0;INITIALIZE
  punt(i,mask,rdestroy);FINISH
  sizes[pass]=(uchar*)code-out.bytes;
 }
 assert(sizes[0]==sizes[1] && sizes[1]<0x300);
 return sizes[1];
}
void *image(void){return out.bytes;}
unsigned regaddr(void){return (uint32_t)(uintptr_t)&R;}
unsigned callback(void){return (uint32_t)(uintptr_t)rdestroy;}
'''.replace('OFFSETS',offsets).replace('INITIALIZE',initialize).replace('FINISH',finish)
checks=0
with tempfile.TemporaryDirectory(prefix='jit-handoff-') as td:
    td=Path(td)
    for arch in ['386','arm']:
        src=td/f'{arch}.c';src.write_text(unit(arch));binary=td/f'{arch}.so'
        flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+flags+['-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-shared','-fPIC','-I'+str(root/'include'),str(src),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary));lib.image.restype=C.c_void_p
        for mode in range(6):
            for outcome in (['decode-fault'] if mode==5 else ['normal','newpc','yield','check-zero','error','newpc-yield','newpc-check-zero']):
                for synthetic in [False,True]:
                    mask=1|2|64|(16 if outcome.startswith('newpc') else 0)|(8 if outcome in ['yield','check-zero','newpc-yield','newpc-check-zero'] else 0)
                    # IA-32/ARM TCHECK=(1<<3), WRTPC=(1<<2), NEWPC=(1<<4).
                    size=lib.build(mode,mask,int(synthetic));host=lib.image();BASE=C.c_uint32(host).value
                    reg=lib.regaddr()&0xffffffff;callback=lib.callback()&0xffffffff
                    FRAME,MP,NEWFRAME,NEWMP,STACK,STOP=0x200000,0x300000,0x400000,0x500000,0x800000,0xa00000
                    image=C.string_at(host,4096);state=bytearray(60)
                    struct.pack_into('<I',state,0,BASE+4);struct.pack_into('<I',state,4,MP);struct.pack_into('<I',state,8,FRAME)
                    struct.pack_into('<I',state,32,STOP)
                    frame=bytearray(b'\xa5'*4096);module=bytearray(b'\xa5'*4096)
                    source=(MP if mode==1 else FRAME)+0x100;middle=(MP if mode==1 else FRAME)+0x200;dest=FRAME+0x300
                    struct.pack_into('<I',frame,0x100,3);struct.pack_into('<I',frame,0x200,7);struct.pack_into('<I',frame,0x300,11)
                    struct.pack_into('<I',module,0x100,3);struct.pack_into('<I',module,0x200,7)
                    if mode==2:av,bv=-129,-17
                    else:av,bv=3,11 if mode==3 else 7
                    if mode==3:middle=dest
                    if mode==4:
                        source=FRAME+0x520;dest=MP+0x530
                        struct.pack_into('<I',frame,0x40,FRAME+0x500);struct.pack_into('<I',module,0x48,MP+0x500)
                        struct.pack_into('<I',frame,0x520,3);struct.pack_into('<I',module,0x530,11)
                    stub=b'\xc3' if arch=='386' else struct.pack('<II',0xe92d4000,0xe8bd8000)
                    u=Uc(UC_ARCH_X86,UC_MODE_32) if arch=='386' else Uc(UC_ARCH_ARM,UC_MODE_ARM)
                    model.mapregions(u,[(BASE,image),(reg,bytes(state)),(FRAME,bytes(frame)),(MP,bytes(module)),(NEWFRAME,bytes(4096)),(NEWMP,bytes(4096)),(STACK,bytes(4096)),(STOP,bytes(16)),(callback,stub)])
                    calls=[]
                    def word(addr):return struct.unpack('<I',u.mem_read(addr,4))[0]
                    def called(uc,address,length,data):
                        if address!=(callback if arch=='386' else callback+4):return
                        calls.append(address)
                        savedpc=word(reg);expectedpc=BASE+4 if synthetic else BASE+0x300
                        assert savedpc==expectedpc,(arch,mode,outcome,synthetic,'stale PC',hex(savedpc),hex(expectedpc))
                        assert word(reg+8)==FRAME
                        # handler() uses next-PC minus one for compiled code.
                        if not synthetic:assert 0x100<=savedpc-BASE-1<0x300
                        sp,dp,mp=word(reg+36),word(reg+40),word(reg+44)
                        if mode!=2:assert (sp,dp,mp)==(source,dest,middle),(arch,mode,'operands')
                        assert word(sp)==av&0xffffffff and word(mp)==bv&0xffffffff
                        if outcome=='error':uc.emu_stop();return
                        uc.mem_write(dp,struct.pack('<I',(av+bv)&0xffffffff))
                        uc.mem_write(NEWFRAME,bytes(uc.mem_read(FRAME,4096)));uc.mem_write(reg+8,struct.pack('<I',NEWFRAME))
                        uc.mem_write(reg+4,struct.pack('<I',NEWMP));uc.mem_write(reg+48,struct.pack('<I',int(outcome in ['yield','newpc-yield'])))
                        if outcome.startswith('newpc'):uc.mem_write(reg,struct.pack('<I',0xdead0000 if outcome=='newpc-yield' else STOP))
                        volatile=[UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX] if arch=='386' else [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R12,UC_ARM_REG_LR]
                        for r in volatile:uc.reg_write(r,0xdeadbeef)
                    u.hook_add(UC_HOOK_CODE,called)
                    if arch=='386':
                        u.reg_write(UC_X86_REG_ESI,FRAME);u.reg_write(UC_X86_REG_EDI,MP);u.reg_write(UC_X86_REG_ESP,STACK+2048)
                        u.mem_write(STACK+2048,struct.pack('<6I',0x10,0x20,0x30,0x40,0x50,STOP))
                    else:
                        u.reg_write(UC_ARM_REG_R5,reg);u.reg_write(UC_ARM_REG_R9,FRAME);u.reg_write(UC_ARM_REG_R8,MP);u.reg_write(UC_ARM_REG_LR,STOP);u.reg_write(UC_ARM_REG_SP,STACK+2048)
                    end=STOP if outcome.startswith('newpc') or outcome=='yield' else BASE+size
                    if outcome=='decode-fault':
                        try:u.emu_start(BASE+0x100,end,count=1000)
                        except UcError:pass
                        else:raise AssertionError('invalid operand did not fault')
                        assert not calls
                        assert word(reg)==(BASE+4 if synthetic else BASE+0x300),'PC was saved after operand decoding'
                        checks+=1;continue
                    u.emu_start(BASE+0x100,end,count=1000)
                    assert len(calls)==1
                    assert word(dest)==(11 if outcome=='error' else (av+bv)&0xffffffff)
                    if outcome not in ['error','yield','newpc-yield']:
                        assert u.reg_read(UC_X86_REG_ESI if arch=='386' else UC_ARM_REG_R9)==NEWFRAME
                        assert u.reg_read(UC_X86_REG_EDI if arch=='386' else UC_ARM_REG_R8)==NEWMP
                    if outcome in ['yield','newpc-yield'] and arch=='386':
                        assert u.reg_read(UC_X86_REG_ESP)==STACK+2048+24
                        for register,value in [(UC_X86_REG_EDI,0x10),(UC_X86_REG_ESI,0x20),(UC_X86_REG_EDX,0x30),(UC_X86_REG_ECX,0x40),(UC_X86_REG_EBX,0x50)]:assert u.reg_read(register)==value
                    checks+=1
print(f'PASS JIT handoff: {checks} executed x86/ARM normal/error/NEWPC/TCHECK cases, exact operands, current-PC exception ranges, synthetic macros, clobbers and frame/MP restore')
