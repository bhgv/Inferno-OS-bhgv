#!/usr/bin/env python3
"""Check actual fixed opcode selection, exact powers, ratios and encoded shifts."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('TYPES_SOURCE',root / 'limbo/types.c')).read_text()
start = source.index('#define I(x)')
stop = source.index('\nint\nispoly(',start)
runtime=(root/'libinterp/xec.c').read_text()
vm=runtime[runtime.index('static WORD\nfixedword('):runtime.index('/*\nOP(cvtxx)\n',runtime.index('static WORD\nfixedword('))]
unit = r'''
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "isa.h"
typedef int64_t Long;
typedef int64_t vlong;
typedef struct Type { double factor; struct {int start;} src; } Type;
static int errors, checks;




/* Keep regression checks enabled independently of NDEBUG. */
static void
check(int condition)
{
    if(!condition){
        fputs("fixed selection check failed\n", stderr);
        exit(1);
    }
}



/* Keep source scales explicit instead of substituting selection arithmetic. */
static double
scale(Type *t)
{
    return t->factor;
}


/* Record compiler diagnostics for rejected scale and shift combinations. */
static void
error(int line, const char *text)
{
    (void)line;
    check(text != NULL);
    errors++;
}


/* Unexpected internal fatal paths fail the fixture immediately. */
static void
fatal(const char *text)
{
    (void)text;
    abort();
}
BODY


/* Link actual WORD32 VM instruction bodies to the selected compiler parameters. */
typedef int32_t WORD; typedef uint32_t UWORD;
typedef int64_t LONG; typedef uint64_t uvlong;
typedef uintptr_t uintptr; // Native decoded-slot/frame span arithmetic.
static struct {
    void *s; // Source operand cell.
    void *m; // Middle operand cell.
    void *d; // Destination operand cell.
    char *FP; // VM frame holding compiler-selected coefficients.
} R;
static WORD frame[NREG+3];
#define nil NULL // Ordinary null frame identity.
#define H ((void*)(uintptr_t)~(uintptr_t)0) // VM nil sentinel.

static char exInval[]="invalid",exZdiv[]="zero",exNilref[]="nil frame";
static char exBounds[]="bounds"; // Malformed slot or wrapping frame prefix.


/* Selected valid parameters must never raise a VM exception. */
static void
vmerror(char *text)
{
    (void)text;
    abort();
}
#define error vmerror // Route VM errors to the fixture failure handler.
#define OP(n) static void n(void) // Keep the production opcode declarations.
#define W(r) (*(WORD*)R.r) // Address WORD32 operand cells.
VM
#undef error


/* Run public selection and verify its emitted-parameter safety contract. */
static int
selectop(int input, double sx, double sy, double sr, int *p, int *a)
{
    Type x={sx,{0}},y={sy,{0}},r={sr,{0}};
    int op, shift;
    errors=0; *p=123456; *a=123456;
    op=fixop(input,&x,&y,&r,p,a);
    if(op != IMOVW) {
        check(!errors);
        check(op==IMULX || op==IDIVX || op==ICVTXX ||
               op==IMULX0 || op==IDIVX0 || op==ICVTXX0 ||
               op==IMULX1 || op==IDIVX1 || op==ICVTXX1);
        shift=*p;
        if(op==IMULX1 || op==IDIVX1 || op==ICVTXX1) shift >>= 2;
        check(shift >= -63 && shift <= 63);
        if(op==IMULX0 || op==IDIVX0 || op==ICVTXX0 ||
           op==IMULX1 || op==IDIVX1 || op==ICVTXX1) check(*a > 0);
    }
    checks++;
    return op;
}


/* Exact powers have direct shifts; neighboring doubles must take coefficient paths. */
static void
powers(void)
{
    int exponent,p,a,op;
    double ratio;
    for(exponent=-60;exponent<=60;exponent++) {
        ratio=ldexp(1.,exponent);
        check(ispow2(ratio));
        check(!ispow2(nextafter(ratio,0.)) && !ispow2(nextafter(ratio,INFINITY)));
        op=selectop(IMULX,ratio,1,1,&p,&a);
        check(op==IMULX && p==exponent && a==0);
        op=selectop(IDIVX,ratio,1,1,&p,&a);
        check(op==IDIVX && p==exponent && a==0);
        op=selectop(ICVTXX,ratio,1,1,&p,&a);
        check(op==ICVTXX && p==exponent && a==0);
        checks+=3;
    }
    check(!ispow2(0) && !ispow2(-1) && !ispow2(NAN) && !ispow2(INFINITY));
    checks++;
}


/* Real powers handle negative coefficient exponents and sign-packed parameters. */
static void
coefficients(void)
{
    int e,p,a,op,input;
    double ratios[]={1.125,1.25,1.5,1.75,1.999999,2.000001};
    unsigned int i;
    for(e=-100;e<=100;e++) for(i=0;i<sizeof(ratios)/sizeof(ratios[0]);i++) {
        for(input=0;input<3;input++) {
            op=selectop(input==0?IMULX:input==1?IDIVX:ICVTXX,
                        ldexp(ratios[i],e),1,1,&p,&a);
            if(op!=IMOVW && !errors)
                check(op!=(input==0?IMULX:input==1?IDIVX:ICVTXX));
        }
    }
    /* Tiny non-power casts must preserve the compiler's zero-result sentinel. */
    check(selectop(ICVTXX,ldexp(1.5,-100),1,1,&p,&a)==IMOVW && !errors);
    /* Cases with exactly representable coefficients have known arithmetic effects. */
    op=selectop(ICVTXX,1.5,1,1,&p,&a);
    check(op==ICVTXX0 || op==ICVTXX1);
    for(e=-10000;e<=10000;e++) {
        WORD input=e,count=p,output=0;
        R.s=&input;R.m=&count;R.d=&output;
        R.FP=(char*)frame;
        frame[NREG]=a;
        if(op==ICVTXX0) cvtxx0(); else cvtxx1();
        check(output==(int64_t)(e*1.5));
        checks++;
    }
}


/* Extreme finite scales can cancel without losing the representable ratio. */
static void
extremes(void)
{
    int p,a,op,input,e;
    double invalid[]={0,-1,NAN,INFINITY,-INFINITY};
    unsigned int i;
    for(input=0;input<3;input++) {
        for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
            op=selectop(input==0?IMULX:input==1?IDIVX:ICVTXX,invalid[i],1,1,&p,&a);
            check(op==IMOVW && errors==1 && p==0 && a==0);
        }
        for(e=-1100;e<=1100;e+=10)
            selectop(input==0?IMULX:input==1?IDIVX:ICVTXX,ldexp(1.5,e),1,1,&p,&a);
    }
    op=selectop(IMULX,ldexp(1.,800),ldexp(1.,225),ldexp(1.,1000),&p,&a);
    check(op==IMULX && p==25 && !errors);
    op=selectop(IDIVX,ldexp(1.,-1000),ldexp(1.,-800),ldexp(1.,-225),&p,&a);
    check(op==IDIVX && p==25 && !errors);
    op=selectop(IMULX,DBL_MAX,DBL_MAX,1,&p,&a);
    check(op==IMOVW && errors==1);
}


/* Bound all regression loops and require actual public selection diagnostics. */
int
main(void)
{
    powers();coefficients();extremes();
    printf("PASS fixed selection: %d opcode/ratio/power/arithmetic checks\n",checks);
    return 0;
}
'''.replace('BODY',source[start:stop]).replace('VM\n',vm+'\n')
with tempfile.TemporaryDirectory(prefix='fixed-ops-') as directory:
    td=Path(directory)
    src=td/'ops.c';src.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    subprocess.run(cmd+['-I'+str(root/'include'),str(src),'-lm','-o',str(td/'ops')],check=True)
    subprocess.run([str(td/'ops')],check=True,timeout=15)
