#!/usr/bin/env python3
"""Execute production Str integer expressions in their shared Limbo/C subset."""
import ctypes as C
import os
from pathlib import Path
import re
import shlex
import string
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


# Keep source and oracle checks enabled under python -O.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Read complete brace-delimited bodies whose integer syntax is identical in C and Limbo.
def body(source, signature):
    start = source.index('{', source.index(signature))
    depth = 0
    for at in range(start, len(source)):
        if source[at] == '{':
            depth += 1
        elif source[at] == '}':
            depth -= 1
            if depth == 0:
                return source[start:at+1]
    raise RuntimeError(signature)


with tempfile.TemporaryDirectory(prefix='string-radix-host-') as directory:
    td = Path(directory)
    total = 0
    for relative in ('appl/lib/string.b', 'appl/alib/string.b'):
        source = Path(os.environ.get('STRING_RADIX_SOURCE', ROOT/relative)).read_text()
        digit = body(source, 'digit(c: int, base: int): int')
        invalid = re.search(r'else if \((base[^\n]+)\)\n\s*return \(0, 0, i\);', source)
        exponent = re.search(r'exp = (exp\*[^;]+);', source)
        require(invalid is not None and exponent is not None, ('missing numeric expressions', relative))
        unit = td/'source.c'
        unit.write_text('''/* Execute the unmodified production digit body in the common integer subset. */
int
digit(int c, int base)
BODY


/* Evaluate the actual inferred-base rejection expression. */
int
invalidbase(int base)
{
    return INVALID;
}


/* Evaluate the actual exponent accumulation expression with bounded inputs. */
int
exponentstep(int exp, int base, int d)
{
    return EXPONENT;
}
'''.replace('BODY', digit).replace('INVALID', invalid.group(1)).replace('EXPONENT', exponent.group(1)))
        binary = td/'source.so'
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(command+['-shared', '-fPIC', str(unit), '-o', str(binary)], check=True)
        # Distinct paths avoid dynamic-loader caching between the two production implementations.
        library_path = td/(relative.replace('/', '_')+'.so')
        binary.rename(library_path)
        lib = C.CDLL(str(library_path))
        lib.digit.argtypes = [C.c_int, C.c_int]
        lib.invalidbase.argtypes = [C.c_int]
        lib.exponentstep.argtypes = [C.c_int, C.c_int, C.c_int]
        alphabet = string.digits+string.ascii_lowercase
        characters = list(range(-2, 257))+[0x410, 0x3bb, 0xff10, 0x10ffff]
        checks = 0
        for base in range(-2, 40):
            for char in characters:
                letter = chr(char).lower() if char >= 0 else ''
                value = alphabet.find(letter) if len(letter) == 1 else -1
                expected = value if 2 <= base <= 36 and 0 <= value < base else -1
                require(lib.digit(char, base) == expected, ('digit', relative, base, char, expected))
                checks += 1
            require(bool(lib.invalidbase(base)) == (base < 2 or base > 36), ('inferred base', relative, base))
            checks += 1
        for base in range(2, 37):
            for previous in (0, 1, 9, 10, 99, 1999):
                for decimal in range(10):
                    require(lib.exponentstep(previous, base, decimal) == previous*10+decimal, ('decimal exponent', relative, base, previous, decimal))
                    checks += 1
        # Build visible conversion examples from the production digit/accumulation operations.
        for text, base, value, tail in [('A', 36, 10, ''), ('Z', 36, 35, ''), ('a:F', 16, 10, ':F'), ('?', 16, 0, '?'), ('19z', 10, 19, 'z')]:
            actual = 0
            at = 0
            while at < len(text):
                number = lib.digit(ord(text[at]), base)
                if number < 0:
                    break
                actual = actual*base+number
                at += 1
            require((actual, text[at:]) == (value, tail), ('conversion', relative, text, base, actual, text[at:]))
            checks += 1
        power = 0
        for char in '10':
            power = lib.exponentstep(power, 2, int(char))
        require(2**power == 1024, ('binary-base decimal exponent', relative, power))
        checks += 1
        total += checks
        print('PASS Str radix:', relative, checks, 'digit/base/exponent checks', flush=True)
    print('PASS Str radix:', total, 'checks')
