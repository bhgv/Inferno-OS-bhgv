#!/usr/bin/env python3
"""Compare both production tokenizers against an independent Unicode oracle."""
import itertools
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def decode(data):
    """Decode one scalar, consuming exactly one byte on malformed input."""
    result = []
    i = 0
    while i < len(data):
        for n in range(1, min(4, len(data)-i)+1):
            try:
                text = data[i:i+n].decode('utf-8')
            except UnicodeDecodeError:
                continue
            if len(text) == 1:
                result.append((text, data[i:i+n], i))
                i += n
                break
        else:
            result.append(('\ufffd', data[i:i+1], i))
            i += 1
    return result


def oracle(data, sep, maxargs, compact):
    chars = decode(data)
    delimiters = {c for c, _, _ in decode(sep)}
    tokens = []
    position = 0
    cuts = set()
    starts = []
    while len(starts) < maxargs:
        while position < len(chars) and chars[position][0] in delimiters:
            cuts.add(position)
            position += 1
        if position == len(chars):
            break
        starts.append(chars[position][2])
        quoted = False
        out = bytearray()
        while position < len(chars):
            c, raw, _ = chars[position]
            if not quoted and c in delimiters:
                break
            position += 1
            if c != "'":
                out.extend(raw)
            elif not quoted:
                quoted = True
            elif position < len(chars) and chars[position][0] == "'":
                out.extend(b"'")
                position += 1
            else:
                quoted = False
        if compact:
            tokens.append(bytes(out))
    if compact:
        return list(zip(starts, tokens))
    modified = b''.join(b'\0'*len(raw) if i in cuts else raw
                        for i, (_, raw, _) in enumerate(chars))
    return [(start, modified[start:].split(b'\0', 1)[0]) for start in starts]


with tempfile.TemporaryDirectory(prefix='tokenize-utf-') as directory:
    directory = Path(directory)
    vectors = directory / 'vectors'
    count = 0
    with vectors.open('wb') as f:
        f.write(b'\0'*4)
        alphabet = [b'a', b"'", b' ', '¢'.encode(), 'Â'.encode(), '€'.encode()]
        inputs = [b''.join(p) for n in range(5) for p in itertools.product(alphabet, repeat=n)]
        inputs += [b'\xffa\x80', b'a\xe2\x82', '😀a😀'.encode(), "'😀'😀''".encode(),
                   b"a''b 'c''d' e", b"'unterminated x", b'\xc2', b'\xe2', b'\xf0\x9f\x98']
        separators = [b' ', '¢'.encode(), 'Â'.encode(), '€'.encode(), '😀'.encode(),
                      ' ¢€'.encode(), b'', '\ufffd'.encode()]
        for data in inputs:
            for mode, sep in [(0, s) for s in separators]+[(1, b' \t\r\n')]:
                for maxargs in (0, 1, 2, 3, 6):
                    expected = oracle(data, sep, maxargs, mode)
                    f.write(struct.pack('<4I', mode, maxargs, len(data), len(sep)))
                    f.write(data+sep)
                    f.write(struct.pack('<I', len(expected)))
                    for offset, token in expected:
                        f.write(struct.pack('<2I', offset, len(token)))
                        f.write(token)
                    count += 1
        f.seek(0)
        f.write(struct.pack('<I', count))
    for library in ('lib9', 'libkern'):
        for charflag in ('-fsigned-char', '-funsigned-char'):
            binary = directory / (library+charflag)
            cmd = shlex.split(os.environ.get('CC', 'cc'))
            cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            cmd += [charflag]+['-I'+str(root / p) for p in ('Linux/arm/include', 'include')]
            tokenize = Path(os.environ.get('TOKENIZE_SOURCE', root / library / 'tokenize.c'))
            sources = [tokenize]+[root / library / (name+'.c') for name in ('rune', 'utfrune')]
            subprocess.run(cmd+[str(root / 'lib9/tests/tokenize-utf.c')]+[str(p) for p in sources]+['-o', str(binary)], check=True)
            subprocess.run([str(binary), str(vectors)], check=True, timeout=60)
