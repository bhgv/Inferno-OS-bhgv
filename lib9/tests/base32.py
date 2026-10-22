#!/usr/bin/env python3
"""Production base32 codec against Python's RFC bit grouping and bounded buffers."""
import base64
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'lib9/u32.c').read_text().replace('#include <lib9.h>', '')
header = '''
#include <stdint.h>
#include <string.h>
typedef unsigned char uchar;
typedef unsigned int uint;
typedef uint64_t uvlong;
#define nil NULL
'''
alphabet = b'23456789abcdefghijkmnpqrstuvwxyz'
translation = bytes.maketrans(b'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567', alphabet)
rng = random.Random(0x626173653332)

with tempfile.TemporaryDirectory(prefix='base32-') as directory:
    td = Path(directory)
    (td / 'codec.c').write_text(header + source)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    binary = td / 'codec.so'
    subprocess.run(cmd + ['-shared', '-fPIC', str(td / 'codec.c'),
                          '-o', str(binary)], check=True)
    lib = C.CDLL(str(binary))
    for name in ('enc32', 'dec32'):
        getattr(lib, name).argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int]
    checks = 0
    vectors = [bytes([value]) for value in range(256)]
    vectors += [rng.randbytes(length) for length in range(129)]
    for raw in vectors:
        expected = base64.b32encode(raw).rstrip(b'=').translate(translation)
        for capacity in range(len(expected)+2):
            output = C.create_string_buffer(b'\xa5' * (len(expected)+8))
            before = output.raw
            n = lib.enc32(output, capacity, raw, len(raw))
            if capacity <= len(expected):
                assert n == -1 and output.raw == before
            else:
                assert n == len(expected)
                assert output.raw[:n+1] == expected + b'\0', (raw, output.raw, expected)
                assert output.raw[n+1:] == before[n+1:]
            checks += 1
        for capacity in range(len(raw)+2):
            output = C.create_string_buffer(b'\xa5' * (len(raw)+8))
            before = output.raw
            n = lib.dec32(output, capacity, expected, len(expected))
            if capacity < len(raw):
                assert n == -1 and output.raw == before
            else:
                assert n == len(raw) and output.raw[:n] == raw
                assert output.raw[n:] == before[n:]
            checks += 1
    for length in range(1, 33):
        text = b'2' * length
        output = C.create_string_buffer(b'\xa5' * 40)
        before = output.raw
        if length % 8 in (1, 3, 6):
            assert lib.dec32(output, 40, text, length) == -1
            assert output.raw == before
            checks += 1
    for bad in (b'0', b'1', b'l', b'o', b'=', b'\0', b'\xff'):
        for position in range(8):
            text = b'2' * position + bad + b'2' * (7-position)
            output = C.create_string_buffer(b'\xa5' * 16)
            assert lib.dec32(output, 16, text, len(text)) == -1
            assert output.raw[:16] == b'\xa5' * 16
            checks += 1
    output = C.create_string_buffer(b'\xa5' * 16)
    for count, capacity in [(-1, 16), (2**31-1, 0), (2**31-1, 1)]:
        assert lib.enc32(output, capacity, None, count) == -1
        assert lib.dec32(output, capacity, None, count) == -1
    assert lib.dec32(None, 0, None, 0) == 0
    assert output.raw[:16] == b'\xa5' * 16
    print(f'PASS base32: {checks} reference/capacity/canary cases, all tails and invalid groups')
