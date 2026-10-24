#!/usr/bin/env python3
"""Run actual VM integer shifts and division with alignment and overlapping slots."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('SHIFT_DIV_SOURCE', root/'libinterp/xec.c')).read_text()


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


names = ('shlb','shrb','shlw','shrw','lsrw','shll','shrl','lsrl','divb','modb','divw','modw','divl','modl')
parts = [function('static WORD\nshiftcount(')]+[function('OP('+name+')') for name in names]
unit = (root/'libinterp/tests/shift-div.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
selected=os.environ.get('SHIFT_DIV_CASE','')
if selected not in ('','div'):raise ValueError(selected)
with tempfile.TemporaryDirectory(prefix='shift-div-') as directory:
    td = Path(directory)
    target = td/'branches.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    if selected=='div':cmd+=['-DSHIFT_DIV_FIRST=8']
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
        for op in range(14):subprocess.run([str(binary),str(op)],check=True)
