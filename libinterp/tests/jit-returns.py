#!/usr/bin/env python3
"""Execute real permanent RET bridges while unmapping the returning module.

Real preamble/macret/RET/punt encoders, projected IA-32/ARM32 layouts. Native
Type destruction and VM RET bodies are simulated boundaries, not a GC/OS boot.
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
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_CODE
import unicorn.x86_const as x
import unicorn.arm_const as a
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('handoff',Path(__file__).with_name('jit-handoff.py'))
handoff=importlib.util.module_from_spec(spec);previous=sys.dont_write_bytecode;sys.dont_write_bytecode=True
try:spec.loader.exec_module(handoff)
finally:sys.dont_write_bytecode=previous
model=handoff.model

def unit(arch):
 s=(root/f'libinterp/comp-{arch}.c').read_text();u=handoff.unit(arch)
 u=u.replace('typedef struct Type {int size,np;void *initialize,*destroy;uchar map[1];} Type;','typedef struct Type {int size,np;uint32_t initialize,destroy;} Type;')
 u=u.replace('static union {ulong align;uchar bytes[4096];} out;','static union {ulong align;uchar bytes[4096];} out __attribute__((aligned(4096)));')
 typ='uchar' if arch=='386' else 'ulong';prefix='x86' if arch=='386' else 'arm'
 decl=r'''
typedef struct Frame {uint32_t lr,fp,mr,t;} Frame;
typedef struct Modlink {uint32_t MP,m,compiled,prog;} Modlink;
static void (*optab[256])(void);
static void error(char *s){(void)s;abort();}
static char exCompile[]="compile";
'''+f'static {typ} *{prefix}ret,*{prefix}frp,*{prefix}helpercode;\n'
 u=u.replace('static int\njitvalidpc(',decl+'static int\njitvalidpc(')
 names=(['cmpl'] if arch=='386' else [])+['macfrp','retbody','preamble','macret'];code='\n'.join(model.function(s,n,'static int' if n=='preamble' else 'static void') for n in names)
 extra=r'''
unsigned buildret(void){
 module.prog=original;module.nprog=2;patch=offsets;offsets[1]=0;offsets[2]=0;
 optab[IRET]=rdestroy;pass=0;assert(preamble());
 base=(void*)out.bytes;code=base;RESET
#ifdef RET_UNSAFE_MODULE_BODY
 code=base+BODYOFFSET;PREFIXret=code;retbody();FLUSH
 code=base;
#endif
 macret();FLUSH
 assert((uchar*)code-out.bytes<4096);return (uchar*)code-out.bytes;
}
void *permanent(void){return shared;}
unsigned permanentsize(void){return sharedbytes;}
void finishret(void){assert(live==1);free((void*)comvec);comvec=nil;assert(live==0);}
'''.replace('RESET','' if arch=='386' else 'pass=1;codeoff=0;rcon.ptr=0;').replace('BODYOFFSET','0x100' if arch=='386' else '0x100/4').replace('PREFIX',prefix).replace('FLUSH','' if arch=='386' else 'flushcon(1);')
 # The old build() remains compiled but unused; all real RET dependencies are
 # the same encoders as that existing bridge harness.
 at=u.index('unsigned build(');return u[:at]+code+'\n'+extra+u[at:]
checks=0
requested=os.environ.get('RET_ARCH');assert requested in [None,'386','arm']
with tempfile.TemporaryDirectory(prefix='jit-returns-') as td:
 td=Path(td)
 for arch in ([requested] if requested else ['386','arm']):
  p=td/f'{arch}.c';p.write_text(unit(arch));binary=td/f'{arch}.so'
  subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))+['-shared','-fPIC','-I'+str(root/'include'),str(p),'-o',str(binary)],check=True)
  lib=C.CDLL(str(binary));lib.image.restype=C.c_void_p;lib.permanent.restype=C.c_void_p
  for parentkind in ['same','compiled','interpreted']:
   for refs in [1,2,7]:
    for fallback in ['none','nil-type','nil-destroy','root']:
     for clobber in [False,True]:
      size=lib.buildret();host=lib.image();MODULE=C.c_uint32(host).value;perm=lib.permanent();PERM=C.c_uint32(perm).value
      reg=lib.regaddr()&0xffffffff;callback=lib.callback()&0xffffffff
      FRAME,PARENT,MOD,OLDMOD,TYPE,DTOR,STACK,STOP,MP=0x200000,0x300000,0x400010,0x500010,0x600000,0x700000,0x800000,0xa00000,0xb00000
      u=Uc(UC_ARCH_X86,UC_MODE_32) if arch=='386' else Uc(UC_ARCH_ARM,UC_MODE_ARM)
      state=bytearray(60);struct.pack_into('<I',state,4,MP);struct.pack_into('<I',state,8,FRAME);struct.pack_into('<I',state,24,MOD);struct.pack_into('<I',state,32,STOP)
      frame=struct.pack('<4I',STOP,0 if fallback=='root' else PARENT,0 if parentkind=='same' else OLDMOD,0 if fallback=='nil-type' else TYPE)
      native_return=b'\xc3' if arch=='386' else struct.pack('<I',0xe1a0f00e)
      regions=[(MODULE,C.string_at(host,4096)),(PERM,C.string_at(perm,lib.permanentsize())),(reg,bytes(state)),(FRAME,frame+bytes(4096-16)),(PARENT,bytes(4096)),(MOD-16,struct.pack('<4I',0,refs,0,0)+struct.pack('<4I',MP,0,1,MODULE)),(OLDMOD-16,struct.pack('<4I',0,10,0,0)+struct.pack('<4I',MP+0x1000,0,int(parentkind!='interpreted'),STOP)),(TYPE,struct.pack('<4I',16,0,0,0 if fallback=='nil-destroy' else DTOR)),(DTOR,native_return),(callback,native_return if arch=='386' else struct.pack('<II',0xe92d4000,0xe8bd8000)),(STACK,bytes(4096)),(STOP,bytes(16))]
      model.mapregions(u,regions)
      def word(addr):return int.from_bytes(u.mem_read(addr,4),'little')
      def write(addr,v):u.mem_write(addr,struct.pack('<I',v&0xffffffff))
      events=[];removed=False
      slow=fallback!='none' or parentkind!='same'
      def hook(uc,pc,n,data):
       global removed
       if pc==DTOR:
        events.append('destroy')
        if clobber:
         regs=[x.UC_X86_REG_EAX,x.UC_X86_REG_ECX,x.UC_X86_REG_EDX] if arch=='386' else [a.UC_ARM_REG_R0,a.UC_ARM_REG_R1,a.UC_ARM_REG_R2,a.UC_ARM_REG_R3]
         for r in regs:uc.reg_write(r,0xdeadbeef)
       if pc==(callback if arch=='386' else callback+4):
        events.append('ret');assert word(reg+8)==FRAME
        assert word(MOD-12)==refs,'module released before VM frame destruction'
        if fallback=='root':uc.emu_stop();return
        # Model the VM's completed RET, including unloading the last module.
        if parentkind!='same' and refs==1:
         uc.mem_unmap(MODULE,4096);removed=True;write(MOD-12,0)
        else:write(MOD-12,refs-(parentkind!='same'))
        write(reg+8,PARENT);write(reg+12,FRAME);write(reg,STOP)
        if parentkind!='same':write(reg+24,OLDMOD);write(reg+4,MP+0x1000)
        write(reg+48,int(parentkind=='interpreted'))
        if clobber:
         regs=[x.UC_X86_REG_EAX,x.UC_X86_REG_ECX,x.UC_X86_REG_EDX] if arch=='386' else [a.UC_ARM_REG_R0,a.UC_ARM_REG_R1,a.UC_ARM_REG_R2,a.UC_ARM_REG_R3,a.UC_ARM_REG_R12,a.UC_ARM_REG_LR]
         for r in regs:uc.reg_write(r,0xdeadbeef)
      u.hook_add(UC_HOOK_CODE,hook)
      if arch=='386':
       u.reg_write(x.UC_X86_REG_ESI,FRAME);u.reg_write(x.UC_X86_REG_EDI,MP);u.reg_write(x.UC_X86_REG_ESP,STACK+2048)
       u.mem_write(STACK+2048,struct.pack('<6I',0x10,0x20,0x30,0x40,0x50,STOP))
      else:
       u.reg_write(a.UC_ARM_REG_R9,FRAME);u.reg_write(a.UC_ARM_REG_R8,MP);u.reg_write(a.UC_ARM_REG_R5,reg);u.reg_write(a.UC_ARM_REG_R2,0 if fallback=='nil-type' else TYPE);u.reg_write(a.UC_ARM_REG_LR,STOP);u.reg_write(a.UC_ARM_REG_SP,STACK+2048)
      try:u.emu_start(MODULE,STOP,count=2000)
      except Exception as e:raise AssertionError((arch,parentkind,refs,fallback,clobber,events,hex(u.reg_read(x.UC_X86_REG_EIP if arch=='386' else a.UC_ARM_REG_PC)),hex(PERM),hex(MODULE))) from e
      assert events==(['ret'] if slow else ['destroy']),(arch,parentkind,refs,fallback,events)
      if fallback=='root':
       assert word(reg+8)==FRAME and word(MOD-12)==refs
       assert bytes(u.mem_read(FRAME,16))==frame
       checks+=1;continue
      assert word(reg+8)==PARENT and word(reg+12)==FRAME
      assert word(MOD-12)==(0 if removed else refs-(parentkind!='same'))
      assert word(reg+24)==(MOD if parentkind=='same' else OLDMOD)
      assert bytes(u.mem_read(FRAME,16))==frame
      if parentkind=='interpreted':
       assert word(reg)==STOP
       if arch=='386':assert u.reg_read(x.UC_X86_REG_ESP)==STACK+2048+24
      else:assert u.reg_read(x.UC_X86_REG_ESI if arch=='386' else a.UC_ARM_REG_R9)==PARENT
      checks+=1
  lib.finishret()
print(f'PASS JIT returns: {checks} permanent RET, module unmap, native/VM returns, fallbacks, references and volatile-clobber cases')
