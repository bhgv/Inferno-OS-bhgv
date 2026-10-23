#!/usr/bin/env python3
"""Execute real 386/ARM mframe selection and guards in Unicorn CPU models.

Constants, VM fallback and call emission are boundary stubs. Hooks stop at
fallback/native-helper entry; no complete module call ABI is simulated.
"""
import ctypes
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
from unicorn import Uc, UC_ARCH_X86, UC_ARCH_ARM, UC_MODE_32, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_R5

root = Path(__file__).resolve().parents[2]
common = r'''
#include <stdint.h>
#include <stddef.h>
#include "isa.h"
#define ulong uint32_t
#define uchar uint8_t
#define nil NULL
#define H ((void*)(uintptr_t)0xffffffff)
#define nelem(a) (sizeof(a)/sizeof((a)[0]))
#define O(t,f) OFF_##t##_##f
#define OA(t,f) OFF_##t##_##f
#define OFF_Modlink_nlinks 4
#define OFF_Modlink_links 32
#define OFF_Modl_frame 4
#define OFF_Type_size 4
#define OFF_Type_initialize 8
struct Frame { uint32_t words[4]; };
typedef struct Frame Frame;
typedef struct Modl { uint32_t pc, frame; } Modl;
typedef struct Inst { int add, op; uint16_t reg; } Inst;
static Inst instructions[2];
static struct { Inst *prog; } module, *mod=&module;
static ulong patch[2], macro[NMACRO];
static void (*optab[256])(void);
static int native_at, punt_at;
'''
# Extract full bodies without reimplementing guard or instruction selection logic.
def function(source, name):
    start = source.index('static void\n'+name+'(')
    return source[start:source.index('\n}', start)+2]

stubs = {
'386': r'''
static void genb(int x) { *code++=x; }
static void gen2(int a,int b) { genb(a); genb(b); }
static void genw(ulong x) { for(int i=0;i<4;i++)genb(x>>(8*i)); }
static int bc(int x) { return x>=-128 && x<128; }
MODRM
CMPL
static void opwld(Inst *i,int op,int r) {
    (void)i;(void)op;gen2(0x8b,(r<<3)|5);genw(0x20000);
}
static void opwst(Inst *i,int op,int r) { (void)i;(void)op;(void)r;genb(0x90); }
static void mid(Inst *i,int op,int r) { (void)i;(void)op;(void)r;genb(0x90); }
static void punt(Inst *i,int flags,void (*fn)(void)) {
    (void)i;(void)flags;(void)fn;punt_at=code-buffer;genb(0x90);
}
static void rbra(ulong address,int op) {
    (void)address;if(op==Ocall)native_at=code-buffer;genb(0x90);
}
''',
'arm': r'''
#undef CALLMAC
#define CALLMAC(c,o) (native_at=(char*)code-(char*)buffer,gen(0xe1a00000))
#undef BRADIS
#define BRADIS(c,o) gen(0xe1a00000)
#undef BRAMAC
#define BRAMAC(c,o) (punt_at=(char*)code-(char*)buffer,gen(0xe1a00000))
static void con(ulong value,int r,int opt) {
    (void)opt;
    gen(0xe3000000|((value&0xf000)<<4)|(r<<12)|(value&0xfff));
    gen(0xe3400000|(((value>>16)&0xf000)<<4)|(r<<12)|((value>>16)&0xfff));
}
static void mem(int op,ulong off,int rm,int r) {
    (void)op;
    if(off>4095) {
        con(off,RCON,1); DP(AL,Add,rm,RCON,0,RCON); rm=RCON; off=0;
    }
    LDW(AL,rm,r,off);
}
static void opwld(Inst *i,int op,int r) { (void)i;mem(op,0,RREG,r); }
static void opwst(Inst *i,int op,int r) { (void)i;(void)op;(void)r;gen(0xe1a00000); }
static void mid(Inst *i,int op,int r) { (void)i;(void)op;(void)r;gen(0xe1a00000); }
static void punt(Inst *i,int flags,void (*fn)(void)) {
    (void)i;(void)flags;(void)fn;punt_at=(char*)code-(char*)buffer;gen(0xe1a00000);
}
'''
}
with tempfile.TemporaryDirectory(prefix='jit-mframe-') as directory:
    td = Path(directory)
    for arch in ('386', 'arm'):
        folder = Path(os.environ.get('JIT_MFRAME_SOURCE_DIR', root/'libinterp'))
        if os.environ.get('JIT_MFRAME_ARCH') not in (None, arch): continue
        source = (folder/('comp-'+arch+'.c')).read_text()
        begin = min(source.index('\nenum'), source.index('\n#define'))
        header = source[begin:source.index('static\t', begin)]
        scalar = 'uchar' if arch == '386' else 'ulong'
        # Header defines NMACRO used by the synthetic module data.
        unit = common[:common.index('static ulong patch')] + header
        unit += common[common.index('static ulong patch'):]
        unit += '\nstatic '+scalar+' buffer[2048], *code, *base;\n'
        if arch == 'arm':
            unit += 'static void gen(ulong x) { *code++=x; }\n'
        stub = stubs[arch]
        if arch == '386':
            stub = stub.replace('MODRM', function(source, 'modrm')).replace('CMPL', function(source, 'cmpl'))
        start = source.index('\tcase IMFRAME:')
        block = source[start:source.index('\n\tcase ', start+5)]
        unit += stub+'\n'+function(source, 'commframe')
        unit += '\nstatic void selectframe(Inst *i) { switch(i->op) { '+block+' } }\n'
        unit += r'''
int emit(void *out,int index,int mode) {
    Inst *i=&instructions[0];
    module.prog=instructions; i->reg=index; i->op=IMFRAME; i->add=mode;
    code=buffer; native_at=punt_at=-1; selectframe(i);
    int n=(char*)code-(char*)buffer; __builtin_memcpy(out,buffer,n); return n;
}
int native_offset(void) { return native_at; }
int punt_offset(void) { return punt_at; }
'''
        target = td/(arch+'.c')
        target.write_text(unit)
        library = td/(arch+'.so')
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        subprocess.run(cmd+['-w', '-shared', '-fPIC', '-I'+str(root/'include'), str(target), '-o', str(library)], check=True)
        lib = ctypes.CDLL(str(library))
        output = ctypes.create_string_buffer(8192)
        checks = 0
        for index in (-32768, -1, 0, 1, 2, 32767):
            n = lib.emit(output, index, 0x40)
            native, fallback = lib.native_offset(), lib.punt_offset()
            for moduleptr in (0, 0xffffffff, 0x21000):
                for count in (-1, 0, 1, 2, 32768):
                    for descriptor in (0, 0xffffffff, 0x22000):
                        for size in (-1, 0, 15, 16, 32):
                            for initialize in (0, 0x30000):
                                cpu, mode = (UC_ARCH_X86, UC_MODE_32) if arch == '386' else (UC_ARCH_ARM, UC_MODE_ARM)
                                uc = Uc(cpu, mode)
                                uc.mem_map(0x10000, 0x1000)
                                uc.mem_write(0x10000, output.raw[:n])
                                uc.mem_map(0x20000, 0x3000)
                                uc.mem_write(0x20000, struct.pack('<I', moduleptr))
                                uc.mem_write(0x21004, struct.pack('<i', count))
                                if index >= 0:
                                    address = 0x21000+32+index*8+4
                                    page = address & ~4095
                                    if page >= 0x23000: uc.mem_map(page, 4096)
                                    uc.mem_write(address, struct.pack('<I', descriptor))
                                uc.mem_write(0x22004, struct.pack('<iI', size, initialize))
                                if arch == 'arm': uc.reg_write(UC_ARM_REG_R5, 0x20000)
                                reached, reads = [], []
                                def read(u, access, address, length, value, data):
                                    reads.append(address)
                                def stop(u, address, length, data):
                                    if address in (0x10000+native, 0x10000+fallback):
                                        reached.append(address-0x10000); u.emu_stop()
                                uc.hook_add(UC_HOOK_CODE, stop)
                                uc.hook_add(UC_HOOK_MEM_READ, read)
                                uc.emu_start(0x10000, 0x10000+n, count=100)
                                valid = index >= 0 and moduleptr == 0x21000 and index < count and descriptor == 0x22000 and size >= 16 and initialize != 0
                                assert reached == [native if valid else fallback], (arch, index, moduleptr, count, descriptor, size, initialize, reached)
                                if index >= 0 and moduleptr == 0x21000 and index >= count:
                                    assert 0x21000+32+index*8+4 not in reads
                                    assert 0x22004 not in reads and 0x22008 not in reads
                                checks += 1
        # Register-based link operands must use the checked VM selection path.
        for index in (-32768, -1, 0, 1, 2, 32767):
            for mode in (0, 0x80, 0xc0):
                lib.emit(output, index, mode)
                assert lib.native_offset() == -1 and lib.punt_offset() == 0
                checks += 1
        print(arch, checks, 'mframe selection/guard cases', flush=True)
