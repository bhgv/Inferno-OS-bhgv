#!/usr/bin/env python3
"""Exercise production String header guards and duplication source spans."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(signature):
    """Extract production C bodies while ignoring braces in comments and literals."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


source = Path(os.environ.get('STRING_HEADER_SOURCE', root/'libinterp/string.c')).read_text()
copy = function('String*\nstringdup(')
model = copy.replace('stringdup(', 'modeldup(').replace('uintptr address;', 'HeaderAddress address;')
model = model.replace('(uintptr)s', '(HeaderAddress)(uintptr)s').replace('(uintptr)-1', '(HeaderAddress)-1')
parts = [copy, model, function('int\nstringcmp(')]
unit=(root/'libinterp/tests/string-header-spans.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='string-header-spans-') as directory:
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
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
