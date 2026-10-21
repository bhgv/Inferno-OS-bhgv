#!/usr/bin/env python3
"""Compile production ARM FP emission paths, then assemble/disassemble/emulate them.

Requires a host C compiler and Python packages keystone-engine, capstone, unicorn.
The boundary stubs are integer address/constant emission, scheduling and punt;
this is not a complete Inferno emulator or an ARM ABI integration test.
"""
import ctypes
import math
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
from keystone import Ks, KS_ARCH_ARM, KS_MODE_ARM
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import (UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC,
                              UC_ARM_REG_R0, UC_ARM_REG_R8, UC_ARM_REG_R9)

ROOT = Path(__file__).resolve().parents[2]
SRC = (ROOT / 'libinterp/comp-arm.c').read_text()


def function(name):
    start = SRC.index('static void\n' + name + '(')
    brace = SRC.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (SRC[end] == '{') - (SRC[end] == '}')
        end += 1
    return SRC[start:end]


# Use the real instruction/address enums and real FP switch cases.
header = SRC[SRC.index('#define\tRESCHED'):SRC.index('static\tulong*\tcode;')]
conversions = SRC[SRC.index('\tcase IDIVF:', SRC.index('comp(Inst')):
                  SRC.index('\tcase ISHLL:', SRC.index('arithf:'))]
move = SRC[SRC.index('\tcase IMOVF:', SRC.index('comp(Inst')):
           SRC.index('\t\t/* if no hardware, just fall through */')]
unit = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#define ulong uint32_t
#define USED(x) (void)(x)
#include "isa.h"
''' + header + r'''
typedef union Adr { int32_t ind; struct { uint16_t f,s; } i; struct Inst *ins; } Adr;
typedef struct Inst { uint8_t op,add; uint16_t reg; Adr s,d; } Inst;
static ulong *code;
static int punted, puntmask;
static void (*optab[256])(void);
static void urk(char *s) { (void)s; abort(); }
/* Stub the integer emitter boundary; FP addressing below is production code. */
static void ccon(int cc, ulong v, int r, int opt) {
    (void)opt;
    *code++ = ((ulong)cc<<28)|0x03000000|(r<<12)|((v&0xf000)<<4)|(v&0xfff);
    *code++ = ((ulong)cc<<28)|0x03400000|(r<<12)|((v>>12)&0xf000)|((v>>16)&0xfff);
}
static void mem(int op, ulong disp, int rm, int r) {
    if(op == Ldw) { LDW(AL, rm, r, disp); }
    else abort();
}
static void opwld(Inst *i, int op, int r) {
    if(op != Ldw) abort();
    mem(op, i->s.ind, USRC(i->add) == AFP ? RFP : RMP, r);
}
static void punt(Inst *i, int mask, void (*fn)(void)) {
    (void)i; (void)fn; punted++; puntmask=mask;
}
static void schedcheck(Inst *i) { (void)i; }
#undef BRADIS
#define BRADIS(C, O) BRA(C, 1)
static void gen(ulong v) { *code++=v; }
''' + '\n'.join(function(n) for n in ('memfl', 'opfl', 'opflld', 'opflst', 'midfl', 'cbraf')) + r'''
int emit(ulong *out, int op, int add, int s, int m, int d, int cond) {
    Inst i = {0}; int r;
    code=out; punted=0; puntmask=0;
    i.op=op; i.add=add; i.s.ind=s; i.reg=m; i.d.ind=d;
    if(cond >= 0) {
        cbraf(&i, cond);
        if(!punted) {
            *code++=0xe3a00000; /* mov r0, #0 */
            *code++=0xea000000; /* b end */
            *code++=0xe3a00001; /* mov r0, #1 */
        }
    } else switch(op) {
''' + move + r'''
        case IMOVL: /* Non-VFP IMOVF intentionally falls through to integer copy. */
            opflld(&i, Ldf, FA2); /* not exercised here */
            break;
''' + conversions + r'''
    }
    return punted ? -puntmask : (int)(code-out);
}
int enabled(void) { return ARM_VFP; }
'''
# Keep the real case bodies unchanged; provide the same Inst pointer binding.
unit = unit.replace('Inst i = {0}; int r;', 'Inst obj = {0}; Inst *i=&obj; int r;')
unit = unit.replace('i.op=op; i.add=add; i.s.ind=s; i.reg=m; i.d.ind=d;',
                    'i->op=op; i->add=add; i->s.ind=s; i->reg=m; i->d.ind=d;')
unit = unit.replace('cbraf(&i, cond)', 'cbraf(i, cond)').replace('opflld(&i,', 'opflld(i,')
# Obtain actual opcode numbers from the ISA enum with the C compiler.
ks = Ks(KS_ARCH_ARM, KS_MODE_ARM)
cs = Cs(CS_ARCH_ARM, CS_MODE_ARM)
BASE, FP, MP = 0x10000, 0x20000, 0x30000
checks = 0


def run(blob, setup, fpscr=0):
    from unicorn.arm_const import UC_ARM_REG_FPSCR
    u = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    for address in (BASE, FP, MP):
        u.mem_map(address, 0x10000)
    u.reg_write(UC_ARM_REG_C1_C0_2, 0xf << 20)
    u.reg_write(UC_ARM_REG_FPEXC, 1 << 30)
    u.reg_write(UC_ARM_REG_FPSCR, fpscr)
    u.reg_write(UC_ARM_REG_R9, FP + 0x4000)
    u.reg_write(UC_ARM_REG_R8, MP + 0x4000)
    u.mem_write(BASE, blob)
    for address, value in setup.items():
        u.mem_write(address, value)
    u.emu_start(BASE, BASE + len(blob), count=100)
    return u


with tempfile.TemporaryDirectory(prefix='arm-vfp-') as td:
    td = Path(td)
    source = td / 'emitter.c'
    source.write_text(unit)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g'))
    libs = []
    for label, defines, expected in [('vfp', ['-DARM_VFP=1'], 1),
                                     ('soft', ['-DARM_VFP=0'], 0),
                                     ('auto-soft', [], 0),
                                     ('auto-vfp', ['-D__ARM_FP=12'], 1),
                                     ('single-only', ['-D__ARM_FP=4'], 0),
                                     ('override', ['-D__ARM_FP=12', '-DARM_VFP=0'], 0)]:
        binary = td / (label + '.so')
        subprocess.run(cc + flags + ['-shared', '-fPIC', '-I' + str(ROOT / 'include')]
                       + defines + [str(source), '-o', str(binary)], check=True)
        lib = ctypes.CDLL(str(binary))
        assert lib.enabled() == expected, label
        libs.append(lib)
    lib, soft = libs[:2]
    names = ['IADDF', 'ISUBF', 'IMULF', 'IDIVF', 'INEGF', 'ICVTWF', 'ICVTFW', 'IMOVF']
    numbers = td / 'numbers.c'
    numbers.write_text('#include <stdio.h>\n#include "isa.h"\nint main(void){' +
                       ''.join(f'printf("%d ", {n});' for n in names) + '}')
    subprocess.run(cc + ['-I' + str(ROOT / 'include'), str(numbers), '-o', str(td/'numbers')], check=True)
    ops = dict(zip(names, map(int, subprocess.check_output([str(td/'numbers')]).split())))

    def emit(name, add=0x80 | (1 << 3) | 1, s=0x100, m=0x200, d=0x300, cond=-1, library=lib):
        words = (ctypes.c_uint32 * 256)()
        n = library.emit(words, ops[name], add, s, m, d, cond)
        return n, bytes(words)[:max(n, 0)*4]

    # Independent assembler oracle: catches wrong coprocessor/register/op bits.
    for name, instruction in [('IADDF', 'vadd.f64 d4,d4,d2'),
                              ('ISUBF', 'vsub.f64 d4,d4,d2'),
                              ('IMULF', 'vmul.f64 d4,d4,d2'),
                              ('IDIVF', 'vdiv.f64 d4,d4,d2'),
                              ('INEGF', 'vneg.f64 d2,d2')]:
        _, blob = emit(name)
        expected = bytes(ks.asm(instruction)[0])
        assert expected in blob, (name, list(cs.disasm(blob, BASE)))
        checks += 1
    _, blob = emit('ICVTWF')
    assert bytes(ks.asm('vmov s4,r3; vcvt.f64.s32 d2,s4')[0]) in blob
    checks += 1

    # Arithmetic order, aliases/in-place destination, addressing and signed zero.
    for add, s, m, d, sb, mb, db, pointers in [
        (0x89, 0x100, 0x200, 0x300, FP+0x4100, FP+0x4200, FP+0x4300, {}),
        (0xc0, 0x100, 0x200, 0x300, MP+0x4100, MP+0x4200, MP+0x4300, {}),
        (0x09, 0x100, 0, 0x300, FP+0x4100, FP+0x4300, FP+0x4300, {}),
        (0x89, 0x300, 0x200, 0x300, FP+0x4300, FP+0x4200, FP+0x4300, {}),
        (0x89, -1020, 0x200, 1020, FP+0x4000-1020, FP+0x4200, FP+0x4000+1020, {}),
        (0x89, 1024, 0x200, 2048, FP+0x4400, FP+0x4200, FP+0x4800, {}),
        # Source and destination double indirection: packed short f/s offsets.
        (0x80 | (5<<3) | 4, (12<<16)|0x80, 0x200, (20<<16)|0x90,
         FP+0x600c, FP+0x4200, MP+0x6014,
         {FP+0x4080: struct.pack('<I', FP+0x6000), MP+0x4090: struct.pack('<I', MP+0x6000)}),
    ]:
        for name, want in [('IADDF', 10.0), ('ISUBF', 4.0), ('IMULF', 21.0), ('IDIVF', 7.0/3.0)]:
            _, blob = emit(name, add, s, m, d)
            setup = dict(pointers)
            setup[sb] = struct.pack('<d', 3.0)
            setup[mb] = struct.pack('<d', 7.0)
            # If source aliases destination the middle value remains independent.
            u = run(blob, setup)
            result = struct.unpack('<d', u.mem_read(db, 8))[0]
            assert result == want, (name, add, result, want)
            checks += 1
    for value in (0.0, -0.0, 1.5, -2.25, math.inf, -math.inf):
        _, blob = emit('INEGF')
        u = run(blob, {FP+0x4100: struct.pack('<d', value)})
        assert bytes(u.mem_read(FP+0x4300, 8)) == struct.pack('<d', -value)
        checks += 1
    for value in (0, 1, -1, 2147483647, -2147483648):
        _, blob = emit('ICVTWF')
        u = run(blob, {FP+0x4100: struct.pack('<i', value)})
        assert struct.unpack('<d', u.mem_read(FP+0x4300, 8))[0] == float(value)
        checks += 1
    # Inline arithmetic must obey the current FPSCR rounding mode.
    for mode, want in [(0, 1.0), (1, math.nextafter(1.0, math.inf)), (2, 1.0), (3, 1.0)]:
        _, blob = emit('IADDF')
        u = run(blob, {FP+0x4100:struct.pack('<d', 2.0**-53),
                       FP+0x4200:struct.pack('<d', 1.0)}, fpscr=mode << 22)
        assert struct.unpack('<d', u.mem_read(FP+0x4300, 8))[0] == want
        checks += 1
    # All six relations, including unordered in either operand and signed zero.
    for cond, predicate in [(0, lambda a,b:a==b), (1, lambda a,b:a!=b),
                            (11, lambda a,b:a<b), (13, lambda a,b:a<=b),
                            (12, lambda a,b:a>b), (10, lambda a,b:a>=b)]:
        for a,b in [(1.,2.), (2.,1.), (2.,2.), (-0.,0.), (math.inf,math.inf),
                    (math.nan,1.), (1.,math.nan), (math.nan,math.nan)]:
            _, blob = emit('IADDF', cond=cond)
            assert bytes(ks.asm('vcmp.f64 d4,d2; vmrs APSR_nzcv,FPSCR')[0]) in blob
            u = run(blob, {FP+0x4100:struct.pack('<d',a), FP+0x4200:struct.pack('<d',b)})
            assert bool(u.reg_read(UC_ARM_REG_R0)) == predicate(a,b), (cond,a,b)
            checks += 1
    # Conversion with Dis-specific rounding and immediate middle operands punt.
    assert emit('ICVTFW')[0] == -(1|2)
    for name in ('IADDF', 'ISUBF', 'IMULF', 'IDIVF'):
        assert emit(name, add=0x49)[0] == -(1|2|64)
        assert emit(name, library=soft)[0] == -(1|2|64)
        checks += 2
    for cond in (0,1,11,13,12,10):
        assert emit('IADDF', add=0x49, cond=cond)[0] == -(1|64|32|16|4)
        assert emit('IADDF', cond=cond, library=soft)[0] == -(1|64|32|16|4)
        checks += 2
    for name in ('INEGF','ICVTWF','ICVTFW'):
        assert emit(name, library=soft)[0] == -(1|2)
        checks += 1
    # Bit-preserving move (negative zero and NaN payload).
    for bits in (0x8000000000000000, 0x7ff8123456789abc):
        _, blob = emit('IMOVF')
        raw = struct.pack('<Q', bits)
        u = run(blob, {FP+0x4100:raw})
        assert bytes(u.mem_read(FP+0x4300,8)) == raw
        checks += 1
print(f'PASS ARM VFP: {checks} encoding, execution, branch and fallback checks; 6 target configurations')
