#!/usr/bin/env python3
"""Exercise real string/quote formatting with an independent UTF-8 scalar oracle."""
import itertools
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def scalars(raw, precision=None):
    """Native malformed input consumes one byte; finite precision omits partial tails."""
    result = ''
    end = len(raw) if precision is None else min(len(raw), precision)
    i = 0
    while i < end:
        first = raw[i]
        required = 1 if first < 0x80 else 2 if first < 0xe0 else 3 if first < 0xf0 else 4
        if precision is not None and required > end-i:
            break
        for size in range(1, min(4, end-i)+1):
            try:
                c = raw[i:i+size].decode('utf-8')
            except UnicodeDecodeError:
                continue
            if len(c) == 1:
                result += c
                i += size
                break
        else:
            result += '\ufffd'
            i += 1
    return result


def quote(text, sharp):
    if sharp or not text or any(ord(c) <= 32 or c == "'" for c in text):
        return "'"+text.replace("'", "''")+"'"
    return text


with tempfile.TemporaryDirectory(prefix='utf-format-') as directory:
    directory = Path(directory)
    data = directory / 'vectors'
    count = 0
    with data.open('wb') as f:
        f.write(b'\0'*4)
        alphabet = [b'a', b' ', b"'", b'\x80', b'\xc2', b'\xa2']
        inputs = [b''.join(p) for n in range(5) for p in itertools.product(alphabet, repeat=n)]
        inputs += ['¢€😀'.encode(), "€ '😀' x".encode(), b'\xe2\x82', b'\xf0\x9f\x98',
                   b'\xed\xa0\x80', b'\xff', b'\xe2a\xac']
        if os.environ.get('UTF_FORMAT_VALID_ONLY') == '1':
            inputs = [raw for raw in inputs if '\ufffd' not in scalars(raw)]
        for raw in inputs:
            decoded = scalars(raw)
            for verb in ('s', 'S', 'q', 'Q'):
                for precision in (None, 0, 1, 2, len(raw)):
                    for width in (0, 7, -7):
                        for sharp in ((False, True) if verb.lower() == 'q' else (False,)):
                            text = decoded
                            if verb == 's' and precision is not None:
                                text = scalars(raw, precision)
                            elif verb == 'S' and precision is not None:
                                text = text[:precision]
                            elif verb.lower() == 'q':
                                text = quote(text, sharp)
                            text = text.ljust(-width) if width < 0 else text.rjust(width)
                            expected = '['+text+']END'
                            fmt = '[%'+('#' if sharp else '')+(str(width) if width else '')
                            fmt += ('.'+str(precision) if precision is not None else '')+verb+']END'
                            encoded = expected.encode()
                            fmt = fmt.encode()
                            f.write(struct.pack('<6I', int(verb.isupper()), len(raw), len(fmt),
                                                len(decoded), len(encoded), len(expected)))
                            f.write(raw+fmt)
                            for c in decoded:
                                f.write(struct.pack('<I', ord(c)))
                            f.write(encoded)
                            for c in expected:
                                f.write(struct.pack('<I', ord(c)))
                            count += 1
        f.seek(0)
        f.write(struct.pack('<I', count))
    names = ('rune', 'snprint', 'vsnprint', 'dofmt', 'fmt', 'fmtlock', 'errfmt',
             'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'fmtquote', 'runeseprint', 'runevseprint')
    for library in ('lib9', 'libkern'):
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'lib9')]
        sources = []
        for name in names:
            source = root / library / (name+'.c')
            if not source.exists():
                source = root / 'lib9' / (name+'.c')
            source = Path(os.environ.get('UTF_FORMAT_'+name.upper(), source))
            sources.append(str(source))
        binary = directory / library
        subprocess.run(cmd+[str(root / 'lib9/tests/utf-format.c')]+sources+['-o', str(binary)], check=True)
        subprocess.run([str(binary), str(data)]+(['wide'] if os.environ.get('UTF_FORMAT_WIDE') == '1' else []), check=True, timeout=120)
