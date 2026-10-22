#!/usr/bin/env python3
"""Production base64 encoding/decoding with independent bytes and buffer bounds."""
import base64
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'lib9/u64.c').read_text().replace('#include <lib9.h>', '')
header = '''
#include <stdint.h>
typedef unsigned char uchar;
typedef uint32_t ulong;
'''
rng = random.Random(0x626173653634)

with tempfile.TemporaryDirectory(prefix='base64-') as directory:
    td = Path(directory)
    (td / 'codec.c').write_text(header + source)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    binary = td / 'codec.so'
    subprocess.run(cmd + ['-shared', '-fPIC', str(td / 'codec.c'),
                          '-o', str(binary)], check=True)
    lib = C.CDLL(str(binary))
    for name in ('enc64', 'dec64'):
        getattr(lib, name).argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int]
    checks = 0
    vectors = [bytes([byte]) for byte in range(256)]
    vectors += [rng.randbytes(length) for length in range(129)]
    for raw in vectors:
        expected = base64.b64encode(raw)
        for capacity in range(len(expected)+2):
            output = C.create_string_buffer(b'\xa5' * (len(expected)+8))
            before = output.raw
            n = lib.enc64(output, capacity, raw, len(raw))
            if capacity == 0:
                assert n == 0 and output.raw == before
            else:
                wanted = expected[:max(0, (capacity-1)//4)*4]
                assert n == len(wanted) and output.raw[:n+1] == wanted + b'\0'
                assert output.raw[n+1:] == before[n+1:]
            checks += 1
        for text in (expected, b' \n'.join(bytes([byte]) for byte in expected)):
            for capacity in range(len(raw)+2):
                output = C.create_string_buffer(b'\xa5' * (len(raw)+8))
                before = output.raw
                n = lib.dec64(output, capacity, text, len(text))
                assert 0 <= n <= capacity
                assert output.raw[:n] == raw[:n]
                assert output.raw[n:] == before[n:]
                if capacity >= len(raw):
                    assert n == len(raw)
                checks += 1
    output = C.create_string_buffer(b'\xa5' * 16)
    for count, capacity in [(-1, 16), (-2**31, 16), (2**31-1, 0), (0, 0)]:
        assert lib.enc64(output, capacity, None, count) == 0
        assert lib.dec64(output, capacity, None, count) == 0
        assert output.raw[:16] == b'\xa5' * 16
    assert lib.enc64(None, 0, None, 0) == 0
    assert lib.dec64(None, 0, None, 0) == 0
    print(f'PASS base64: {checks} oracle/capacity/guard cases, ignored whitespace and zero limits')
