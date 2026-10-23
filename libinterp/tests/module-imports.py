#!/usr/bin/env python3
"""Run production load/import lookup with full-width VM indices and nil data."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('XEC_SOURCE', root / 'libinterp/xec.c')).read_text()


# Extract complete production bodies, ignoring braces in comments and literals.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace + token.end()]
    raise ValueError(signature)


parts = []
if 'moduleimports(Module' in source:
    parts.append(function('static Import*\nmoduleimports('))
parts.append(function('OP(iload)'))
unit = (root / 'libinterp/tests/module-imports.c').read_text()
with tempfile.TemporaryDirectory(prefix='module-imports-') as directory:
    td = Path(directory)
    target = td / 'imports.c'
    target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
    for width in (32, 64):
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS',
                           '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        binary = td / ('imports' + str(width))
        subprocess.run(cmd + ['-DWORD_BITS=' + str(width), str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=15)
