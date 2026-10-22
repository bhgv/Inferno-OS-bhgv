#!/usr/bin/env python3
"""Check actual real-to-fixed casts and destination preservation in two word models."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=Path(os.environ.get('XEC_SOURCE',root/'libinterp/xec.c')).read_text()
a=source.index('OP(cvtfx)');b=source.index('OP(cvtxf)',a)
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#if WORD_BITS == 32
typedef int32_t WORD;
#else
typedef int64_t WORD;
#endif
typedef double REAL;
static struct {void *s,*m,*d;} R;
static jmp_buf boundary;
static int failed,checks;
static char exInval[]="invalid";
/* Retain the actual destination bytes when a VM conversion fails. */
static void error(char *text) {assert(text==exInval);failed=1;longjmp(boundary,1);}
#define OP(n) static void n(void)
#define F(r) (*(REAL*)R.r)
#define W(r) (*(WORD*)R.r)
BODY
/* Compare mathematical truncation of the rounded double against exact word bounds. */
static void check(double input,double factor,int alias)
{
    union Slot {double real;WORD word;uint64_t raw;} s,m,d,before,*output;
    double rounded=input*factor;
    long double integer;
    int valid;
    s.real=input;m.real=factor;d.raw=UINT64_C(0x1122334455667788);
    output=alias==1?&s:alias==2?&m:&d;before=*output;
    rounded=rounded<0?rounded-.5:rounded+.5;integer=truncl((long double)rounded);
    valid=isfinite(rounded) && integer>=-ldexpl(1.L,WORD_BITS-1) && integer<ldexpl(1.L,WORD_BITS-1);
    R.s=&s.real;R.m=&m.real;R.d=&output->word;failed=0;
    if(setjmp(boundary)==0)cvtfx();
    if(valid)assert(!failed && output->word==(WORD)integer);
    else assert(failed && !memcmp(&before,output,sizeof(before)));
    checks++;
}
/* Exercise ties, width boundaries, exceptional products and arbitrary bit patterns. */
int main(void)
{
    double values[]={0,-0.,.5,-.5,1.5,-1.5,2147483647.,2147483647.5,
        -2147483648.,-2147483648.5,2147483648.,-2147483649.,
        0x1p63,-0x1p63,0x1.fffffffffffffp62,-0x1.0000000000001p63,
        INFINITY,-INFINITY,NAN,1e300,-1e300,0x1p-1074};
    double factors[]={0,-0.,1,-1,.25,1.5,3,1e-300,1e300,INFINITY,-INFINITY,NAN};
    uint64_t state=UINT64_C(0x7265616c666978);
    union {uint64_t raw;double real;} x,y;
    unsigned int i,j;int alias;
    for(i=0;i<sizeof(values)/sizeof(values[0]);i++)
        for(j=0;j<sizeof(factors)/sizeof(factors[0]);j++)
            for(alias=0;alias<3;alias++)check(values[i],factors[j],alias);
    for(i=0;i<10000;i++) {
        state=state*UINT64_C(6364136223846793005)+1;x.raw=state;
        state=state*UINT64_C(6364136223846793005)+1;y.raw=state;
        for(alias=0;alias<3;alias++)check(x.real,y.real,alias);
    }
    printf("PASS real fixed: %d rounding/bounds/exception/alias checks, WORD%d\n",checks,WORD_BITS);
    return 0;
}
'''.replace('BODY',source[a:b])
with tempfile.TemporaryDirectory(prefix='real-fixed-') as directory:
    td=Path(directory);src=td/'real.c';src.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    for bits in (32,64):
        binary=td/('real'+str(bits))
        subprocess.run(cmd+['-DWORD_BITS='+str(bits),str(src),'-lm','-o',str(binary)],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([str(binary)],env=env,check=True)
