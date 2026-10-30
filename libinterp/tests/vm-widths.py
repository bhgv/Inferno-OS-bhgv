#!/usr/bin/env python3
"""Check production canonical serialization and native pointer decoding widths."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]

def extract(src, name, result):
    start=src.index(result+'\n'+name+'(')
    pos=src.index('{',start)+1
    depth=1
    while depth:
        depth+=(src[pos]=='{')-(src[pos]=='}')
        pos+=1
    return src[start:pos]

load=(root/'libinterp/load.c').read_text()
limbo=(root/'limbo/dtocanon.c').read_text()
vm=extract(load,'canontod','double').replace('canontod(', 'vm_canontod(')
limbo=limbo.replace('#include "limbo.h"','').replace('canontod(', 'compiler_canontod(')
big = re.search(r'bigbits = \(\(uvlong\)hi<<32\)\|lo;\s*memmove\(si, &bigbits, sizeof\(bigbits\)\);', load)
if big is None:
    raise RuntimeError('canonical big loader statements changed; update the borrowed-body probe')
big = big.group(0)
macro=next(line for line in (root/'libinterp/dec.c').read_text().splitlines() if line.startswith('#define DIND'))
# The generator must emit the same pointer expression as the generated file.
if macro.replace('\\','\\\\').replace('"','\\"') not in (root/'libinterp/decgen.c').read_text():
    raise RuntimeError('decoder generator does not emit the current indirect-address macro')
patch=extract(load,'brpatch','int')
address=(root/'libinterp/dec-addr.h').read_text()
helpers='\n'.join(extract(address,name,result) for name,result in [('decaddress','DecInline uchar*'),('decindirect','DecInline uchar*')]).replace('DecInline','static')
unit=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "isa.h"
#ifdef LONG32
#define ulong uint32_t
#else
#define ulong unsigned long
#endif
#ifdef WORD32
#define WORD int32_t
#define UWORD uint32_t
#else
#define WORD int64_t
#define UWORD uint64_t
#endif
typedef int64_t LONG;
typedef uint64_t uvlong;
typedef unsigned char uchar;
typedef uintptr_t uintptr;
typedef uint16_t ushort;
#define nil NULL // Portable null in the borrowed address helpers.
#define H ((void*)(uintptr)-1) // Native VM nil sentinel.
static char exBounds[]="bounds", exNilref[]="nil";


/* Retain complete width checks even when the host build defines NDEBUG. */
static void
check(int condition)
{
    if(!condition)abort();
}



/* The valid-width probe must never enter the error boundary. */
static void error(char *message)
{
    (void)message;
    abort();
}
''' + vm + '\n' + limbo + r'''
typedef struct Inst Inst;
typedef union { WORD imm,ind; Inst *ins; struct { uint16_t f,s; } i; } Adr;
struct Inst { int op; Adr s,d; };
typedef struct { Inst *prog; int nprog; } Module;
static struct { uchar *FP; Inst *PC; } R;
''' + helpers + '\n' + macro + '\n' + patch + r'''


/* Assemble unsigned canonical hi/lo halves with unchanged loader statements.
 * Return the exact LONG payload through copied native bits, without a signed
 * narrowing conversion. Scratch belongs to this synchronous probe only. */
static LONG
join(ulong hi, ulong lo)
{
    uvlong bigbits;
    LONG value;
    uchar *si = (uchar*)&value;
    BIG_STATEMENTS
    return value;
}


/* Check scalar widths without narrowing native pointer cells. */
int
main(void)
{
    static const uint64_t bits[]={0,0x8000000000000000ULL,0x3ff0000000000000ULL,
        0x3ff8000000000000ULL,0xbff8000000000000ULL,1,0x0010000000000000ULL,
        0x7fefffffffffffffULL,0x7ff0000000000000ULL,0xfff0000000000000ULL,
        0x7ff8123456789abcULL};
    static const uint32_t halves[]={0,1,0x7fffffffU,0x80000000U,0xffffffffU,
        0x01234567U,0x89abcdefU}; // Independent canonical halves, including both signed boundaries.
    ulong words[2],again[2];
    uint64_t result;
    double input,decoded;
    unsigned int i,j;
    union { void *aligned; uchar raw[32]; } area;
    uchar target[32];
    Inst instructions[2]={{0}}, pc;
    Module m={instructions,2};
    for(i=0;i<sizeof(bits)/sizeof(bits[0]);i++) {
        memcpy(&input,&bits[i],8);
        dtocanon(input,words);
        check(words[0]==(uint32_t)(bits[i]>>32) && words[1]==(uint32_t)bits[i]);
        decoded=vm_canontod(words); memcpy(&result,&decoded,8); check(result==bits[i]);
        decoded=compiler_canontod(words); memcpy(&result,&decoded,8); check(result==bits[i]);
        dtocanon(decoded,again); check(again[0]==words[0] && again[1]==words[1]);
    }
    for(i=0;i<sizeof(halves)/sizeof(halves[0]);i++)
        for(j=0;j<sizeof(halves)/sizeof(halves[0]);j++) {
            uint32_t hi=halves[i],lo=halves[j];
            check((uint64_t)join(hi,lo)==((uint64_t)hi<<32|lo));
        }
    *(uchar**)area.raw=target;
    R.FP=area.raw; R.PC=instructions;
    instructions[0].s.i.f=0; instructions[0].s.i.s=7;
    pc=instructions[0];
    check(DIND(FP,s)==target+7); /* also with a 32-bit ulong and 64-bit pointers */
    instructions[0].op=IJMP; instructions[0].d.imm=1;
    check(brpatch(&instructions[0],&m)==1 && instructions[0].d.ins==instructions+1);
    instructions[0].d.imm=2; check(brpatch(&instructions[0],&m)==0);
    return 0;
}
'''.replace('BIG_STATEMENTS',big)
with tempfile.TemporaryDirectory(prefix='vm-widths-') as td:
    td=Path(td)
    src=td/'widths.c';src.write_text(unit)
    for name,defines in [('LP64',[]),('word32-pointer64',['-DWORD32']),
                         ('LLP64-model',['-DWORD32','-DLONG32'])]:
        exe=td/name
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+
                       shlex.split(os.environ.get('CFLAGS','-O1 -g'))+
                       ['-I'+str(root/'include')]+defines+[str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print('PASS VM widths: canonical real bit patterns, 49 independent big-half pairs, indirect pointers and branch patching in three data models')
