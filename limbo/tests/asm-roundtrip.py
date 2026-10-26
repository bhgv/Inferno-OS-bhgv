#!/usr/bin/env python3
"""Compare production compiler assembly directives with binary initializer images and lexical bytes."""
import json
import os
from pathlib import Path
import random
import re
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract actual compiler functions while ignoring comments and quoted braces.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


# Enforce every oracle under python -O too.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


# Decode the assembler's quoted byte spelling, requiring the closing quote to end the record.
def quoted(text):
    require(text.startswith(b'"'), ('missing quote', text))
    result = bytearray()
    at = 1
    while at < len(text):
        byte = text[at]
        at += 1
        if byte == 34:
            require(at == len(text), ('trailing quoted input', text))
            return bytes(result)
        if byte == 92:
            require(at < len(text), ('unfinished escape', text))
            byte = text[at]
            at += 1
            byte = {ord('n'): 10, ord('z'): 0, 34: 34, 92: 92}.get(byte, byte)
        result.append(byte)
    raise RuntimeError(('unterminated quote', text))


assembly = Path(os.environ.get('ASM_ROUNDTRIP_SOURCE', ROOT/'limbo/asm.c')).read_text()
quote_source = assembly if 'static void\nasmquoted(' in assembly else (ROOT/'limbo/asm.c').read_text()
parts = [function(quote_source, 'static void\nasmquoted(')]
parts += [function(assembly, signature) for signature in (
    'void\nasmpath(', 'void\nasmstring(', 'void\nasminitializer(')]
parts.insert(0, function((ROOT/'limbo/dtocanon.c').read_text(), 'void\ndtocanon('))
parts.insert(0, function((ROOT/'limbo/com.c').read_text(), 'Long\ncaseend('))
binary = (ROOT/'limbo/dis.c').read_text()
binary = binary[binary.index('static void\ndisdatum('):]
arms = []
for kind in ('Tcasel', 'Treal'):
    match = re.search(r'\tcase '+kind+r':[\s\S]*?\t\tbreak;', binary)
    require(match is not None, ('missing binary arm', kind))
    arms.append(match.group())
unit = (ROOT/'limbo/tests/asm-roundtrip.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts)).replace('BINARY_ARMS', '\n'.join(arms))
requests = []
for count in range(6):
    for wildcard in (0, 1):
        for pattern in range(4):
            requests.append(('t', f't {count} {wildcard} {pattern}', None))
bits = [0, 1 << 63, 1, (1 << 63)+1, 0x0010000000000000, 0x7fefffffffffffff,
        0x7ff0000000000000, 0xfff0000000000000, 0x7ff0000000000001,
        0x7ff8000000000000, 0x7fffffffffffffff, 0xfff0000000000001,
        0xfff8000000000042, 0xffffffffffffffff, 0x3ff0000000000001]
rng = random.Random(0x41534d)
bits += [rng.getrandbits(64) for _ in range(1024)]
for sign in (0, 1):
    bits += [0x7ff0000000000000 | (sign << 63) | (1 << bit) for bit in range(52)]
    bits += [0x7ff0000000000000 | (sign << 63) | (rng.getrandbits(52) or 1) for _ in range(128)]
for value in bits:
    requests.append(('r', 'r '+format(value, '016x'), value))
strings = [b'', b'a', b'C:\\new\\folder\\"quoted"\\source.b', b'line\nsource.b',
           bytes(range(128)), 'каталог/исходник λ.b'.encode(), b'a\x00b', b'"\\\n\x00']
strings += [bytes([value]) for value in range(128)]
for value in strings:
    requests.append(('q', 'q '+(value.hex() or '-'), value))
selected = os.environ.get('ASM_ROUNDTRIP_KIND')
if selected:
    require(selected in ('t', 'r', 'q'), ('unknown record kind', selected))
    requests = [request for request in requests if request[0] == selected]
input_data = ('\n'.join(request for _, request, _ in requests)+'\n').encode()
with tempfile.TemporaryDirectory(prefix='asm-roundtrip-') as directory:
    td = Path(directory)
    common = td/'dtoa'
    common.mkdir()
    header = '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <math.h>\n#define nil NULL\n'
    header += '#undef __LITTLE_ENDIAN\n#undef __BIG_ENDIAN\n'
    header += '#define __LITTLE_ENDIAN 1\n' if sys.byteorder == 'little' else '#define __BIG_ENDIAN 1\n'
    (common/'lib9.h').write_text(header)
    mathunit = common/'math.c'
    mathunit.write_text('#include "lib9.h"\n#define strtod fixture_strtod\n#include '+json.dumps(str(ROOT/'libmath/dtoa.c'))+'\n#undef strtod\n#include '+json.dumps(str(ROOT/'libmath/g_fmt.c'))+'\n')
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    mathobject = common/'math.o'
    subprocess.run(command+['-fno-strict-aliasing', '-I'+str(common), '-c', str(mathunit), '-o', str(mathobject)], check=True)
    target = td/'fixture.c'
    target.write_text(unit)
    total = 0
    for cells in (4, 8):
        model = td/str(cells)
        model.mkdir()
        isa = re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(cells), (ROOT/'include/isa.h').read_text())
        (model/'isa.h').write_text(isa)
        executable = model/'fixture'
        includes = [model]+[ROOT/path for path in ('Linux/arm/include', 'include', 'limbo')]
        subprocess.run(command+['-I'+str(path) for path in includes]+[str(target), str(mathobject), '-o', str(executable), '-lm'], check=True)
        output = subprocess.run([str(executable)], input=input_data, stdout=subprocess.PIPE, check=True).stdout
        blocks = output.split(b'BEGIN\n')[1:]
        require(len(blocks) == len(requests), ('record count', cells, len(blocks)))
        for request, block in zip(requests, blocks):
            kind, original, expected = request
            require(block.endswith(b'END\n'), ('record end', original))
            lines = block[:-4].split(b'\n')
            lines = [line for line in lines if line]
            if kind == 'q':
                require(lines[0].startswith(b'\tstring\t@mp+64,'), ('string directive', original))
                require(quoted(lines[0].split(b',', 1)[1]) == expected, ('string bytes', original))
                if b'\x00' not in expected:
                    require(len(lines) == 2 and lines[1].startswith(b'\tsource\t'), ('source directive', original))
                    require(quoted(lines[1][len(b'\tsource\t'):]) == expected, ('source bytes', original))
                else:
                    require(len(lines) == 1, ('source fixture', original))
            else:
                require(lines[-1].startswith(b'IMAGE '), ('missing image', original))
                expected_image = bytes.fromhex(lines[-1][6:].decode())
                image = bytearray(512)
                for line in lines[:-1]:
                    directive = line.split(b'#', 1)[0].strip()
                    match = re.fullmatch(rb'(word|long|real)\s+@mp\+(\d+),(.+)', directive)
                    require(match is not None, ('data directive', original, directive))
                    name, offset, values = match.groups()
                    at = int(offset)
                    for value in values.split(b','):
                        if name == b'real':
                            data = struct.pack('=d', float(value))
                        else:
                            width = cells if name == b'word' else 8
                            data = (int(value) & ((1 << (width*8))-1)).to_bytes(width, sys.byteorder)
                        image[at:at+len(data)] = data
                        at += len(data)
                require(bytes(image) == expected_image, ('initializer mismatch', cells, original))
                if kind == 'r':
                    require(int.from_bytes(image[64:72], sys.byteorder) == expected, ('real bits', original))
            total += 1
        print('PASS assembly roundtrip:', len(requests), 'records cell', cells, flush=True)
    print('PASS assembly roundtrip:', total, 'table/float/quote records')
