#!/usr/bin/env python3
"""Execute production array address instructions with real VM declarations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
case=os.environ.get('ARRAY_INDEX_CASE','')
if case not in ('','valid','zero','invalid','length','descriptor','wrap'):raise ValueError(case)
source=Path(os.environ.get('ARRAY_INDEX_SOURCE',root/'libinterp/xec.c')).read_text()


def function(signature):
    """Extract a complete body while skipping comment and literal braces."""
    start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)


parts=[]
if 'static uchar*\narrayaddress(' in source:parts.append(function('static uchar*\narrayaddress('))
if 'static void\narrayindex(' in source:parts.append(function('static void\narrayindex('))
parts += [function('OP('+name+')') for name in ('indx','indb','indw','indl','indf','lena')]
unit=(root/'libinterp/tests/array-indices.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='array-indices-') as directory:
    td=Path(directory);target=td/'indices.c';target.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))
    command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        includes=[]
        if bits==32:
            model=td/'word32';model.mkdir()
            header=(root/'include/interp.h').read_text()
            header,n=re.subn(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
            header,m=re.subn(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            assert n==m==1
            (model/'interp.h').write_text(header);includes=['-I'+str(model)]
        includes += ['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary=td/('indices'+str(bits))
        subprocess.run(command+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)]+([case] if case else []),check=True)
