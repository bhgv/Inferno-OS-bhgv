#!/usr/bin/env python3
"""Production channel creation/cleanup with real VM types and error frames."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
def function(path,signature):
    source=(root/path).read_text();start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)
for name, result in [('kprotect','int'),('krethrow','void')]:
    assert function('emu/port/dis.c',result+'\n'+name+'(') == function('os/port/dis.c',result+'\n'+name+'(')
parts=[function('emu/port/dis.c','int\nkprotect('),function('emu/port/dis.c','void\nkrethrow(')]
parts += [function('libinterp/heap.c',r+'\n'+n+'(') for n,r in [('dtype','Type*'),('freetype','void'),('acheck','void'),('initmem','void'),('freeptrs','void'),('nheap','Heap*'),('heap','Heap*'),('heaparray','Heap*'),('initarray','void'),('freechan','void'),('freearray','void')]]
src=(root/'libinterp/xec.c').read_text()
parts += [src[src.index('typedef struct ChannelBuild'):src.index('OP(newcl)')], src[src.index('typedef struct RawChannel'):src.index('OP(newcmp)')]]
unit=(root/'libinterp/tests/channels.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='channels-') as directory:
    td=Path(directory);p=td/'channels.c';p.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O2 -g'))
    cmd += ['-I'+str(root/p) for p in ['Linux/arm/include','emu/port','include','libinterp']]
    subprocess.run(cmd+[str(p),'-o',str(td/'channels')],check=True)
    subprocess.run([str(td/'channels')],check=True)
