#!/usr/bin/env python3
"""Exercise production prime routines with deterministic, bounded test randomness."""
import ctypes as C
import os
from pathlib import Path
import random
import re
import shlex
import signal
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
files = ['mpaux', 'mpfmt', 'strtomp', 'mptobe', 'betomp', 'mpadd', 'mpsub',
         'mpcmp', 'mpmul', 'mprand', 'mpexp', 'mpmod', 'mpleft', 'mpright',
         'mpvecadd', 'mpvecsub', 'mpvecdigmuladd', 'mpveccmp', 'mpdigdiv',
         'mpdiv', 'mptoui', 'mptoi']


# Trial division supplies an exact oracle for all small test candidates.
def prime(value):
    if value < 2:
        return False
    return all(value % divisor for divisor in range(2, int(value**0.5)+1))


with tempfile.TemporaryDirectory(prefix='primes-') as directory:
    td = Path(directory)
    header = '#include "' + str(root / 'libmp/tests/text-support.h') + '"\n'
    header += '#define nelem(a) (sizeof(a)/sizeof((a)[0]))\n'
    header += 'void genrandom(uchar*, int);\nvoid prng(uchar*, int);\n'
    header += 'int smallprimetest(mpint*);\nint probably_prime(mpint*, int);\n'
    header += 'void genprime(mpint*, int, int);\n'
    (td / 'support.h').write_text(header)
    units = []
    paths = [root / ('libmp/port/' + name + '.c') for name in files]
    paths += [root / ('libsec/port/' + name + '.c') for name in
              ['probably_prime', 'smallprimetest', 'genprime', 'gensafeprime']]
    paths += [root / 'lib9/u32.c', root / 'lib9/u64.c', root / 'libmp/tests/text-support.c']
    for path in paths:
        text = re.sub(r'^#include[^\n]*\n', '', path.read_text(), flags=re.M)
        target = td / path.name
        target.write_text('#include "support.h"\n' + text)
        units.append(str(target))
    (td / 'random.c').write_text('''#include "support.h"
static void (*source)(uchar*, int, int);
/* Select a deterministic source for tests, never for production use. */
void setsource(void (*fill)(uchar*, int, int)) { source = fill; }
/* Keep candidate and witness streams independently controllable. */
void genrandom(uchar *p, int n) { source(p, n, 0); }
/* Supply witnesses independently of adversarial candidate bytes. */
void prng(uchar *p, int n) { source(p, n, 1); }
''')
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    binary = td / 'primes.so'
    subprocess.run(cmd + ['-shared', '-fPIC', '-I'+str(root / 'include')] + units +
                   [str(td / 'random.c'), '-o', str(binary)], check=True)
    lib = C.CDLL(str(binary))
    callback = C.CFUNCTYPE(None, C.POINTER(C.c_ubyte), C.c_int, C.c_int)
    lib.setsource.argtypes = [callback]
    lib.strtomp.argtypes = [C.c_char_p, C.c_void_p, C.c_int, C.c_void_p]
    lib.strtomp.restype = C.c_void_p
    lib.mpnew.argtypes = [C.c_int]
    lib.mpnew.restype = C.c_void_p
    lib.mpfree.argtypes = [C.c_void_p]
    lib.probably_prime.argtypes = [C.c_void_p, C.c_int]
    lib.genprime.argtypes = [C.c_void_p, C.c_int, C.c_int]
    lib.gensafeprime.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int]
    lib.mptoa.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int]
    lib.mptoa.restype = C.c_void_p
    lib.testfree.argtypes = [C.c_void_p]
    lib.mpsetminbits(1)
    rng = random.Random(0x7072696d65)
    mode, calls = 'random', [0, 0]
    sequence = iter(())

    @callback
    def fill(data, count, witness):
        calls[witness] += 1
        # Fail old infinite witness loops promptly instead of hanging the suite.
        if sum(calls) > 100000:
            os._exit(2)
        if witness or mode == 'random':
            for i in range(count):
                data[i] = rng.randrange(256)
        elif mode == 'ones':
            C.memset(data, 255, count)
        else:
            C.memset(data, 0, count)
            if mode == 'sequence':
                data[count-1] = next(sequence)

    lib.setsource(fill)
    # A production regression must not leave an unbounded native loop running.
    signal.alarm(45)

    # Decode through production formatting and release its temporary buffer.
    def value(number):
        encoded = lib.mptoa(number, 16, None, 0)
        assert encoded
        result = int(C.string_at(encoded), 16)
        lib.testfree(encoded)
        return result

    checks = 0
    for candidate in list(range(-32, 2049)) + [2**31-1, 2**61-1, 2**127-1,
                                             (2**31-1)*(2**61-1)]:
        number = lib.strtomp(str(candidate).encode(), None, 10, None)
        calls[:] = [0, 0]
        expected = prime(candidate) if candidate <= 2048 else candidate in (2**31-1, 2**61-1, 2**127-1)
        assert bool(lib.probably_prime(number, 4)) == expected, candidate
        assert value(number) == candidate
        lib.mpfree(number)
        assert lib.liveblocks() == 0
        checks += 1
    for mode in ('zeros', 'ones', 'random'):
        for bits in range(2, 17):
            number = lib.strtomp(b'-18446744073709551615', None, 10, None)
            calls[:] = [0, 0]
            lib.genprime(number, bits, 4)
            result = value(number)
            assert result.bit_length() == bits and prime(result), (mode, bits, result)
            lib.mpfree(number)
            assert lib.liveblocks() == 0
            checks += 1
    mode = 'sequence'
    sequence = iter([0, 0, 1, 2, 3])
    calls[:] = [0, 0]
    p, alpha = lib.mpnew(0), lib.mpnew(0)
    lib.gensafeprime(p, alpha, 3, 4)
    assert value(p) == 7 and value(alpha) == 3 and calls[0] == 5
    lib.mpfree(p)
    lib.mpfree(alpha)
    assert lib.liveblocks() == 0
    checks += 1
    mode = 'random'
    for bits in range(3, 13):
        calls[:] = [0, 0]
        p, alpha = lib.mpnew(0), lib.mpnew(0)
        lib.gensafeprime(p, alpha, bits, 4)
        modulus, generator = value(p), value(alpha)
        q = (modulus-1)//2
        assert modulus.bit_length() == bits and prime(modulus) and prime(q)
        assert 1 < generator < modulus-1
        assert pow(generator, 2, modulus) != 1 and pow(generator, q, modulus) != 1
        lib.mpfree(p)
        lib.mpfree(alpha)
        assert lib.liveblocks() == 0
        checks += 1
    signal.alarm(0)
    print(f'PASS primes: {checks} primality/width/safe-group/ownership cases')
