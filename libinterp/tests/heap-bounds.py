#!/usr/bin/env python3
"""Exercise production heap sizing and slice paths with a bounded pool stub."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
def function(path,signature):
    source=Path(os.environ.get('HEAP_XEC_SOURCE', root/path) if path=='libinterp/xec.c' else root/path).read_text();start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)
parts=[function('emu/port/dis.c','int\nkprotect(')]
parts += [function('libinterp/heap.c',r+'\n'+n+'(') for n,r in [('incmem','void'),('freeptrs','void'),('acheck','void'),('nheap','Heap*'),('heap','Heap*'),('initmem','void'),('heapz','Heap*'),('initarray','void'),('heaparray','Heap*'),('arraycpy','void*')]]
parts += [function('libinterp/link.c','Modlink*\nmklinkmod('),function('libinterp/xec.c','List*\ncons(')]
parts += [function('libinterp/xec.c','OP('+n+')') for n in ['newa','newaz','slicea','slicela']]
unit=(root/'libinterp/tests/heap-bounds.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='heap-bounds-') as directory:
    td=Path(directory);p=td/'heap.c';p.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O2 -g'))
    command += ['-I'+str(root/p) for p in ['Linux/arm/include','emu/port','include','libinterp']]
    subprocess.run(command+[str(p),'-o',str(td/'heap')],check=True)
    subprocess.run([str(td/'heap')],check=True)
