#!/usr/bin/env python3
"""Round-trip production libmp text conversion against Python integer oracles."""
import ctypes as C
import os
from pathlib import Path
import random
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
files = ['mpaux', 'mpfmt', 'strtomp', 'mptobe', 'betomp', 'mpadd', 'mpsub',
         'mpcmp', 'mpmul', 'mpexp', 'mpmod', 'mpleft', 'mpright', 'mpvecadd', 'mpvecsub',
         'mpvecdigmuladd', 'mpveccmp', 'mpdigdiv', 'mpdiv', 'mptoui']
alphabet = '23456789abcdefghijkmnpqrstuvwxyz'


# Use a bit-string oracle independent of the production base32 encoder.
def base32(data):
    bits = ''.join(f'{byte:08b}' for byte in data)
    return ''.join(alphabet[int(bits[i:i+5].ljust(5, '0'), 2)]
                   for i in range(0, len(bits), 5))


with tempfile.TemporaryDirectory(prefix='mp-text-') as directory:
    td = Path(directory)
    units = []
    for path in [root / ('libmp/port/' + name + '.c') for name in files] + [
            root / 'lib9/u32.c', root / 'lib9/u64.c']:
        text = re.sub(r'^#include[^\n]*\n', '', path.read_text(), flags=re.M)
        target = td / path.name
        target.write_text(text)
        units.append(str(target))
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    cmd += ['-shared', '-fPIC', '-I' + str(root / 'include'),
            '-include', str(root / 'libmp/tests/text-support.h')]
    binary = td / 'text.so'
    subprocess.run(cmd + units + [str(root / 'libmp/tests/text-support.c'),
                                 '-o', str(binary)], check=True)
    lib = C.CDLL(str(binary))
    lib.strtomp.argtypes = [C.c_char_p, C.POINTER(C.c_void_p), C.c_int, C.c_void_p]
    lib.strtomp.restype = C.c_void_p
    lib.mptoa.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int]
    lib.mptoa.restype = C.c_void_p
    lib.betomp.argtypes = [C.c_void_p, C.c_uint, C.c_void_p]
    lib.betomp.restype = C.c_void_p
    lib.mpfree.argtypes = [C.c_void_p]
    lib.mpnew.argtypes = [C.c_int]
    lib.mpnew.restype = C.c_void_p
    lib.mpright.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
    lib.mpleft.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
    lib.mpexp.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.mplowbits0.argtypes = [C.c_void_p]
    lib.testfree.argtypes = [C.c_void_p]
    lib.mpsetminbits(1)
    rng = random.Random(0x6d7074657874)
    values = [0, 1, 15, 16, 255, 256, 2**31, 2**32-1, 2**63, 2**64-1]
    values += [rng.getrandbits(bits) for bits in range(1, 513, 7)]
    checks = 0
    for value in values:
        data = value.to_bytes(max(1, (value.bit_length()+7)//8), 'big')
        number = lib.betomp(data, len(data), None)
        assert number
        if value:
            assert lib.mplowbits0(number) == (value & -value).bit_length()-1
        shifted = lib.mpnew(0)
        for shift in (0, 1, 31, 32, 63, 64, value.bit_length()+64):
            lib.mpright(number, shift, shifted)
            result = lib.mptoa(shifted, 16, None, 0)
            assert result and int(C.string_at(result), 16) == value >> shift
            lib.testfree(result)
        lib.mpfree(shifted)
        for base in (10, 16, 32, 64):
            result = lib.mptoa(number, base, None, 0)
            assert result
            text = C.string_at(result)
            if base in (10, 16):
                assert int(text, base) == value
            elif base == 32:
                assert text == base32(data).encode(), (value, text, base32(data))
            parsed = lib.strtomp(text, None, base, None)
            assert parsed, (base, text)
            hexresult = lib.mptoa(parsed, 16, None, 0)
            assert int(C.string_at(hexresult), 16) == value
            lib.testfree(hexresult)
            lib.mpfree(parsed)
            lib.testfree(result)
            checks += 1
        lib.mpfree(number)
        assert lib.liveblocks() == 0
    # Compare modular powers and all output aliases with Python's integer oracle.
    power_checks = 0
    for base_value in (0, 1, 2, 17, 2**32+1, 2**65-1):
        for exponent_value in (0, 1, 2, 7, 31):
            for modulus_value in (1, 2, 19, 2**64-59):
                for alias in range(4):
                    pointers = [lib.strtomp(str(value).encode(), None, 10, None)
                                for value in (base_value, exponent_value, modulus_value)]
                    result = lib.mpnew(0) if alias == 3 else pointers[alias]
                    lib.mpexp(*pointers, result)
                    encoded = lib.mptoa(result, 16, None, 0)
                    assert int(C.string_at(encoded), 16) == pow(base_value, exponent_value, modulus_value)
                    lib.testfree(encoded)
                    if alias == 3:
                        lib.mpfree(result)
                    for pointer in pointers:
                        lib.mpfree(pointer)
                    assert lib.liveblocks() == 0
                    power_checks += 1
    # Minimum negative left shift is a large right shift, with no signed negation.
    number = lib.strtomp(b'123456789', None, 10, None)
    shifted = lib.mpnew(0)
    lib.mpleft(number, -2**31, shifted)
    encoded = lib.mptoa(shifted, 16, None, 0)
    assert int(C.string_at(encoded), 16) == 0
    lib.testfree(encoded)
    lib.mpfree(shifted)
    lib.mpfree(number)
    assert lib.liveblocks() == 0
    # No-number parsing must release only internally allocated destinations.
    for text, base in [(b'', 16), (b'!', 16), (b'1', 32), (b'222', 32), (b'0', 32)]:
        assert not lib.strtomp(text, None, base, None)
        assert lib.liveblocks() == 0, (text, base, lib.liveblocks())
    # Ordinary output allocation failures must not leak the input number.
    number = lib.betomp(b'\x12\x34', 2, None)
    before = lib.liveblocks()
    for capacity in range(7):
        output = C.create_string_buffer(b'\xa5' * 16)
        result = lib.mptoa(number, 64, output, capacity)
        if capacity < 5:
            assert not result and output.raw[:16] == b'\xa5' * 16
        else:
            assert result == C.addressof(output)
            assert output.raw[:5] == b'EjQ=\0'
            assert output.raw[5:16] == b'\xa5' * 11
        assert lib.liveblocks() == before
    for base in (16, 32, 64):
        for failure in range(2 if base in (32, 64) else 1):
            lib.setfailure(failure)
            assert not lib.mptoa(number, base, None, 0)
            lib.setfailure(-1)
            assert lib.liveblocks() == before
    for text, base in [(b'22', 32), (b'AQ', 64)]:
        lib.setfailure(0)
        assert not lib.strtomp(text, None, base, number)
        lib.setfailure(-1)
        assert lib.liveblocks() == before
    lib.mpfree(number)
    assert lib.liveblocks() == 0
    print(f'PASS mp text: {checks} text round trips, {power_checks} modular power/alias cases, minimum shift and ownership')
