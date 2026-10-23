#!/usr/bin/env python3
"""Check actual real-to-integer casts and destination preservation in two word models."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=Path(os.environ.get('XEC_SOURCE',root/'libinterp/xec.c')).read_text()
a=source.index('OP(cvtfl)');b=source.index('OP(cvtcl)',a)
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
typedef int64_t LONG;
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
#define V(r) (*(LONG*)R.r)
BODY
/* Compare mathematical truncation of the rounded double against exact word bounds. */
static void check(double input,int alias,int big)
{
    union Slot {double real;WORD word;LONG big;uint64_t raw;} s,d,before,*output;
    double rounded=input;
    int bits=big?64:WORD_BITS;
    long double integer;
    int valid;
    s.real=input;d.raw=UINT64_C(0x1122334455667788);
    output=alias?&s:&d;before=*output;
    rounded=rounded<0?rounded-.5:rounded+.5;integer=truncl((long double)rounded);
    valid=isfinite(rounded) && integer>=-ldexpl(1.L,bits-1) && integer<ldexpl(1.L,bits-1);
    R.s=&s.real;R.d=&output->word;failed=0;
    if(setjmp(boundary)==0){if(big)cvtfl();else cvtfw();}
    if(valid)assert(!failed && (big ? output->big==(LONG)integer : output->word==(WORD)integer));
    else assert(failed && !memcmp(&before,output,sizeof(before)));
    checks++;
}
/* Exercise ties, width boundaries, non-finite values and arbitrary bit patterns. */
int main(void)
{
    double values[]={0,-0.,.5,-.5,1.5,-1.5,2147483647.,2147483647.5,
        -2147483648.,-2147483648.5,2147483648.,-2147483649.,
        0x1p63,-0x1p63,0x1.fffffffffffffp62,-0x1.0000000000001p63,
        INFINITY,-INFINITY,NAN,1e300,-1e300,0x1p-1074};
    uint64_t state=UINT64_C(0x7265616c666978);
    union {uint64_t raw;double real;} x;
    unsigned int i;int alias,big;
    for(i=0;i<sizeof(values)/sizeof(values[0]);i++)
        for(alias=0;alias<2;alias++)for(big=0;big<2;big++){
            check(values[i],alias,big);
            check(nextafter(values[i],-INFINITY),alias,big);
            check(nextafter(values[i],INFINITY),alias,big);
        }
    for(i=0;i<10000;i++) {
        state=state*UINT64_C(6364136223846793005)+1;x.raw=state;
        for(alias=0;alias<2;alias++)for(big=0;big<2;big++)check(x.real,alias,big);
    }
    printf("PASS real integers: %d rounding/bounds/exception/alias checks, WORD%d\n",checks,WORD_BITS);
    return 0;
}
'''.replace('BODY',source[a:b])
with tempfile.TemporaryDirectory(prefix='real-integers-') as directory:
    td=Path(directory);src=td/'real.c';src.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    for bits in (32,64):
        binary=td/('real'+str(bits))
        subprocess.run(cmd+['-DWORD_BITS='+str(bits),str(src),'-lm','-o',str(binary)],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([str(binary)],env=env,check=True)
