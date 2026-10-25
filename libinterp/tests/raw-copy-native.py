#!/usr/bin/env python3
"""Compare actual C and published AMD64 raw copies at ownership-free byte boundaries."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

if platform.machine().lower() not in ('x86_64', 'amd64'):
    raise RuntimeError('native raw-copy execution requires an x86-64 host')
ROOT = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('RAW_COPY_SOURCE', ROOT/'libinterp/xec.c')).read_text()


# Ignore comments and literals while extracting complete production C bodies.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [function(signature) for signature in ('OP(movm)', 'static List*\nlisthead(', 'OP(headm)')]
unit = (ROOT/'libinterp/tests/raw-copy-native.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='raw-copy-native-') as directory:
    td = Path(directory)
    target = td/'copies.c'; target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (4, 8):
        model = td/str(bits); model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 4:
            header, words = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, unsigned = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if words != 1 or unsigned != 1:raise RuntimeError('WORD32 header mismatch')
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/name) for name in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('copy'+str(bits))
        subprocess.run(command+includes+[str(target), '-o', str(binary), '-lm'], check=True)
        subprocess.run([str(binary)], check=True)
