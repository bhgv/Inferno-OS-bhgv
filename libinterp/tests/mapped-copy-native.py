#!/usr/bin/env python3
"""Compare actual native mapped record copies with C and the FP environment."""
import os
import platform
import sys
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

if platform.machine().lower() not in ('x86_64','amd64'):
    print('SKIP native AMD64 mapped record copies: requires an x86-64 host')
    sys.exit(0)

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('MAPPED_COPY_SOURCE', root/'libinterp/xec.c')).read_text()


def function(signature):
    """Extract production C bodies while ignoring braces in comments and literals."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:return source[start:brace+token.end()]
    raise ValueError(signature)


parts=[function('static Type*\nmoduletype('),function('static List*\nlisthead('),function('OP(movmp)'),function('OP(headmp)')]
if 'static int\nmappedcopytype(' in source:
    parts.insert(0, function('static int\nmappedcopytype('))

unit=(root/'libinterp/tests/mapped-copy-native.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='mapped-copy-native-') as directory:
    td = Path(directory)
    target = td/'branches.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        model = td/('word'+str(bits))
        model.mkdir()
        header = (root/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('branches'+str(bits))
        subprocess.run(cmd+includes+[str(target),'-o',str(binary),'-lm'],check=True)
        subprocess.run([str(binary)],check=True)
