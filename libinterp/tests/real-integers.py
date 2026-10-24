#!/usr/bin/env python3
"""Check actual real-to-integer casts and destination preservation in two word models."""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=Path(os.environ.get('XEC_SOURCE',root/'libinterp/xec.c')).read_text()
a=source.index('OP(cvtfl)');b=source.index('OP(cvtcl)',a)
fixed=source.index('OP(cvtfx)');brace=source.index('{',fixed);depth=0
for token in re.finditer(r'[{}]',source[brace:]):
    depth += 1 if token.group()=='{' else -1
    if depth==0:
        fixedbody=source[fixed:brace+token.end()];break
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
/* Stack/address checks remain active where leak-process inspection is unavailable. */
int __lsan_is_turned_off(void) {return 1;}
static char exInval[]="invalid";
/* Retain the actual destination bytes when a VM conversion fails. */
static void error(char *text) {assert(text==exInval);failed=1;longjmp(boundary,1);}
#define OP(n) static void n(void)
#define F(r) (*(REAL*)R.r)
#define W(r) (*(WORD*)R.r)
#define V(r) (*(LONG*)R.r)
BODY
/* Compare mathematical truncation of the rounded double against exact word bounds. */
static void check(double input,int alias,int big,int alignment,double scale)
{
    union Slot {double real;unsigned char bytes[144];} storage;
    unsigned char before[144];
    void *output;
    WORD word;
    LONG large;
    double rounded=big==2 ? input*scale : input;
    int bits=big==1?64:WORD_BITS;
    long double integer;
    int valid;
    memset(storage.bytes,0xa5,sizeof(storage.bytes));
    R.s=storage.bytes+24+alignment;
    R.m=storage.bytes+64+alignment;
    output=alias==0 ? storage.bytes+104+alignment : alias==1 ? R.s : alias==2 ? (char*)R.s+1 : alias==3 ? (char*)R.s-1 : R.m;
    memcpy(R.s,&input,sizeof(input));memcpy(R.m,&scale,sizeof(scale));
    memcpy(before,storage.bytes,sizeof(before));
    rounded=rounded<0?rounded-.5:rounded+.5;integer=truncl((long double)rounded);
    valid=isfinite(rounded) && integer>=-ldexpl(1.L,bits-1) && integer<ldexpl(1.L,bits-1);
    R.d=output;failed=0;
    if(setjmp(boundary)==0){if(big==1)cvtfl();else if(big==2)cvtfx();else cvtfw();}
    if(valid){
        assert(!failed);
        if(big==1){large=(LONG)integer;memcpy(before+((char*)output-(char*)storage.bytes),&large,sizeof(large));}
        else{word=(WORD)integer;memcpy(before+((char*)output-(char*)storage.bytes),&word,sizeof(word));}
    }else assert(failed);
    assert(!memcmp(before,storage.bytes,sizeof(before)));
    checks++;
}
/* Fixed conversions also capture independent finite, zero and non-finite scales. */
static void checkall(double input,int alias,int big,int alignment)
{
    double scales[]={1.,.5,-2.,0.,INFINITY,NAN};
    unsigned int i;
    for(i=0;i<(big==2 ? sizeof(scales)/sizeof(scales[0]) : 1);i++)
        check(input,alias,big,alignment,scales[i]);
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
    unsigned int i;int alias,big,alignment;
    for(i=0;i<sizeof(values)/sizeof(values[0]);i++)
        for(alignment=0;alignment<8;alignment++)for(alias=0;alias<5;alias++)for(big=0;big<3;big++){
            checkall(values[i],alias,big,alignment);
            checkall(nextafter(values[i],-INFINITY),alias,big,alignment);
            checkall(nextafter(values[i],INFINITY),alias,big,alignment);
        }
    for(i=0;i<10000;i++) {
        state=state*UINT64_C(6364136223846793005)+1;x.raw=state;
        for(alignment=0;alignment<8;alignment++)for(alias=0;alias<5;alias++)for(big=0;big<3;big++)checkall(x.real,alias,big,alignment);
    }
    printf("PASS real integers: %d rounding/bounds/exception/alias checks, WORD%d\n",checks,WORD_BITS);
    return 0;
}
'''.replace('BODY',source[a:b]+'\n'+fixedbody)
with tempfile.TemporaryDirectory(prefix='real-integers-') as directory:
    td=Path(directory);src=td/'real.c';src.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    for bits in (32,64):
        binary=td/('real'+str(bits))
        subprocess.run(cmd+['-DWORD_BITS='+str(bits),str(src),'-lm','-o',str(binary)],check=True)
        env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
        subprocess.run([str(binary)],env=env,check=True)
