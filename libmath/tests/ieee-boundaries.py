#!/usr/bin/env python3
"""Exercise production fdlibm bit operations against independent host oracles."""
import ctypes as C
import math
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
names = {'ilogb': 's_ilogb.c', 'fmod': 'e_fmod.c', 'remainder': 'e_remainder.c',
         'ceil': 's_ceil.c', 'floor': 's_floor.c', 'rint': 's_rint.c',
         'nextafter': 's_nextafter.c', 'atan2': 'e_atan2.c', 'hypot': 'e_hypot.c'}


def bits(x):
    """Expose the complete binary64 representation, including signed zero."""
    return struct.unpack('=Q', struct.pack('=d', x))[0]


def real(n):
    """Build arbitrary IEEE values without decimal conversion."""
    return struct.unpack('=d', struct.pack('=Q', n))[0]


def equal(got, wanted, context):
    """Require exact bits except for implementation-specific NaN payloads."""
    if math.isnan(wanted):
        assert math.isnan(got), context
    else:
        assert bits(got) == bits(wanted), (context, got, wanted)


def worker(directory, selected):
    """Run native calls in a child so sanitizer exits leave no temporary artifacts."""
    rng = random.Random(0xfdb17)
    values = [0., -0., math.inf, -math.inf, math.nan]
    values += [real((sign << 63) | (1 << bit) | tail)
               for sign in range(2) for bit in range(52) for tail in (0, 1)]
    values += [real((high << 32) | low) for high in (0, 1, 0xfffff, 0x100000, 0x3ff00000, 0x7fefffff, 0x80000000, 0xbff00000)
               for low in (0, 1, 0x7fffffff, 0x80000000, 0xffffffff)]
    values += [real(rng.getrandbits(64)) for _ in range(4000)]
    values += [sign * math.nextafter(2.**power+0.5, direction)
               for sign in (-1, 1) for power in range(19)
               for direction in (-math.inf, math.inf)]
    values += [sign * (2.**power+fraction) for sign in (-1, 1)
               for power in range(53) for fraction in (.25, .5, .75)]
    lib = C.CDLL(str(Path(directory) / 'ieee.so'))
    checks = 0
    for name in selected:
        fn = getattr(lib, 'tested_'+name)
        binary = name in ('fmod', 'remainder', 'nextafter', 'atan2', 'hypot')
        fn.argtypes = [C.c_double] * (2 if binary else 1)
        fn.restype = C.c_int if name == 'ilogb' else C.c_double
        if name == 'ilogb':
            for x in values:
                wanted = -2147483647 if x == 0 else 2147483647 if not math.isfinite(x) else math.frexp(x)[1]-1
                assert fn(x) == wanted, (name, x, fn(x), wanted)
                checks += 1
        elif name in ('ceil', 'floor', 'rint'):
            oracle = {'ceil': math.ceil, 'floor': math.floor, 'rint': round}[name]
            for x in values:
                wanted = float(oracle(x)) if math.isfinite(x) else x
                if wanted == 0:
                    wanted = math.copysign(0., x)
                equal(fn(x), wanted, (name, x))
                checks += 1
        else:
            oracle = getattr(math, name)
            pairs = [(x, y) for x in values[:253] for y in values[:253:11]]
            pairs += [(x, values[rng.randrange(len(values))]) for x in values]
            pairs += [(x, x) for x in values]
            pairs += [(x, math.nextafter(x, math.inf)) for x in values if math.isfinite(x)]
            pairs += [(0., -0.), (-0., 0.), (1., -1.), (-1., -1.)]
            for x, y in pairs:
                try:
                    wanted = oracle(x, y)
                except ValueError:
                    wanted = math.nan
                got = fn(x, y)
                if name in ('atan2', 'hypot') and math.isfinite(wanted) and wanted != 0:
                    assert abs(got-wanted) <= 2*math.ulp(wanted), (name, x, y, got, wanted)
                else:
                    equal(got, wanted, (name, x, y))
                checks += 1
        if name == 'rint':
            lib.set_rounding.argtypes = [C.c_int]
            lib.set_rounding.restype = C.c_int
            try:
                for mode, oracle in enumerate((round, math.floor, math.ceil, math.trunc)):
                    expected = [(x, math.copysign(0., x) if oracle(x) == 0 else float(oracle(x)))
                                for x in values if math.isfinite(x)]
                    assert lib.set_rounding(mode) == 0
                    for x, wanted in expected:
                        equal(fn(x), wanted, ('rint mode', mode, x))
                        checks += 1
                    assert lib.set_rounding(0) == 0
            finally:
                lib.set_rounding(0)
        print(name+': passed', flush=True)
    print(f'fdlibm IEEE boundaries: {checks} oracle cases passed', flush=True)


def main():
    """Compile actual fdlibm headers and separate translation units with renamed exports."""
    if len(sys.argv) > 1 and sys.argv[1] == '--worker':
        worker(sys.argv[2], sys.argv[3:])
        return
    selected = os.environ.get('FDLIBM_CASE', 'ilogb fmod remainder hypot').split()
    with tempfile.TemporaryDirectory(prefix='ieee-boundaries-') as directory:
        td = Path(directory)
        objects = []
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-fno-strict-aliasing', '-fno-builtin', '-frounding-math', '-D__LITTLE_ENDIAN']
        cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'libmath/fdlibm')]
        renames = {name: 'tested_'+name for name in ('ilogb', 'ceil', 'floor', 'rint', 'nextafter')}
        renames.update({'__ieee754_'+name: 'tested_'+name for name in ('fmod', 'remainder', 'atan2', 'hypot')})
        cmd += ['-D'+name+'='+new for name, new in renames.items()]
        # remainder calls production fmod, so always build both objects.
        build = list(dict.fromkeys(selected + (['fmod'] if 'remainder' in selected else [])))
        for name in build:
            source = Path(os.environ.get('FDLIBM_'+name.upper()+'_SOURCE', root / 'libmath/fdlibm' / names[name]))
            obj = td / (name+'.o')
            subprocess.run(cmd+['-fPIC', '-c', str(source), '-o', str(obj)], check=True)
            objects.append(str(obj))
        if 'rint' in selected:
            helper = td / 'rounding-mode.c'
            helper.write_text('''#include <fenv.h>
/* Select standard host rounding modes without hardcoding their numeric values. */
int set_rounding(int n) {
    int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    return n < 0 || n > 3 ? -1 : fesetround(modes[n]);
}
''')
            obj = td / 'rounding-mode.o'
            subprocess.run(cmd+['-fPIC', '-c', str(helper), '-o', str(obj)], check=True)
            objects.append(str(obj))
        subprocess.run(cmd+['-shared', *objects, '-lm', '-o', str(td / 'ieee.so')], check=True)
        subprocess.run([sys.executable, __file__, '--worker', directory, *selected], check=True, timeout=45)


if __name__ == '__main__':
    main()
