#!/usr/bin/env python3
"""Compare native scaling and VM exponent bridges with independent host oracles."""
import ctypes as C
import math
import os
from pathlib import Path
import random
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
vm = Path(os.environ.get('MATH_EXPONENT_SOURCE', root / 'libinterp/math.c')).read_text()
native = Path(os.environ.get('SCALBN_SOURCE', root / 'libmath/fdlibm/s_scalbn.c')).read_text()
native = native.replace('#include "fdlibm.h"', '').replace('scalbn(', 'tested_scalbn(').replace('scalbn (', 'tested_scalbn (')
ipow = (root / 'libmath/pow10.c').read_text()
ipow = re.sub(r'^#include.*$', '', ipow, flags=re.M)
parts = []
for name in ('mathexponent', 'Math_pow10', 'Math_scalbn'):
    match = re.search(r'(?:static int|void)\n'+name+r'\([^)]*\)\n\{', vm)
    if match:
        start = match.start()
        end = vm.index('\n}', match.end())+2
        parts.append(vm[start:end].replace('= scalbn(', '= tested_scalbn('))
header = '''#include <stdint.h>
#include <string.h>
typedef double REAL;
#include <math.h>
typedef WORD_TYPE WORD;
typedef struct { double *ret; WORD p; } F_Math_pow10;
typedef struct { double *ret; double x; WORD n; } F_Math_scalbn;
/* Match fdlibm's little-endian word access; build without strict aliasing. */
#define __HI(x) (((int32_t*)&(x))[1])
#define __LO(x) (((int32_t*)&(x))[0])
'''
bridge = '''
/* Enter the production builtin through bounded scalar fixture frames. */
double call_pow10(WORD n) {
    double out; F_Math_pow10 f = {&out, n}; Math_pow10(&f); return out;
}
double call_scalbn(double x, WORD n) {
    double out; F_Math_scalbn f = {&out, x, n}; Math_scalbn(&f); return out;
}
'''
rng = random.Random(0x5ca1b)
with tempfile.TemporaryDirectory(prefix='math-exponents-') as directory:
    td = Path(directory)
    for width in (32, 64):
        target = td / ('math'+str(width)+'.c')
        target.write_text(header.replace('WORD_TYPE', 'int'+str(width)+'_t') + native + ipow + '\n'.join(parts) + bridge)
        binary = td / ('math'+str(width)+'.so')
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd + ['-fno-strict-aliasing', '-shared', '-fPIC', str(target), '-lm', '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        word = C.c_int32 if width == 32 else C.c_int64
        lib.call_pow10.argtypes = [word]
        lib.call_pow10.restype = C.c_double
        lib.call_scalbn.argtypes = [C.c_double, word]
        lib.call_scalbn.restype = C.c_double
        lib.tested_scalbn.argtypes = [C.c_double, C.c_int]
        lib.tested_scalbn.restype = C.c_double
        exponents = [-2**31, -50001, -50000, -1075, -1074, -1024, -1, 0, 1, 1024, 50000, 50001, 2**31-1]
        if width == 64:
            exponents += [-2**63, -2**32-1, -2**32, 2**32, 2**32+1, 2**63-1]
        values = [0., -0., 1., -1., math.inf, -math.inf, math.nan, 5e-324, -5e-324, 1e308, -1e308]
        values += [rng.uniform(-1, 1)*2.**rng.randrange(-1022, 1024) for _ in range(200)]
        checks = 0
        for n in exponents + list(range(-1100, 1101, 13)):
            wanted = math.inf if n > 308 else 0. if n < -324 else 10.**n
            got = lib.call_pow10(n)
            assert got == wanted, ('pow10', width, n, got, wanted)
            checks += 1
            for x in values:
                bounded = max(-100000, min(100000, n))
                try:
                    wanted = math.ldexp(x, bounded)
                except OverflowError:
                    wanted = math.copysign(math.inf, x)
                functions = [lib.call_scalbn]
                if -2**31 <= n < 2**31:
                    functions.append(lib.tested_scalbn)
                for fn in functions:
                    got = fn(x, n)
                    if math.isnan(wanted):
                        assert math.isnan(got)
                    else:
                        assert got == wanted and math.copysign(1, got) == math.copysign(1, wanted), (width, x, n, got, wanted)
                    checks += 1
        print(f'WORD{width}: {checks} native/VM exponent, special-value and scaling cases passed')
