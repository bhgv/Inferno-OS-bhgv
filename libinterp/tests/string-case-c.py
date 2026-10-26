#!/usr/bin/env python3
"""Run production string case tables across independent cell and WORD widths."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('STRING_CASE_SOURCE', root/'libinterp/xec.c')).read_text()


# Extract actual production functions rather than mirroring their implementation.
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


parts = [function('static Inst*\ncasepc('), function('static uvlong\ncaseccell('), function('OP(casec)')]
parts += [function('OP('+name+')') for name in ('icase', 'casel', 'igoto')]
source = (root/'libinterp/string.c').read_text()
parts.insert(0, function('int\nstringcmp('))
unit = (root/'libinterp/tests/string-case-c.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='string-case-c-') as directory:
    td = Path(directory)
    target = td/'branches.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits, cells in ((64,4),(32,4),(64,8),(32,8)):
        model = td/('word'+str(bits)+'cell'+str(cells))
        model.mkdir()
        header = (root/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        isa = (root/'include/isa.h').read_text()
        isa = re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(cells), isa)
        (model/'isa.h').write_text(isa)
        includes = ['-I'+str(model)]+['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('branches'+str(bits)+'cell'+str(cells))
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
