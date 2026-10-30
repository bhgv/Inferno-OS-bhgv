#!/usr/bin/env python3
"""Run production Dis parsing with real VM types/error frames and heap stubs.

The fixture exercises input truncation, metadata, data/GC-map bounds and unwind
cleanup. Cryptography, full GC, instruction execution and native JIT are stubs.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]


# Extract production functions for the parser and its ownership cleanup.
def function(path,name,result):
    selected = Path(os.environ.get('DIS_LOAD_SOURCE', root/path)) if path == 'libinterp/load.c' else root/path
    source = selected.read_text()
    start=source.index(result+'\n'+name+'(');brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(name)
source=Path(os.environ.get('DIS_LOAD_SOURCE', root/'libinterp/load.c')).read_text()
parser=source[source.index('/* Check the complete encoded operand'):source.index('Module*\nnewmod(')]
parts=['#include "dis64.h"\n', function('emu/port/dis.c','kprotect','int')]
parts.append((root/'libinterp/disc.c').read_text().replace('#include "lib9.h"','').replace('#include "isa.h"','').replace('#include "interp.h"',''))
parts += [function('libinterp/heap.c',n,r) for n,r in [('dtype','Type*'),('freetype','void'),('initmem','void'),('initarray','void')]]
parts += [function('libinterp/link.c',n,r) for n,r in [('newlink','static void'),('mlink','void'),('destroylinks','void')]]
parts += [function('libinterp/load.c',n,r) for n,r in [('operand','static int'),('disw','static ulong'),('canontod','double'),('brpatch','int'),('freemod','void')]]
if 'dissignedword(' in source:
    parts.append(function('libinterp/load.c','dissignedword','static WORD'))
unit=Path(os.environ.get('DIS_LOAD_FIXTURE',root/'libinterp/tests/dis-loader.c')).read_text().replace('IMPLEMENTATION','\n'.join(parts)+'\n'+parser)
with tempfile.TemporaryDirectory(prefix='dis-loader-') as directory:
    td=Path(directory);p=td/'loader.c';p.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O2 -g'))
    model=td/'model'
    model.mkdir()
    header=(root/'include/interp.h').read_text()
    if os.environ.get('DIS_WORD_BYTES') == '4':
        header='#define DIS_WORD32 1 // Select the actual production scalar model.\n'+header
    (model/'interp.h').write_text(header)
    command += ['-Wno-sign-compare','-I'+str(model)]
    command += ['-I'+str(root/p) for p in ['Linux/arm/include','emu/port','include','libinterp']]
    subprocess.run(command+[str(p),'-o',str(td/'loader')],check=True)
    subprocess.run([str(td/'loader')],check=True)
