#!/usr/bin/env python3
"""Execute all nine production fixed operations against arbitrary-precision oracles."""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=Path(os.environ.get('XEC_SOURCE',root/'libinterp/xec.c')).read_text()
start=source.index('static LONG\nfixedshift(') if 'static LONG\nfixedshift(' in source else source.index('OP(mulx)')
stop=source.index('/*\nOP(cvtxx)\n',start)
names=['mulx','divx','cvtxx','mulx0','divx0','cvtxx0','mulx1','divx1','cvtxx1']
unit=r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>
#if WORD_BITS == 32
typedef int32_t WORD; typedef uint32_t UWORD;
#else
typedef int64_t WORD; typedef uint64_t UWORD;
#endif
typedef int64_t LONG; typedef uint64_t uvlong;
typedef unsigned char uchar;
enum {NREG=5,IBY2WD=4};
static struct {void *s,*m,*d;uchar *FP;} R;
static WORD Stmp,Dtmp;
static jmp_buf boundary;
static int raised;
static char exInval[]="invalid",exZdiv[]="zero";
/* Translate VM exceptions to fixture status without replacing arithmetic. */
static void error(char *text) { raised=text==exZdiv?1:2; longjmp(boundary,1); }
#define OP(name) static void name(void)
#define W(r) (*(WORD*)R.r)
BODY
/* Capture byte snapshots before executing aligned, unaligned or overlapping slots. */
int executealigned(int op, WORD x, WORD y, WORD count, WORD coefficient, WORD *result,
    int alias, int sa, int ma, int da, int fa)
{
    union {uint64_t alignment;uchar bytes[144];} slots,frame;
    uchar before[144],oldframe[144];
    WORD source=y,middle=x,initial=99;
    uintptr_t address;
    if(op%3==2){source=x;middle=count;}
    memset(slots.bytes,0xa5,sizeof(slots.bytes));memset(frame.bytes,0x5a,sizeof(frame.bytes));
    R.s=slots.bytes+16+sa;R.m=slots.bytes+48+ma;R.d=slots.bytes+96+da;R.FP=frame.bytes+16+fa;
    memcpy(R.s,&source,sizeof(source));memcpy(R.m,&middle,sizeof(middle));memcpy(R.d,&initial,sizeof(initial));
    Stmp=coefficient;Dtmp=count;
    memcpy(R.FP+NREG*IBY2WD,&coefficient,sizeof(coefficient));
    memcpy(R.FP+(NREG+2)*IBY2WD,&count,sizeof(count));
    if(alias==1)R.d=R.s;
    if(alias==2)R.d=R.m;
    if(alias==3)R.d=(uchar*)R.s+1;
    if(alias==4)R.d=(uchar*)R.m-1;
    if(alias==5)R.d=R.FP+NREG*IBY2WD;
    if(alias==6)R.d=R.FP+(NREG+2)*IBY2WD;
    memcpy(before,slots.bytes,sizeof(before));memcpy(oldframe,frame.bytes,sizeof(oldframe));
    memcpy(&initial,R.d,sizeof(initial));raised=0;
    if(setjmp(boundary)==0){switch(op){ CASES }}
    memcpy(result,R.d,sizeof(*result));
    if(raised)assert(*result==initial);
    else {
        address=(uintptr_t)R.d;
        if(address>=(uintptr_t)slots.bytes && address<(uintptr_t)(slots.bytes+sizeof(slots.bytes)))
            memcpy(before+(address-(uintptr_t)slots.bytes),result,sizeof(*result));
        else memcpy(oldframe+(address-(uintptr_t)frame.bytes),result,sizeof(*result));
    }
    assert(!memcmp(before,slots.bytes,sizeof(before)) && !memcmp(oldframe,frame.bytes,sizeof(oldframe)));
    return raised;
}
/* Keep the existing Python oracle entry point independent of slot placement. */
int execute(int op, WORD x, WORD y, WORD count, WORD coefficient, WORD *result, int alias)
{
    return executealigned(op,x,y,count,coefficient,result,alias,0,0,0,0);
}
/* Keep stack instrumentation without unsupported leak-process inspection. */
int __lsan_is_turned_off(void) {return 1;}
#ifdef SELFTEST
/* Exercise native sanitizer execution without loading ASan into the Python host. */
int main(void)
{
    WORD edges[]={0,1,-1,(WORD)((UWORD)1<<(WORD_BITS-1)),
        (WORD)(((UWORD)1<<(WORD_BITS-1))-1)};
    WORD output;
    int op,i,j,alias,count,align,checks=0;
    for(op=0;op<9;op++) for(i=0;i<5;i++) for(j=0;j<5;j++)
        for(alias=0;alias<7;alias++) for(align=0;align<8;align++) for(count=-64;count<=64;count++) {
            int status=executealigned(op,edges[i],edges[j],op>=6?count*4:count,
                edges[(i+j)%5],&output,alias,align,(align*3+1)%8,(align*5+2)%8,(align*7+3)%8);
            assert(status>=0 && status<=2); checks++;
        }
    printf("PASS fixed native sanitizer: %d cases, WORD%d\n",checks,WORD_BITS);
    return 0;
}
#endif
'''.replace('BODY',source[start:stop]).replace('CASES',' '.join(f'case {i}: {n}(); break;' for i,n in enumerate(names)))


# Interpret two's-complement storage independently using unbounded Python integers.
def signed(value,bits=64):
    value &= (1<<bits)-1
    return value-(1<<bits) if value>>(bits-1) else value


# C signed division truncates toward zero, including negative operands.
def divide(value,denominator):
    return signed((abs(value)//abs(denominator))*(-1 if (value<0)!=(denominator<0) else 1))


# Model VM instructions, with width conversion at each specified storage boundary.
def oracle(op,x,y,count,a,bits):
    variant,kind=divmod(op,3)
    if kind==1 and y==0 or variant and kind!=1 and a==0:
        return 1,None
    if variant and (x==0 or kind==0 and y==0):
        return 0,0
    p=count if variant!=2 else count>>2
    if not -63<=p<=63:
        return 2,None
    v=w=0
    if variant==2:
        vnz,wnz=count&2,count&1
        if vnz:
            v=1 if kind==1 else signed(a-1,bits)
            if (kind==2 and x<0) or (kind!=2 and (x<0)!=(y<0)):
                v=signed(-v,bits)
        need=False
        if wnz:
            if kind==1: need=x<=0
            elif kind==2: need=(not vnz and x<0) or (vnz and x>0)
            else: need=(not vnz and x*y<0) or (vnz and x*y>0)
        if need:
            if p>=0: return 2,None
            w=(1<<(-p))-1
    value=x*y if kind==0 else a*x if kind==1 and variant else x
    value=signed(value+w)
    value=signed(value<<p) if p>=0 else value>>(-p)
    if kind!=1: value=signed(value+v)
    if kind==1: value=divide(value,y)
    elif variant: value=divide(value,a)
    if kind==1: value+=v
    return 0,signed(value,bits)


with tempfile.TemporaryDirectory(prefix='fixed-runtime-') as directory:
    td=Path(directory);src=td/'ops.c';src.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (32,64):
        binary=td/('ops'+str(bits)+'.so')
        executable=td/('native'+str(bits))
        subprocess.run(cmd+['-DWORD_BITS='+str(bits),'-DSELFTEST',str(src),'-o',str(executable)],check=True)
        environment=dict(os.environ)
        environment.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([str(executable)],check=True,env=environment)
        shared=[]
        for flag in cmd:
            if flag.startswith('-fsanitize='):
                modes=[m for m in flag.split('=',1)[1].split(',') if m!='address']
                if modes: shared.append('-fsanitize='+','.join(modes))
            else: shared.append(flag)
        subprocess.run(shared+['-shared','-fPIC','-DWORD_BITS='+str(bits),str(src),'-o',str(binary)],check=True)
        lib=C.CDLL(str(binary))
        word=C.c_int32 if bits==32 else C.c_int64
        lib.execute.argtypes=[C.c_int,word,word,word,word,C.POINTER(word),C.c_int]
        lib.executealigned.argtypes=lib.execute.argtypes+[C.c_int]*4
        rng=random.Random(0x666978+bits)
        limits=[-(1<<(bits-1)),(1<<(bits-1))-1,-1,0,1,2,-2,2147483647,-2147483648]
        checks=0
        for op in range(9):
            for case in range(1400):
                x=rng.choice(limits) if case<700 else signed(rng.getrandbits(bits),bits)
                y=rng.choice(limits) if case<700 else signed(rng.getrandbits(bits),bits)
                a=rng.choice(limits) if case<700 else signed(rng.getrandbits(bits),bits)
                shift=rng.choice([-63,-62,-32,-31,-1,0,1,31,32,62,63])
                flags=rng.randrange(4) if shift<0 else rng.choice([0,2])
                count=shift*4+flags if op>=6 else shift
                if case%17==0: count=rng.choice([64,-64,-(1<<(bits-1)),(1<<(bits-1))-1])
                status,expected=oracle(op,x,y,count,a,bits)
                for alias in (0,1,2):
                    old=(x if op%3==2 else y) if alias==1 else (count if op%3==2 else x) if alias==2 else 99
                    output=word()
                    align=case%8
                    got=lib.executealigned(op,x,y,count,a,C.byref(output),alias,align,(align*3+1)%8,(align*5+2)%8,(align*7+3)%8)
                    assert got==status,(bits,names[op],x,y,count,a,got,status)
                    assert output.value==(old if status else expected),(bits,names[op],x,y,count,a,output.value,expected)
                    checks+=1
        print(f'PASS fixed runtime: {checks} modular/shift/division/rounding/alias cases, WORD{bits}')
