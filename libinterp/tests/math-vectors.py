#!/usr/bin/env python3
"""Run production Math sorting/vector/matrix paths in two VM word models."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'libinterp/math.c').read_text()


# Extract C functions while ignoring braces inside comments and literals.
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


start = source.index('static int\nmathvectorcount(') if 'static int\nmathvectorcount(' in source else source.index('static int\ncmp(')
parts = [source[start:source.index('/************ BLAS')]]
if 'static void\nmathmatrix(' in source:
    parts.append(function('static void\nmathmatrix('))
parts += [function('void\nMath_'+name+'(') for name in ['dot', 'iamax', 'norm1', 'norm2', 'gemm']]
for path in ('libmath/blas.c', 'libmath/gemm.c'):
    parts.append(re.sub(r'^#include[^\n]*\n', '', (root / path).read_text(), flags=re.M))
unit = (root / 'libinterp/tests/math-vectors.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='math-vectors-') as directory:
    td = Path(directory)
    (td / 'vectors.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (32, 64):
        binary = td / ('vectors'+str(width))
        subprocess.run(cmd + ['-DWORD_BITS='+str(width), str(td / 'vectors.c'), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
