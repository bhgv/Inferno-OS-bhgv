#!/usr/bin/env python3
"""Compare production hex codecs with independent byte and delimiter oracles."""
import ctypes as C
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'lib9/u16.c').read_text().replace('#include <lib9.h>', '')
header = 'typedef unsigned char uchar; typedef unsigned int uint;\n'
rng = random.Random(0x686578)
hexchars = b'0123456789abcdefABCDEF'

with tempfile.TemporaryDirectory(prefix='hex-codec-') as directory:
    td = Path(directory)
    (td / 'codec.c').write_text(header + source)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    binary = td / 'codec.so'
    subprocess.run(cmd + ['-shared', '-fPIC', str(td / 'codec.c'), '-o', str(binary)], check=True)
    lib = C.CDLL(str(binary))
    for name in ('enc16', 'dec16'):
        getattr(lib, name).argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int]
    checks = 0
    vectors = [bytes([byte]) for byte in range(256)]
    vectors += [rng.randbytes(length) for length in range(129)]
    for raw in vectors:
        expected = raw.hex().upper().encode()
        for capacity in range(len(expected)+2):
            output = C.create_string_buffer(b'\xa5'*(len(expected)+8))
            before = output.raw
            n = lib.enc16(output, capacity, raw, len(raw))
            if capacity == 0:
                assert n == 0 and output.raw == before
            else:
                wanted = expected[:((capacity-1)//2)*2]
                assert n == len(wanted) and output.raw[:n+1] == wanted+b'\0'
                assert output.raw[n+1:] == before[n+1:]
            checks += 1
        for text in (expected, expected.lower(), b'gZ :\n'.join(bytes([c]) for c in expected)):
            for capacity in range(len(raw)+2):
                output = C.create_string_buffer(b'\xa5'*(len(raw)+8))
                before = output.raw
                n = lib.dec16(output, capacity, text, len(text))
                assert n == min(len(raw), capacity)
                assert output.raw[:n] == raw[:n] and output.raw[n:] == before[n:]
                checks += 1
    texts = [bytes(range(256)), b'gGzZ', b'1g2', b'abc', b'F', b'00xyzFF', b'!']
    texts += [rng.randbytes(length) for length in range(128)]
    for text in texts:
        digits = bytes(c for c in text if c in hexchars)
        expected = bytes.fromhex(digits[:len(digits)//2*2].decode())
        output = C.create_string_buffer(b'\xa5'*(len(text)+8))
        before = output.raw
        n = lib.dec16(output, len(text), text, len(text))
        assert output.raw[:n] == expected and n == len(expected)
        assert output.raw[n:] == before[n:]
        checks += 1
    for capacity in (0, -1, -2**31):
        for count in (-2**31, -1, 0, 2**31-1):
            output = C.create_string_buffer(b'\xa5'*16)
            assert lib.enc16(output, capacity, None, count) == 0
            assert lib.dec16(output, capacity, None, count) == 0
            assert output.raw[:16] == b'\xa5'*16
            checks += 1
    for count in (-2**31, -1, 0, 2**31-1):
        output = C.create_string_buffer(b'\xa5'*16)
        assert lib.enc16(output, 1, None, count) == 0
        assert output.raw[:16] == b'\0'+b'\xa5'*15
        checks += 1
    for count in (-2**31, -1, 0):
        output = C.create_string_buffer(b'\xa5'*16)
        assert lib.dec16(output, 16, None, count) == 0
        assert output.raw[:16] == b'\xa5'*16
        checks += 1
    assert lib.enc16(None, 0, None, 0) == 0
    assert lib.dec16(None, 0, None, 0) == 0
    print(f'PASS hex: {checks} oracle/capacity/guard/delimiter/extreme-count cases')
