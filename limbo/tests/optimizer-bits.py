#!/usr/bin/env python3
"""Exercise actual optimizer bit sets with 32/64-bit host ulong models."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('OPTIM_SOURCE', root / 'limbo/optim.c')).read_text()
macros = source[source.index('#define BLEN'):source.index('#define isnilsrc')]
enums = source[source.index('enum\n{\n\tBclr'):source.index('enum\n{\n\tSuse')]
struct = source[source.index('struct Bits\n'):source.index('struct Blist\n')]
body = source[source.index('static int\nbitc('):source.index('static char*\ndecname(')]
unit = r'''

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
typedef unsigned int uint;
typedef uint32_t u32int;
typedef uint64_t uvlong;
#ifdef LONG32
typedef uint32_t hostulong;
#else
typedef uint64_t hostulong;
#endif
#define ulong hostulong
#define bzero bbzero
typedef struct Bits Bits;
MACROS
ENUMS
STRUCT
/* Preserve instrumented checks without unavailable process-based leak scanning. */
int
__lsan_is_turned_off(void)
{
    return 1;
}


/* Keep inherited bitmap predicates active in optimized fixture builds. */
static void
bitpredicate(int condition)
{
    if(!condition)
        abort();
}
#define bitcheck(condition) bitpredicate(!!(condition)) // Normalize scalar and pointer predicates.
static unsigned long checks;
static int printed[1024], nprinted;
/* Track diagnostic positions instead of parsing formatted output. */
static void print(const char *format, ...) {
    va_list ap; (void)format;
    va_start(ap, format); printed[nprinted++] = va_arg(ap, int); va_end(ap);
}
/* Keep production allocation and zero-length semantics bounded and explicit. */
static void *allocmem(ulong n) { void *p = malloc(n ? n : 1); bitcheck(p); return p; }
/* Abort on a production invariant violation. */
static void fatal(const char *format, ...) { (void)format; abort(); }
BODY
/* Compare every position and aggregate query against a byte-array oracle. */
static void verify(Bits b, const unsigned char *model, int count) {
    int i, total = 0, first = -1, last = -1;
    for(i = 0; i < count; i++) {
        bitcheck(!!bmem(b,i) == !!model[i]);
        if(model[i]) { if(first < 0) first = i; last = i; total++; }
        checks++;
    }
    bitcheck(bitcnt(b) == total && lowbit(b) == first && topbit(b) == last);
    bitcheck(!!bzero(b) == (total == 0));
    nprinted = 0; pbits(b); bitcheck(nprinted == total);
    for(i = 0; i < nprinted; i++) bitcheck(model[printed[i]]);
    checks += 4;
}
/* Supply varied patterns with deterministic unsigned arithmetic. */
static uint32_t randomword(void) {
    static uint32_t state = 0x6c696d62;
    state = state*1664525U+1013904223U; return state;
}
/* Evaluate the Boolean truth table independently of production cell masks. */
static int truth(int op, int s, int d) {
    switch(op) {
    case Bclr: return 0; case Band: return s&&d;
    case Bandinv: return s&&!d; case Bstore: return s;
    case Bandrev: return !s&&d; case Bnoop: return d;
    case Bxor: return s!=d; case Bor: return s||d;
    case Bnor: return !(s||d); case Bequiv: return s==d;
    case Binv: return !d; case Bimpby: return s||!d;
    case Brev: return !s; case Bimp: return !s||d;
    case Bnand: return !(s&&d); case Bset: return 1;
    }
    abort();
}
/* Cross cell boundaries and compare mutations with independent bit oracles. */
int main(void) {
    unsigned char model[288] = {0}, left[288], right[288];
    Bits b, a, copy;
    int size, i, lo, hi, op, trial, last;
    for(size = 0; size <= 257; size++) {
        b = bnew(size, 1);
        bitcheck(b.n == (size+31)/32); checks++;
        memset(model,0,sizeof(model)); verify(b,model,b.n*32);
        for(i = 0; i < size; i++) {
            bset(b,i); model[i] = 1; verify(b,model,b.n*32);
            bclr(b,i); model[i] = 0; verify(b,model,b.n*32);
        }
        bclrs(b,0,-1); verify(b,model,b.n*32);
        bfree(b);
    }
    for(lo = 0; lo < 96; lo++) for(hi = 0; hi < 96; hi++) {
        b = bnew(96,1); memset(model,0,sizeof(model));
        bsets(b,lo,hi);
        for(i = lo; i <= hi; i++) model[i] = 1;
        verify(b,model,96);
        boper(Bset,b,b); memset(model,1,96);
        bclrs(b,lo,hi);
        for(i = lo; i <= hi; i++) model[i] = 0;
        verify(b,model,96);
        bclrs(b,lo,-1);
        for(i = lo; i < 96; i++) model[i] = 0;
        verify(b,model,96); bfree(b);
    }
    for(trial = 0; trial < 100; trial++) for(op = Bclr; op <= Bset; op++) {
        a = bnew(9,0); b = bnew(9,0); copy = bnew(9,0);
        for(i = 0; i < 9; i++) { a.b[i]=randomword(); b.b[i]=randomword(); }
        for(i = 0; i < 288; i++) {
            left[i] = (a.b[i/32] >> (i%32)) & 1;
            right[i] = (b.b[i/32] >> (i%32)) & 1;
            model[i] = truth(op,left[i],right[i]);
        }
        boper(Bstore,b,copy); bitcheck(beq(b,copy));
        boper(op,a,b); verify(b,model,288);
        boper(op,a,a);
        for(i = 0; i < 288; i++) model[i]=truth(op,left[i],left[i]);
        verify(a,model,288);
        last = topbit(copy);
        if(last >= 0) { boper(Bstore,copy,b); bclr(copy,last); bitcheck(!beq(copy,b)); }
        bfree(a); bfree(b); bfree(copy);
    }
    printf("PASS optimizer bits: %lu membership/range/truth-table/query checks, ulong%zu\n",checks,sizeof(ulong)*8);
    return 0;
}
'''
unit = unit.replace('MACROS', macros).replace('ENUMS', enums).replace('STRUCT', struct).replace('BODY', body)
with tempfile.TemporaryDirectory(prefix='optimizer-bits-') as directory:
    td = Path(directory)
    src = td / 'bits.c'
    src.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for name, flags in [('long64',[]),('long32',['-DLONG32'])]:
        binary = td / name
        subprocess.run(cmd+flags+[str(src),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
