#!/usr/bin/env python3
"""Production integer helpers against an independent Python oracle, WORD32/64.

Executes opcode bodies with register aliases and catches VM errors. No OS boot.
"""
import os
from pathlib import Path
import random
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
text = (root/'libinterp/xec.c').read_text()
helpers = text[text.index('/* Integer contract:'):text.index('OP(runt)')]
names = [a+b for a in ('add','sub','mul','div','mod','shl','shr') for b in ('b','w','l')]+['lsrw','lsrl']
ops = '\n'.join(re.search(r'OP\('+n+r'\)\s*\{[^}]*\}',text).group() for n in names)
unit = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#ifdef WORD32
typedef int32_t WORD; typedef uint32_t UWORD;
#else
typedef int64_t WORD; typedef uint64_t UWORD;
#endif
typedef int64_t LONG; typedef uint64_t ULONG;
static struct {void *s,*m,*d;} R;
static jmp_buf unwind;
static char *exZdiv="zero divide";
static int caught;
static void error(char *s) {caught=!strcmp(s,exZdiv)?1:2;longjmp(unwind,1);}
#define OP(n) static void n(void)
#define B(r) (*(uint8_t*)R.r)
#define W(r) (*(WORD*)R.r)
#define V(r) (*(LONG*)R.r)
'''+helpers+ops+r'''
static void (*functions[])(void)={FUNCTIONS};
int main(void) {
    unsigned op,alias,expectederr;
    unsigned long long s,m,want;
    union {uint64_t u; WORD w; LONG l; uint8_t b;} slots[3];
    while(scanf("%u %llx %llx %llx %u %u",&op,&s,&m,&want,&expectederr,&alias)==6) {
        slots[0].u=s;slots[1].u=m;slots[2].u=0xa5;
        R.s=&slots[0];R.m=&slots[1];R.d=&slots[alias];
        caught=0;
        if(setjmp(unwind)==0) functions[op]();
        if(caught!=expectederr) return 10;
        if(caught) {
            if(slots[0].u!=s || slots[1].u!=m || slots[2].u!=0xa5) return 11;
        } else {
            uint64_t mask=op<21 && op%3==0 ? 255 :
                (op<21 && op%3==1) || op==21 ? (uint64_t)(UWORD)~(UWORD)0 : UINT64_MAX;
            if((slots[alias].u&mask)!=want) {
                fprintf(stderr,"op %u s=%llx m=%llx got=%llx expected=%llx alias=%u\n",op,s,m,(unsigned long long)(slots[alias].u&mask),want,alias);
                return 12;
            }
        }
    }
    return 0;
}
'''.replace('FUNCTIONS',','.join(names))
rng = random.Random(0x123da)
total=0
with tempfile.TemporaryDirectory(prefix='integer-arithmetic-') as td:
    td=Path(td);src=td/'test.c';src.write_text(unit)
    for wb in (32,64):
        records=[]
        def signed(n,b):
            n &= (1<<b)-1
            return n-(1<<b) if n>>(b-1) else n
        for op,name in enumerate(names):
            kind=name[-1];bits=8 if kind=='b' else wb if kind=='w' else 64
            mask=(1<<bits)-1; wmask=(1<<wb)-1
            vals=[0,1,2,mask,mask-1,1<<(bits-1),(1<<(bits-1))-1]
            counts=[-1,0,1,7,8,31,32,63,64,65,255,256,1<<(wb-1)]
            pairs=[(s,m) for s in (counts if name.startswith(('sh','lsr')) else vals) for m in vals]
            pairs += [(rng.getrandbits(wb if name.startswith(('sh','lsr')) else bits),rng.getrandbits(bits)) for _ in range(500)]
            for s,m in pairs:
                shift=name.startswith(('sh','lsr'));sv=signed(s,wb) if shift else s&mask if kind=='b' else signed(s,bits)
                mv=m&mask if kind=='b' else signed(m,bits)
                err=0;want=0
                if shift:
                    if sv<0 or sv>=bits:err=2
                    elif name.startswith('shl'):want=(m&mask)<<sv
                    elif name.startswith('lsr'):want=(m&mask)>>sv
                    else:want=mv>>sv
                elif name.startswith('add'):want=mv+sv
                elif name.startswith('sub'):want=mv-sv
                elif name.startswith('mul'):want=mv*sv
                elif sv==0:err=1
                else:
                    q=abs(mv)//abs(sv);q=-q if (mv<0)!=(sv<0) else q
                    want=q if name.startswith('div') else mv-q*sv
                for alias in range(3):
                    records.append(f'{op} {s & (wmask if shift else mask):x} {m&mask:x} {want&mask:x} {err} {alias}\n')
        exe=td/f'word{wb}'
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))+(['-DWORD32'] if wb==32 else [])+[str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],input=''.join(records),text=True,check=True)
        total+=len(records)
print(f'PASS integer arithmetic: {total} oracle cases, WORD32/64, byte/big, aliased destinations, zero division and invalid shifts (UBSan by default)')
