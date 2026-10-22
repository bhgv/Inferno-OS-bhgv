#!/usr/bin/env python3
"""Execute production frame allocation and module-call encoders in Unicorn.

ARM32/IA-32 layout projections; extend/rmcall bodies are recording boundaries.
No OS boot, target C stack allocator or actual module invocation is performed.
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
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE,UC_HOOK_MEM_READ
import unicorn.x86_const as x
import unicorn.arm_const as a
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('lifetime',Path(__file__).with_name('jit-lifetime.py'))
model=importlib.util.module_from_spec(spec);previous=sys.dont_write_bytecode;sys.dont_write_bytecode=True
try:spec.loader.exec_module(model)
finally:sys.dont_write_bytecode=previous

def unit(arch):
 s=(root/f'libinterp/comp-{arch}.c').read_text()
 common=model.common.replace('static void urk(char *s){(void)s;abort();}','#define urk(...) abort()')
 common=common.replace('typedef struct Type {int size,np;void *initialize,*destroy;uchar map[1];} Type;','typedef struct Type {int size,np;uint32_t initialize,destroy;} Type;')
 common+=r'''
typedef struct Frame {uint32_t lr,fp,mr,t;} Frame;
typedef struct Modl {union {uint32_t pc;} u;uint32_t frame;} Modl;
typedef struct Modlink {uint32_t MP,m,compiled,prog,type,data,nlinks;Modl links[1];} Modlink;
typedef union Adr {int32_t imm,ind;struct {uint16_t f,s;} i;struct Inst *ins;} Adr;
typedef struct Inst {uint8_t op,add;uint16_t reg;Adr s,d;} Inst;
#define OA(t,f) O(t,f)
static void __attribute__((aligned(16))) extend(void){}
static void __attribute__((aligned(16))) rmcall(void){}
static void __attribute__((aligned(16))) cold(void){}
static void (*optab[256])(void);
static void punt(Inst*,int,void(*)(void));
static struct {Inst *prog;} module,*mod=&module;
static Inst program[2];
static union {uint32_t align;uchar bytes[4096];} out;
static ulong *patch,offsets[3],macro[NMACRO];
'''
 if arch=='386':
  header=s[s.index('enum\n'):s.index('static\tuchar*\tcode;')]
  state='static uchar *code,*base;\n#define DOT ((ulong)(uintptr_t)code)\n'
  names=[('bc','static int')]+[(n,'static void') for n in ['genb','gen2','genw','modrm','con','bra','rbra','cmpl','opwld','opwst','mid','macfram','modguard','commcall','macmcal']]
  setup='code=base=out.bytes;'
  emit='if(kind==0)macfram();else if(kind==2)modguard(i,0);else {commcall(i);code=base+0x400;macmcal();}'
  extra=''
 else:
  header=s[s.index('#define\tRESCHED'):s.index('static\tulong*\tcode;')]
  state=s[s.index('typedef struct Const'):s.index('static Con rcon;')+len('static Con rcon;')]+r'''
static ulong *code,*base;static int pass,codeoff;static void flushchk(void);static void gen(ulong);
'''
  names=[(n,'static '+r) for n,r in [('gen','void'),('immrot','long'),('flushcon','void'),('flushchk','void'),('ccon','void'),('con','void'),('armcall','void'),('memc','void'),('mem','void'),('opx','void'),('opwld','void'),('opwst','void'),('mid','void'),('macfram','void'),('modguard','void'),('commcall','void'),('macmcal','void')]]
  setup='code=base=(ulong*)out.bytes;pass=1;codeoff=0;rcon.ptr=0;'
  emit='if(kind==0)macfram();else if(kind==2)modguard(i,0);else {commcall(i);flushcon(1);code=base+0x400/4;macmcal();}'
  extra='flushcon(1);'
 stub='static void punt(Inst *i,int mask,void(*fn)(void)){(void)i;(void)mask;(void)fn;'+('bra((ulong)cold,Ocall);' if arch=='386' else 'CALL(cold);')+'}\n'
 # common contains NMACRO globals, so place it after architecture enums.
 return common[:common.index('static ulong *patch')]+header+state+common[common.index('static ulong *patch'):]+ '\n'.join(model.function(s,n,r) for n,r in names)+stub+r'''
int build(int kind,int mode,int linkindex){
 Inst *i=&program[0];memset(out.bytes,0,sizeof out.bytes);memset(i,0,sizeof(*i));module.prog=program;
 i->add=mode==1?SRC(AMP)|DST(AMP)|AXIMM:mode==2?SRC(AIND|AFP)|DST(AIND|AMP)|AXIMM:SRC(AFP)|DST(AFP)|AXIMM;
 i->op=kind==2?IMFRAME:IMCALL;i->reg=linkindex;i->s.ind=mode==2?(0x20<<16)|0x40:0x100;i->d.ind=mode==2?(0x30<<16)|0x48:0x200;
 patch=offsets;offsets[1]=ARCHNEXT;macro[MacMCAL]=ARCHMAC;
 SETUP
 EMIT
 EXTRA
 assert((uchar*)code-out.bytes<4096);return (uchar*)code-out.bytes;
}
void *image(void){return out.bytes;}
unsigned regaddr(void){return (uint32_t)(uintptr_t)&R;}
unsigned callback(int which){return (uint32_t)(uintptr_t)(which==2?cold:which?rmcall:extend);}
'''.replace('ARCHNEXT','0x300' if arch=='386' else '0x300/4').replace('ARCHMAC','0x400' if arch=='386' else '0x400/4').replace('SETUP',setup).replace('EMIT',emit).replace('EXTRA',extra)
checks=0
with tempfile.TemporaryDirectory(prefix='jit-frames-') as td:
 td=Path(td)
 for arch in ['386','arm']:
  src=td/f'{arch}.c';src.write_text(unit(arch));binary=td/f'{arch}.so'
  subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+['-shared','-fPIC','-I'+str(root/'include'),str(src),'-o',str(binary)],check=True)
  lib=C.CDLL(str(binary));lib.image.restype=C.c_void_p;lib.regaddr.restype=C.c_uint32;lib.callback.restype=C.c_uint32
  BASE=C.c_uint32(lib.image()).value;REG=lib.regaddr();FP,MP,TYPE,INIT,STOP,CSTACK,NEW=0x20000,0x30000,0x40000,0x50000,0x60000,0x70000,0x80000
  def setup(kind,mode=0,sp=0x90000,linkindex=0):
   size=lib.build(kind,mode,linkindex);u=Uc(UC_ARCH_X86,UC_MODE_32) if arch=='386' else Uc(UC_ARCH_ARM,UC_MODE_ARM)
   callback=lib.callback(kind)
   regions=[(BASE,C.string_at(lib.image(),size)),(REG,b'\0'*64),(FP,b'\xa5'*4096),(MP,b'\xa5'*4096),(TYPE,b'\0'*4096),(INIT,b'\xc3' if arch=='386' else struct.pack('<I',0xe1a0f00e)),(STOP,b'\0'*4),(CSTACK,b'\0'*4096),(NEW,b'\0'*4096),(sp&~4095,b'\xa5'*4096),(callback,b'\xc3' if arch=='386' else struct.pack('<I',0xe1a0f00e)),(lib.callback(2),b'\xc3' if arch=='386' else struct.pack('<I',0xe1a0f00e))]
   model.mapregions(u,regions)
   def write(address,value):u.mem_write(address,struct.pack('<I',value&0xffffffff))
   write(REG+4,MP);write(REG+8,FP);write(REG+12,sp)
   if arch=='386':
    u.reg_write(x.UC_X86_REG_ESI,FP);u.reg_write(x.UC_X86_REG_EDI,MP);u.reg_write(x.UC_X86_REG_ESP,CSTACK+0x800);u.reg_write(x.UC_X86_REG_EDX,TYPE);write(CSTACK+0x800,STOP)
   else:
    u.reg_write(a.UC_ARM_REG_R9,FP);u.reg_write(a.UC_ARM_REG_R8,MP);u.reg_write(a.UC_ARM_REG_R5,REG);u.reg_write(a.UC_ARM_REG_R4,TYPE);u.reg_write(a.UC_ARM_REG_LR,STOP);u.reg_write(a.UC_ARM_REG_SP,CSTACK+0x800)
   return u,callback,write
  def read(u,address):return int.from_bytes(u.mem_read(address,4),'little')
  vectors=[(0x90000,32,0x90100),(0x90000,32,0x90020),(0x90000,64,0x90020),
           (0x7fffff00,32,0x80000100),(0x7fffffe0,64,0x7ffffff0),
           (0x80000000,32,0x80000100),(0xfffffff0,64,0xfffffff8)]
  for sp,size,top in vectors:
   for oldflags in [0,5,10,15]:
    u,callback,write=setup(0,sp=sp);write(TYPE,size);write(TYPE+8,INIT);write(REG+16,top)
    calls=[]
    def hook(uc,pc,n,data):
     if pc==callback:
      calls.append('extend');assert read(u,REG+36)==TYPE;assert read(u,REG+8)==FP
      write(REG+36,NEW);write(REG+8,FP+0x1000);write(REG+4,MP+0x1000)
      if arch=='386':
       for r in [x.UC_X86_REG_EAX,x.UC_X86_REG_ECX,x.UC_X86_REG_EDX]:uc.reg_write(r,0xdeadbeef)
      else:
       for r in [a.UC_ARM_REG_R0,a.UC_ARM_REG_R1,a.UC_ARM_REG_R2,a.UC_ARM_REG_R3]:uc.reg_write(r,0xdeadbeef)
     elif pc==INIT:calls.append('init')
    u.hook_add(UC_HOOK_CODE,hook)
    if arch=='arm':u.reg_write(a.UC_ARM_REG_CPSR,(u.reg_read(a.UC_ARM_REG_CPSR)&0x0fffffff)|(oldflags<<28))
    else:u.reg_write(x.UC_X86_REG_EFLAGS,2|((oldflags&1)<<0)|((oldflags&2)<<5)|((oldflags&4)<<5)|((oldflags&8)<<8))
    before=bytes(u.mem_read(sp&~4095,4096))
    u.emu_start(BASE,STOP,count=500)
    slow=sp+size>0xffffffff or sp+size>=top
    assert calls==(['extend'] if slow else ['init']),(arch,hex(sp),size,hex(top),calls)
    actual=u.reg_read(x.UC_X86_REG_ECX if arch=='386' else a.UC_ARM_REG_R3)
    assert actual==(NEW if slow else sp),(arch,actual)
    assert read(u,REG+12)==(sp if slow else sp+size)
    if slow:
     assert u.reg_read(x.UC_X86_REG_ESI if arch=='386' else a.UC_ARM_REG_R9)==FP+0x1000
     assert u.reg_read(x.UC_X86_REG_EDI if arch=='386' else a.UC_ARM_REG_R8)==MP+0x1000
    else:assert read(u,sp+12)==TYPE and read(u,sp+8)==0
    expected=bytearray(before)
    if not slow:
     struct.pack_into('<I',expected,(sp&4095)+8,0);struct.pack_into('<I',expected,(sp&4095)+12,TYPE)
    assert bytes(u.mem_read(sp&~4095,4096))==expected,'frame canary or slow-path storage changed'
    checks+=1
  # Nil callee has no entry lookup; the untouched entry register must not
  # accidentally choose the native branch or become a callback address.
  for mode in [0,1,2]:
   for poison in [0,0x11111111,0xffffffff,0x7fffffff]:
    u,callback,write=setup(1,mode)
    sa=(MP if mode==1 else FP)+0x100;da=(MP if mode in [1,2] else FP)+0x200
    if mode==2:
     sa=FP+0x120;da=MP+0x230;write(FP+0x40,sa-0x20);write(MP+0x48,da-0x30)
    write(sa,NEW);write(da,0xffffffff)
    if arch=='386':u.reg_write(x.UC_X86_REG_EAX,poison)
    else:u.reg_write(a.UC_ARM_REG_R1,poison)
    callback=lib.callback(2)
    reached=[]
    def hook(uc,pc,n,data):
     if pc==callback:
      reached.append(pc);assert read(u,REG+8)==FP;assert bytes(u.mem_read(NEW,16))==bytes(16);uc.emu_stop()
    u.hook_add(UC_HOOK_CODE,hook);u.emu_start(BASE,STOP,count=500)
    assert reached,(arch,mode,poison)
    checks+=1
  # Non-nil entry lookup must overwrite the default sentinel and preserve
  # compiled, interpreter and runtime-module selection.
  for mode in [0,1,2]:
   for outcome in ['compiled','interpreted','runtime']:
    u,callback,write=setup(1,mode)
    sa=(MP if mode==1 else FP)+0x100;da=(MP if mode in [1,2] else FP)+0x200
    if mode==2:
     sa=FP+0x120;da=MP+0x230;write(FP+0x40,sa-0x20);write(MP+0x48,da-0x30)
    callee=TYPE+0x100
    write(sa,NEW);write(da,callee);write(callee-12,2)
    for off,value in [(0,0xb0000),(8,int(outcome=='compiled')),(12,0 if outcome=='runtime' else 0xc0000),(24,1),(28,STOP)]:write(callee+off,value)
    write(REG+32,STOP)
    if arch=='386':u.mem_write(CSTACK+0x800,struct.pack('<6I',0x10,0x20,0x30,0x40,0x50,STOP))
    calls=[]
    def hook(uc,pc,n,data):
     if pc==callback:
      calls.append(pc);assert outcome=='runtime';assert read(u,REG+56)==STOP;assert read(u,REG+8)==NEW;uc.emu_stop()
    u.hook_add(UC_HOOK_CODE,hook);u.emu_start(BASE,STOP,count=500)
    assert bool(calls)==(outcome=='runtime')
    if outcome!='runtime':
     assert read(u,REG+24)==callee and read(u,REG+4)==0xb0000
     assert read(u,callee-12)==3
     if outcome=='interpreted':assert read(u,REG)==STOP and read(u,REG+8)==NEW
     else:assert u.reg_read(x.UC_X86_REG_ESI if arch=='386' else a.UC_ARM_REG_R9)==NEW
    assert read(u,NEW)==(BASE+0x300)&0xffffffff and read(u,NEW+4)==FP
    checks+=1
  # Production guards reject invalid slots without fetching the slot or
  # touching a pending frame. The helper body is a recording boundary.
  for kind in [1,2]:
   for mode in [0,1,2]:
    for index in [0,1,32767]:
     for count,target in [(0,STOP),(1,STOP),(2,STOP),(-1,STOP),(2,0),(2,0xffffffff)]:
      u,_,write=setup(kind,mode,linkindex=index);callback=lib.callback(2)
      sa=(MP if mode==1 else FP)+0x100;da=(MP if mode in [1,2] else FP)+0x200
      if mode==2:
       sa=FP+0x120;da=MP+0x230;write(FP+0x40,sa-0x20);write(MP+0x48,da-0x30)
      callee=TYPE+0x100
      write(sa,NEW if kind==1 else callee);write(da,callee if kind==1 else 0xa5a5a5a5)
      write(callee+24,count)
      if index<2:write(callee+28+index*8+(4 if kind==2 else 0),target)
      # Only invalid cases execute here; normal calls are covered above.
      invalid=index>=count or target in [0,0xffffffff]
      if not invalid and kind==1:continue
      reached=[]
      if index>=count:
       def forbidden_read(uc,access,address,size,value,data):raise AssertionError('invalid link slot was read')
       u.hook_add(UC_HOOK_MEM_READ,forbidden_read,begin=callee+28,end=callee+4095)
      def hook(uc,pc,n,data):
       if pc==callback:reached.append(pc);uc.emu_stop()
      u.hook_add(UC_HOOK_CODE,hook);u.emu_start(BASE,STOP if invalid else BASE+lib.build(kind,mode,index),count=500)
      assert bool(reached)==invalid,(arch,kind,mode,index,count,target)
      assert bytes(u.mem_read(NEW,16))==bytes(16) and read(u,REG+8)==FP
      checks+=1
print(f'PASS JIT frames: {checks} x86/ARM stack-limit/wrap, allocation/handoff and nil-module native cases')
