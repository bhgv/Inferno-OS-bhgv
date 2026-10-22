#!/usr/bin/env python3
"""Check native DSA seed bounds and each candidate against a Python SHA1 oracle."""
import ctypes as C
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
sourcepath = Path(os.environ.get('DSA_SOURCE', root / 'libsec/port/dsaprimes.c'))
files = ['mpaux', 'mpfmt', 'strtomp', 'mptobe', 'betomp', 'letomp', 'mpadd',
         'mpsub', 'mpcmp', 'mpmul', 'mpleft', 'mpright', 'mpvecadd', 'mpvecsub',
         'mpvecdigmuladd', 'mpveccmp', 'mpdigdiv', 'mpdiv', 'mpmod', 'mptoui', 'mptoi']
with tempfile.TemporaryDirectory(prefix='dsa-primes-') as directory:
    td = Path(directory)
    # Compile the seed helper independently with a real LP64 unsigned long.
    source = sourcepath.read_text()
    start = source.index('static void\nHrand(')
    stop = source.index('\n}', start)+2
    helper = """#include <assert.h>
#include <string.h>
typedef unsigned char uchar;
typedef unsigned long ulong;
enum {SHA1dlen = 20};
/* Keep baseline fastrand references linkable for before/after reproduction. */
static unsigned int fastrand(void) { return 0x5a5a5a5a; }
/* Require the exact byte count and tolerate deliberately unaligned storage. */
static void genrandom(uchar *p, int n) { assert(n == 20); memset(p,0x5a,n); }
""" + source[start:stop] + """
/* Check all alignments and guards around the exact 20-byte write. */
int main(void) {
    uchar bytes[64]; int offset, i;
    for(offset = 0; offset < 8; offset++) {
        memset(bytes,0xa5,sizeof(bytes)); Hrand(bytes+offset);
        for(i = 0; i < 64; i++)
            assert(bytes[i] == (i >= offset && i < offset+20 ? 0x5a : 0xa5));
    }
    return 0;
}
"""
    native = td / 'seed.c'
    native.write_text(helper)
    compiler = shlex.split(os.environ.get('CC','cc'))
    flags = shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(compiler+flags+[str(native),'-o',str(td/'seed')],check=True)
    subprocess.run([str(td/'seed')],check=True)
    header = '#include "' + str(root / 'libmp/tests/text-support.h') + '"\n'
    header += '''#define SHA1dlen 20
void genrandom(uchar*, int);
int probably_prime(mpint*, int);
void *sha1(uchar*, ulong, uchar*, void*);
'''
    (td / 'support.h').write_text(header)
    paths = [root / ('libmp/port/' + name + '.c') for name in files]
    paths += [sourcepath, root / 'lib9/u32.c',
              root / 'lib9/u64.c', root / 'libmp/tests/text-support.c']
    units = []
    for path in paths:
        source = re.sub(r'^#include[^\n]*\n', '', path.read_text(), flags=re.M)
        target = td / path.name
        target.write_text('#include "support.h"\n' + source)
        units.append(str(target))
    (td / 'callbacks.c').write_text('''#include "support.h"
static void (*hash)(uchar*, ulong, uchar*);
static int (*classify)(mpint*, int);
static int randomcalls;
/* Install independent hash/oracle callbacks for this arithmetic fixture. */
void setup(void (*h)(uchar*, ulong, uchar*), int (*p)(mpint*, int))
{
    hash = h;
    classify = p;
    randomcalls = 0;
}
/* Fill exactly the requested bytes with a seed that exercises carry propagation. */
void genrandom(uchar *p, int n)
{
    int i;
    if(n != 20) abort();
    for(i = 0; i < n; i++) p[i] = i < 2 ? 255 : i;
    randomcalls++;
}
/* Adapt Python hashlib without replacing the candidate construction under test. */
void *sha1(uchar *p, ulong n, uchar *digest, void *state)
{
    if(state != nil || n != 20) abort();
    hash(p, n, digest);
    return nil;
}
/* Force retries independently of primality so every candidate can be inspected. */
int probably_prime(mpint *p, int accuracy) { return classify(p, accuracy); }
/* Expose the exact seed-buffer request count. */
int calls(void) { return randomcalls; }
''')
    binary = td / 'dsa.so'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd + ['-shared', '-fPIC', '-I'+str(root / 'include')] + units +
                   [str(td / 'callbacks.c'), '-o', str(binary)], check=True)
    lib = C.CDLL(str(binary))
    byteptr = C.POINTER(C.c_ubyte)
    hashcb = C.CFUNCTYPE(None, byteptr, C.c_uint32, byteptr)
    primecb = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int)
    lib.setup.argtypes = [hashcb, primecb]
    lib.mpnew.argtypes = [C.c_int]
    lib.mpnew.restype = C.c_void_p
    lib.mpfree.argtypes = [C.c_void_p]
    lib.DSAprimes.argtypes = [C.c_void_p, C.c_void_p, byteptr]
    lib.mptoa.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int]
    lib.mptoa.restype = C.c_void_p
    lib.testfree.argtypes = [C.c_void_p]
    lib.mpsetminbits(1)

    # Use production formatting to inspect every candidate without layout guesses.
    def value(number):
        text = lib.mptoa(number, 16, None, 0)
        result = int(C.string_at(text), 16)
        lib.testfree(text)
        return result

    @hashcb
    def digest(data, count, output):
        C.memmove(output, hashlib.sha1(C.string_at(data, count)).digest(), 20)

    seen = []
    @primecb
    def classify(number, accuracy):
        seen.append((value(number), accuracy))
        if len(seen) > 8:
            os._exit(2)
        # Accept the seed-derived q; reject the first two p candidates.
        return int(len(seen) == 1 or len(seen) == 4)

    seed = bytes([255, 255] + list(range(2, 20)))
    modulus = 1 << 160
    initial = int.from_bytes(seed, 'little')

    def h(offset):
        data = ((initial + offset) % modulus).to_bytes(20, 'little')
        return int.from_bytes(hashlib.sha1(data).digest(), 'little')

    qexpected = (h(0) ^ h(1)) | 1 | (1 << 159)
    expected = [(qexpected, 18)]
    for attempt in range(3):
        offset = 2 + 7*attempt
        w = sum(h(offset+k) << (160*k) for k in range(6))
        w += (h(offset+6) % (1 << 63)) << 960
        x = w + (1 << 1023)
        expected.append((x - (x % (2*qexpected) - 1), 5))
    checks = 512
    for withseed in (True, False):
        seen.clear()
        lib.setup(digest, classify)
        q, p = lib.mpnew(0), lib.mpnew(0)
        storage = (C.c_ubyte * 22)(*([0xa5]*22))
        output = C.cast(C.byref(storage, 1), byteptr) if withseed else None
        lib.DSAprimes(q, p, output)
        assert seen == expected, (seen, expected)
        assert value(q) == qexpected and value(p) == expected[-1][0]
        assert lib.calls() == 1
        assert storage[0] == storage[21] == 0xa5
        if withseed:
            assert bytes(storage)[1:21] == seed
        lib.mpfree(q)
        lib.mpfree(p)
        assert lib.liveblocks() == 0
        checks += len(seen) + 5
    print(f'PASS DSA candidates: {checks} hash-sum/seed/carry/ownership checks')
