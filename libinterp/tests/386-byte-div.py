#!/usr/bin/env python3
"""Execute production x86 byte division emission across the full byte domain."""
import ctypes as C
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
)

root = Path(__file__).resolve().parents[2]
source = (root / 'libinterp/comp-386.c').read_text()


# Compile the actual emitter helpers and register/opcode constants.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace + token.end()]
    raise ValueError(signature)


constants = source[source.index('enum\n'):source.index('static\tuchar*\tcode;')]
parts = [function('static void\n' + name + '(') for name in ['genb', 'gen2']]
bytecase = source[source.index('\tcase IMODB:\n\tcase IDIVB:\n\tcase IMULB:'):]
bytecase = bytecase[:bytecase.index('\n\tcase IINDX:')]
if 'divbyte(i->op == IMODB)' in bytecase:
    parts.append(function('static void\ndivbyte('))
else:
    # Retain baseline execution so this test demonstrates the original bug.
    old = re.search(r'else \{([\s\S]*?)\n\t\t}', bytecase).group(1)
    old = old.replace('i->op == IMODB', 'remainder')
    parts.append('static void divbyte(int remainder) {' + old + '\n}')
unit = '''
#include <stdint.h>
#include <string.h>
typedef uint8_t uchar;
CONSTANTS
static uchar *code;
IMPLEMENTATION
/* Expose production code bytes to the execution oracle. */
int
emit(uchar *out, int remainder)
{
    code = out;
    divbyte(remainder);
    return code-out;
}
'''.replace('CONSTANTS', constants).replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='386-byte-div-') as directory:
    td = Path(directory)
    (td / 'emit.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g'))
    subprocess.run(cmd + ['-shared', '-fPIC', str(td / 'emit.c'),
                          '-o', str(td / 'emit.so')], check=True)
    library = C.CDLL(str(td / 'emit.so'))
    library.emit.argtypes = [C.POINTER(C.c_ubyte), C.c_int]
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    base = 0x10000
    machine.mem_map(base, 0x1000)
    checks = 0
    for remainder in (0, 1):
        buffer = (C.c_ubyte * 64)(*[0xa5] * 64)
        length = library.emit(buffer, remainder)
        assert 0 < length < 64
        assert bytes(buffer)[length:] == b'\xa5' * (64 - length)
        blob = bytes(buffer)[:length]
        listing = list(Cs(CS_ARCH_X86, CS_MODE_32).disasm(blob, base))
        assert any(i.mnemonic in ('div', 'idiv') for i in listing)
        machine.mem_write(base + remainder * 0x100, blob)
        start = base + remainder * 0x100
        for dividend in range(256):
            for divisor in range(1, 256):
                machine.reg_write(UC_X86_REG_EAX, 0xa5a5ff00 | dividend)
                machine.reg_write(UC_X86_REG_EBX, 0xdeadbe00 | divisor)
                machine.reg_write(UC_X86_REG_EDX, 0xffffffff)
                for register in (UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                    machine.reg_write(register, 0x12345678)
                machine.emu_start(start, start + length, count=20)
                result = machine.reg_read(UC_X86_REG_EAX)
                want = dividend % divisor if remainder else dividend // divisor
                assert result == want, (remainder, dividend, divisor, result, want)
                for register in (UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
                    assert machine.reg_read(register) == 0x12345678
                checks += 1
        # Division by zero must still raise a native exception for runtime handling.
        machine.reg_write(UC_X86_REG_EAX, 255)
        machine.reg_write(UC_X86_REG_EBX, 0xabcd0000)
        try:
            machine.emu_start(start, start + length, count=20)
        except UcError:
            checks += 1
        else:
            raise AssertionError('zero divisor did not fault')
    print(f'PASS x86 byte div/mod: {checks} executed cases, dirty registers and zero divisors')
