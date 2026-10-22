#!/usr/bin/env python3
"""Independent exact-integer/rational oracles for production scaled/real ops.

The host harness stubs VM errors and temporary frame slots; no target boot.
"""
import math
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile
from fractions import Fraction

root=Path(__file__).resolve().parents[2]
source=(root/'libinterp/xec.c').read_text()
helpers=source[source.index('/* Integer contract:'):source.index('OP(runt)')]
names=['mulx','divx','cvtxx','mulx0','divx0','cvtxx0','mulx1','divx1','cvtxx1','cvtfw','cvtfl','cvtfx']
def body(name):
    a=source.index('OP('+name+')');b=source.index('{',a)+1;depth=1
    while depth:
        depth+=(source[b]=='{')-(source[b]=='}');b+=1
    return source[a:b]
unit=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <fenv.h>
#ifdef WORD32
typedef int32_t WORD;typedef uint32_t UWORD;
#else
typedef int64_t WORD;typedef uint64_t UWORD;
#endif
typedef int64_t LONG;typedef uint64_t ULONG;typedef double REAL;
static struct {void *s,*m,*d;} R;
static WORD Stmp,Dtmp;
static jmp_buf unwind;
static int caught;
static char *exZdiv="zero divide",*exRange="value out of range";
static void error(char *s){caught=!strcmp(s,exZdiv)?1:!strcmp(s,exRange)?3:2;longjmp(unwind,1);}
#define OP(n) static void n(void)
#define W(r) (*(WORD*)R.r)
#define F(r) (*(REAL*)R.r)
#define V(r) (*(LONG*)R.r)
'''+helpers+'\n'.join(body(n) for n in names)+r'''
static void (*functions[])(void)={FUNCTIONS};
int main(void){
 unsigned op,err,alias;unsigned long long s,m,a,p,want;
 union {uint64_t u;WORD w;LONG l;REAL f;} slots[3];
 while(scanf("%u %llx %llx %llx %llx %llx %u %u",&op,&s,&m,&a,&p,&want,&err,&alias)==8){
  int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
  int rounds=(op==9 || op==10)?4:1;
  for(int ri=0;ri<rounds;ri++){
  if(fesetround(modes[ri]))return 13;
  slots[0].u=s;slots[1].u=m;slots[2].u=0xa5;Stmp=(WORD)a;Dtmp=(WORD)p;
  R.s=&slots[0];R.m=&slots[1];R.d=&slots[alias];caught=0;
  if(!setjmp(unwind))functions[op]();
  if(caught!=err){fprintf(stderr,"op=%u err=%d expected=%u s=%llx m=%llx a=%llx p=%llx\n",op,caught,err,s,m,a,p);return 10;}
  if(caught){if(slots[0].u!=s||slots[1].u!=m||slots[2].u!=0xa5)return 11;}
  else {
   uint64_t mask=op==10?UINT64_MAX:(uint64_t)(UWORD)~(UWORD)0;
   if((slots[alias].u&mask)!=want){fprintf(stderr,"op=%u got=%llx want=%llx s=%llx m=%llx a=%llx p=%llx\n",op,(unsigned long long)(slots[alias].u&mask),want,s,m,a,p);return 12;}
  }
  }
 }
 return 0;
}
'''.replace('FUNCTIONS',','.join(names))
class VMError(Exception):
    pass
def signed(v,b):
    v&=(1<<b)-1
    return v-(1<<b) if v>>(b-1) else v
def wrap(v):return signed(v,64)
def shift(v,p):
    if abs(p)>=64:raise VMError(2)
    return wrap(v<<p) if p>=0 else v>>-p
def divide(v,d):
    if d==0:raise VMError(1)
    q=abs(v)//abs(d)
    return wrap(-q if (v<0)!=(d<0) else q)
def bias(p):
    if p>0 or p<=-64:raise VMError(2)
    return (1<<-p)-1

def fixed(name,s,m,a,p,wb):
    conv=name.startswith('cvt');mul=name.startswith('mul')
    x=s if conv else m;y=s
    if conv:p=m
    variant=int(name[-1]) if name[-1].isdigit() else -1
    if variant==1:flags=p&3;p=p>>2
    else:flags=0
    den=a if conv or mul else y
    if variant>=0 and (x==0 or mul and y==0):
        shift(0,p);divide(0,den);return 0
    v=w=0
    if flags&2:
        v=1 if not (conv or mul) else signed(a-1,wb)
        if x<0 if conv else (x<0)!=(y<0):v=signed(-v,wb)
    if flags&1:
        if conv:needed=(not(flags&2) and x<0) or (flags&2 and x>0)
        elif mul:needed=(not(flags&2) and x*y<0) or (flags&2 and x*y>0)
        else:needed=x<=0
        if needed:w=bias(p)
    if conv:r=wrap(x+w)
    elif mul:r=wrap(x*y+w)
    else:r=wrap((a if variant>=0 else 1)*x+w)
    r=shift(r,p)
    if variant>=0 and (conv or mul):r=divide(wrap(r+v),a)
    elif not (conv or mul):r=divide(r,y)
    if variant==1 and not (conv or mul):r+=v
    return signed(r,wb)

def realbits(f):return struct.unpack('<Q',struct.pack('<d',f))[0]
def realoracle(f,bits):
    if not math.isfinite(f):raise VMError(3)
    q=Fraction.from_float(abs(f))+Fraction(1,2)
    v=q.numerator//q.denominator
    if f<0:v=-v
    if not -(1<<(bits-1))<=v<(1<<(bits-1)):raise VMError(3)
    return v
rng=random.Random(0xf18ed)
total=0
with tempfile.TemporaryDirectory(prefix='scaled-arithmetic-') as td:
    td=Path(td);src=td/'test.c';src.write_text(unit)
    for wb in (32,64):
        mask=(1<<wb)-1;records=[]
        vals=[0,1,-1,2,-2,(1<<(wb-1))-1,-(1<<(wb-1))]
        scales=[-65,-64,-63,-32,-1,0,1,31,32,63,64,65,-(1<<(wb-1))]
        for op,name in enumerate(names[:9]):
            cases=[(s,m,a,p) for s in vals for m in vals for a in [0,1,-1,3] for p in scales]
            cases += [tuple(signed(rng.getrandbits(wb),wb) for _ in range(4)) for _ in range(1000)]
            for _ in range(4000):
                sv,mv,av=(signed(rng.getrandbits(wb),wb) for _ in range(3))
                scale=rng.choice(scales[:-1])
                if name.endswith('1'):scale=signed(scale*4+rng.randrange(4),wb)
                if name.startswith('cvt'):mv=scale
                cases.append((sv,mv,av,scale))
            # Packed rounding flags and usable valid scale coverage.
            if name.endswith('1'):
                cases += [(s,m,a,signed(p*4+flags,wb)) for s in vals for m in vals for a in [1,-1,3] for p in [-63,-2,-1,0,1,63] for flags in range(4)]
            for s,m,a,p in cases:
                err=0;want=0
                try:want=fixed(name,s,m,a,p,wb)
                except VMError as e:err=e.args[0]
                for alias in range(3):records.append(f'{op} {s&mask:x} {m&mask:x} {a&mask:x} {p&mask:x} {want&mask:x} {err} {alias}\n')
        floats=[0.,-0.,.49,.5,-.5,1.5,-1.5,float('nan'),float('inf'),-float('inf')]
        for bits in (32,64):
            bound=float(1<<(bits-1))
            for v in [-bound,bound,-bound-.5,-bound+.5,bound-1.]:
                floats.extend([v,math.nextafter(v,-math.inf),math.nextafter(v,math.inf)])
        floats.extend([math.ldexp(1.,e) for e in (51,52,53,62,63,64,1023)])
        floats += [struct.unpack('<d',struct.pack('<Q',rng.getrandbits(64)))[0] for _ in range(4000)]
        for op in range(9,12):
            for f in floats:
                for factor in ([0.,-1.,1.,.5,2.,float('inf')] if op==11 else [1.]):
                    bits=64 if op==10 else wb;err=0;want=0
                    try:want=realoracle(f*factor if op==11 else f,bits)
                    except VMError as e:err=e.args[0]
                    for alias in range(3):records.append(f'{op} {realbits(f):x} {realbits(factor):x} 0 0 {want&((1<<bits)-1):x} {err} {alias}\n')
        exe=td/f'word{wb}'
        flags=os.environ.get('CFLAGS','-O2 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all')
        subprocess.run(shlex.split(os.environ.get('CC','cc'))+shlex.split(flags)+(['-DWORD32'] if wb==32 else [])+[str(src),'-o',str(exe),'-lm'],check=True)
        subprocess.run([str(exe)],input=''.join(records),text=True,check=True)
        total+=len(records)+3*sum(r.startswith(('9 ','10 ')) for r in records)
print(f'PASS scaled/real arithmetic: {total} exact-oracle cases, WORD32/64, 12 opcodes, aliases, rounding, malformed scales, zero divisors, NaN/infinity, range errors and four rounding modes')
