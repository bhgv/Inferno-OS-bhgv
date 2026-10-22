#!/usr/bin/env python3
"""Check production fixed-type sizing before unsafe host integer conversions."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('TYPES_SOURCE', root / 'limbo/types.c')).read_text()
start = source.index('static void\nckfix(')
stop = source.index('\ndouble\nscale(', start)
unit = r'''
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
typedef int64_t vlong;
typedef uint64_t uvlong;
typedef struct Node { double rval; } Node;
typedef struct Type { Node *val; struct { int start; } src; } Type;
static int errors, checks;
/* Record diagnostics without replacing the sizing implementation. */
static void error(int location, const char *text) { (void)location; assert(text); errors++; }
BODY
/* Require rejected inputs to leave the original scale bit pattern intact. */
static void reject(double scale, double maximum) {
    Node n = {scale}; Type t = {&n,{0}};
    union { double d; uint64_t bits; } before, after;
    before.d = n.rval; errors = 0; ckfix(&t,maximum); after.d = n.rval;
    assert(errors == 1 && before.bits == after.bits); checks++;
}
/* Check finite range boundaries and representable scale normalization. */
int main(void) {
    int exponent, states;
    double s, maximum;
    Node n; Type t = {&n,{0}};
    for(exponent = -900; exponent <= 900; exponent += 30) {
        s = ldexp(1.0,exponent);
        n.rval = s; errors = 0; ckfix(&t,0); assert(!errors && n.rval == s); checks++;
        for(states = 2; states <= 32; states++) {
            /* Half the next power gives k = 2**(states-1)+1, hence p=states. */
            maximum = ldexp(s,states-2);
            n.rval = s; errors = 0; ckfix(&t,maximum);
            assert(!errors && n.rval == ldexp(s,states-32)); checks++;
        }
        reject(s,ldexp(s,31));
        reject(s,s/4);
    }
    reject(0,1); reject(-0.0,1); reject(-1,1);
    reject(NAN,1); reject(INFINITY,1); reject(-INFINITY,1);
    reject(1,NAN); reject(1,INFINITY); reject(1,-INFINITY); reject(1,-1);
    reject(DBL_MIN,DBL_MAX); reject(DBL_MAX,DBL_MIN);
    reject(DBL_MIN*DBL_EPSILON,DBL_MIN*DBL_EPSILON);
    n.rval = 1; errors = 0; ckfix(&t,2147483647.0);
    assert(!errors && n.rval == 1); checks++;
    reject(1,2147483647.5);
    printf("PASS fixed scale: %d normalization/bounds/diagnostic checks\n",checks);
    return 0;
}
'''.replace('BODY', source[start:stop])
with tempfile.TemporaryDirectory(prefix='fixed-scale-') as directory:
    td = Path(directory)
    src = td / 'scale.c'
    src.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    subprocess.run(cmd+[str(src),'-lm','-o',str(td/'scale')],check=True)
    subprocess.run([str(td/'scale')],check=True)
