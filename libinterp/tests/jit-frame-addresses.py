#!/usr/bin/env python3
"""Execute production macfram arithmetic/branches in five Unicorn CPU models.

Descriptor layout and call/constant emission are boundary stubs. Execution stops
at the first stack publication or slow-path type store, before calls or frame
initialization. This is not a native ABI/integration test.
"""
import ctypes
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import *
from unicorn.x86_const import UC_X86_REG_EDX
from unicorn.arm_const import UC_ARM_REG_R0
from unicorn.mips_const import UC_MIPS_REG_0
from unicorn.ppc_const import UC_PPC_REG_0
from unicorn.sparc_const import UC_SPARC_REG_G0, UC_SPARC_REG_L4, UC_SPARC_REG_L3

root=Path(__file__).resolve().parents[2]
common=r'''
#include <stdint.h>
#include <stddef.h>
#define ulong uint32_t
#define uchar uint8_t
typedef uint32_t Rune;
#define nil NULL
#define O(t,f) OFF_##t##_##f
#define REGMOD 1
#define OFF_REG_SP 16
#define OFF_REG_TS 20
#define OFF_REG_s 24
#define OFF_REG_FP 28
#define OFF_REG_MP 32
#define OFF_REG_st 36
#define OFF_Type_size 4
#define OFF_Type_initialize 8
#define OFF_Frame_t 0
#define OFF_Frame_mr 4
static int R;
static void extend(void) {}
'''
# Keep all instruction enums and encoding macros from each backend.
def header(source):
    start=min(source.index('\nenum'),source.index('\n#define'))
    return source[start:source.index('static\t',start)]

def function(source,name):
    start=source.index('static void\n'+name+'(')
    return source[start:source.index('\n}',start)+2]

stubs={
'386':r'''
static void genb(int x) {*code++=x;}
static void gen2(int a,int b) {genb(a);genb(b);}
static void genw(ulong x) {for(int i=0;i<4;i++)genb(x>>(8*i));}
static int bc(int x) {return x>=-128 && x<128;}
MODRM
static void con(ulong v,int r) {(void)v;genb(0xb8+r);genw(0x20000);}
static void bra(ulong a,int op) {(void)a;(void)op;genb(0x90);}
''',
'arm':r'''
#undef CALL
#define CALL(x) gen(0xe1a00000)
static void mem(int op,ulong off,int rm,int r) {
 if(op==Ldw) { LDW(AL,rm,r,off); }
 else { STW(AL,rm,r,off); }
}
static void con(ulong v,int r,int opt) {(void)v;(void)r;(void)opt;gen(0xe1a00000);}
''',
'mips':r'''
static void delay(void) {gen(0);}
static void ldc(ulong v,int r) {(void)v;(void)r;gen(0);}
''',
'power':r'''
static void br(ulong op,ulong disp) {gen(op|(disp&0xfffc));}
static void mtspr(int d,int s) {(void)d;(void)s;gen(0x60000000);}
static void mfspr(int d,int s) {(void)d;(void)s;gen(0x60000000);}
static void jmpl(ulong *p) {(void)p;gen(0x60000000);}
static void ldc(ulong v,int r) {(void)v;(void)r;gen(0x60000000);}
#undef SETR0
#define SETR0() gen(0x60000000)
#undef LRET
#define LRET() gen(0x60000000)
''',
'sparc':r'''
#undef CALL
#define CALL(x) NOOP
static void mem(int op,ulong off,int rm,int r) { FM3I(3,op,off,rm,r); }
static void con(ulong v,int r,int opt) {(void)v;(void)r;(void)opt;NOOP;}
'''
}
configs={
'386':(UC_ARCH_X86,UC_MODE_32,'<',[(UC_X86_REG_EDX,0x21000)]),
'arm':(UC_ARCH_ARM,UC_MODE_ARM,'<',[(UC_ARM_REG_R0+5,0x20000),(UC_ARM_REG_R0+4,0x21000)]),
'mips':(UC_ARCH_MIPS,UC_MODE_MIPS32|UC_MODE_BIG_ENDIAN,'>',[(UC_MIPS_REG_0+15,0x20000),(UC_MIPS_REG_0+11,0x21000)]),
'power':(UC_ARCH_PPC,UC_MODE_32|UC_MODE_BIG_ENDIAN,'>',[]),
'sparc':(UC_ARCH_SPARC,UC_MODE_32|UC_MODE_BIG_ENDIAN,'>',[(UC_SPARC_REG_L4,0x20000),(UC_SPARC_REG_L3,0x21000)]),
}
with tempfile.TemporaryDirectory(prefix='jit-frame-addresses-') as directory:
    td=Path(directory)
    for arch,(cpu,mode,endian,registers) in configs.items():
        if os.environ.get('JIT_STACK_ARCH') not in (None,arch): continue
        source=(Path(os.environ.get('JIT_STACK_SOURCE_DIR',root/'libinterp'))/('comp-'+arch+'.c')).read_text()
        h=header(source)
        # PPC has the register assignment in the same enum as the instruction set.
        if arch=='power':
            import re
            def reg(name): return int(re.search(r'\b'+name+r'\s*=\s*(\d+)',h).group(1))
            registers=[(UC_PPC_REG_0+reg('Rreg'),0x20000),(UC_PPC_REG_0+reg('Ri'),0x21000)]
        scalar='uchar' if arch=='386' else 'ulong'
        unit=common+h+'\nstatic '+scalar+' buffer[1024], *code;\n'
        if arch!='386': unit+='static void gen(ulong x) {*code++=x;}\n'
        stub=stubs[arch]
        if arch=='386': stub=stub.replace('MODRM',function(source,'modrm'))
        unit+=stub+'\n'+function(source,'macfram')
        unit+='\nint emit(void *out) {code=buffer;macfram();int n=(char*)code-(char*)buffer;__builtin_memcpy(out,buffer,n);return n;}\n'
        target=td/(arch+'.c');target.write_text(unit)
        library=td/(arch+'.so')
        cmd=shlex.split(os.environ.get('CC','cc'))
        subprocess.run(cmd+['-w','-shared','-fPIC','-I'+str(root/'include'),str(target),'-o',str(library)],check=True)
        lib=ctypes.CDLL(str(library));output=ctypes.create_string_buffer(4096);n=lib.emit(output)
        code=output.raw[:n]
        if endian=='>': code=b''.join(code[i:i+4][::-1] for i in range(0,n,4))
        starts=[0x10000,0x7fffff00,0x80000000,0xfffff000,0xffffff00]
        sizes=[32,64,255,256,4096,0x7fffffff]
        checks=0
        for sp in starts:
            for size in sizes:
                for room in [0,31,32,63,64,255,256,4096,0x10000]:
                    ts=sp+room
                    if ts>0xffffffff: continue
                    uc=Uc(cpu,mode);uc.mem_map(0x10000,0x1000);uc.mem_write(0x10000,code)
                    uc.mem_map(0x20000,0x2000)
                    uc.mem_write(0x20010,struct.pack(endian+'II',sp,ts))
                    uc.mem_write(0x21004,struct.pack(endian+'I',size))
                    for reg,value in registers: uc.reg_write(reg,value)
                    result=[]
                    def written(u,access,address,length,value,data):
                        if address in (0x20010,0x20018):
                            result.append((address,value));u.emu_stop()
                    uc.hook_add(UC_HOOK_MEM_WRITE,written)
                    uc.emu_start(0x10000,0x10000+n,count=100)
                    expected=0x20010 if sp+size<ts else 0x20018
                    assert result and result[0][0]==expected,(arch,hex(sp),size,room,result)
                    if expected==0x20010: assert result[0][1]==sp+size
                    checks+=1
        print(arch,checks,'machine-code boundary cases',flush=True)
