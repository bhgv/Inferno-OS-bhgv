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
    source=Path(os.environ.get('CHANNEL_CREATE_SOURCE',root/path) if path=='libinterp/xec.c' else root/path).read_text();start=source.index(signature);brace=source.index('{',start);depth=0
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
src=Path(os.environ.get('CHANNEL_CREATE_SOURCE',root/'libinterp/xec.c')).read_text()
parts += [src[src.index('typedef struct ChannelBuild'):src.index('OP(newcl)')], src[src.index('typedef struct RawChannel'):src.index('OP(newcmp)')]]
unit=(root/'libinterp/tests/channels.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='channels-') as directory:
    td=Path(directory);p=td/'channels.c';p.write_text(unit)
    cmd=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        includes=[]
        if bits==32:
            model=td/'word32';model.mkdir()
            header=(root/'include/interp.h').read_text()
            header=re.sub(r'typedef long\s+WORD;', 'typedef int WORD;',header,count=1)
            header=re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;',header,count=1)
            (model/'interp.h').write_text(header);includes=['-I'+str(model)]
        includes+=['-I'+str(root/p) for p in ['Linux/arm/include','emu/port','include','libinterp']]
        binary=td/('channels'+str(bits))
        subprocess.run(cmd+includes+[str(p),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
