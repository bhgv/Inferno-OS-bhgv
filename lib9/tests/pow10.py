#!/usr/bin/env python3
"""Compare both production decimal-power routines with exact decimal rounding."""
import ctypes as C
from decimal import Decimal
import math
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pow10-') as directory:
    td = Path(directory)
    for library in ('lib9', 'libkern'):
        path = Path(os.environ.get('POW10_SOURCE', root / library / 'pow10.c'))
        source = path.read_text().replace('#include\t"lib9.h"', '')
        source = source.replace('pow10(', 'testedpow10(')
        target = td / (library+'.c')
        target.write_text(source)
        binary = td / (library+'.so')
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd + ['-shared', '-fPIC', str(target), '-o', str(binary)], check=True)
        lib = C.CDLL(str(binary))
        fn = lib.testedpow10
        fn.argtypes = [C.c_int]
        fn.restype = C.c_double
        checks = 0
        for n in range(-1100, 1101):
            wanted = float(Decimal('1e'+str(n)))
            actual = fn(n)
            if wanted == 0 or math.isinf(wanted):
                assert actual == wanted, (library, n, actual, wanted)
            else:
                assert abs(actual-wanted) <= 4*math.ulp(wanted), (library, n, actual, wanted)
            checks += 1
        for n in (-2**31, -2**31+1, -10**9, 10**9, 2**31-1):
            assert fn(n) == (0 if n < 0 else math.inf)
            checks += 1
        print(f'{library} pow10: {checks} decimal-oracle and extreme-exponent cases passed')
