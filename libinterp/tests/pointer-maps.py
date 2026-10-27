#!/usr/bin/env python3
"""Execute production pointer maps at every byte alignment with exact byte/order oracles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('HEAP_MAP_SOURCE', root/'libinterp/heap.c')).read_text()


def function(name):
    """Extract a complete production function without replacing its behavior."""
    start = source.index('void\n'+name+'(')
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:return source[start:brace+token.end()]
    raise ValueError(name)


unit = (root/'libinterp/tests/pointer-maps.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(function(name) for name in ('incmem','freeptrs','scanptrs','initmem')))
# Old helpers use BIT directly; keep that source dependency in substitution runs.
unit = '#define BIT(v,n) ((v)&(1<<(n)))\n'+unit
selected = os.environ.get('POINTER_MAP_CASE','')
choices = {'inc':1,'free':2,'scan':3,'init':4}
if selected and selected not in choices:raise ValueError(selected)
with tempfile.TemporaryDirectory(prefix='pointer-maps-') as directory:
    td = Path(directory)
    target = td/'maps.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    if selected:cmd += ['-DMAP_OPERATION='+str(choices[selected])]
    widths=tuple(map(int,os.environ.get('POINTER_MAP_WORD_BITS','64 32').split()))
    if not widths or any(bits not in (32,64) for bits in widths):
        raise ValueError('POINTER_MAP_WORD_BITS must select 32 and/or 64')
    for bits in widths:
        model = td/('word'+str(bits))
        model.mkdir()
        header = (root/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('maps'+str(bits))
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
