#!/usr/bin/env python3
"""Compile production Math codecs against bounded WORD32/WORD64 array models."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'libinterp/math.c').read_text()


# Extract complete C functions without counting braces in strings or comments.
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
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [source[source.index('typedef union MathBits64'):source.index('/* Check full VM lengths')]] if 'typedef union MathBits64' in source else [source[source.index('static union'):source.index('void\nmathmodinit')]]
for signature in ('static int\nmatharraycount(', 'static WORD\nmathsigned32('):
    if signature in source:
        parts.append(function(signature))
names = ['import_int', 'export_int', 'import_real32', 'export_real32',
         'import_real', 'export_real', 'bits32real', 'realbits32',
         'bits64real', 'realbits64', 'lgamma']
parts += [function('void\nMath_'+name+'(') for name in names]
unit = (root / 'libinterp/tests/math-codecs.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='math-codecs-') as directory:
    td = Path(directory)
    (td / 'math.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (32, 64):
        binary = td / ('math'+str(width))
        subprocess.run(cmd + ['-DWORD_BITS='+str(width), str(td / 'math.c'), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
