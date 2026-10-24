#!/usr/bin/env python3
"""Run native-descriptor record moves and typed channel transfers with real VM bodies."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(text, signature):
    """Extract whole source functions while ignoring comment and literal braces."""
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


source = Path(os.environ.get('MAPPED_MOVE_SOURCE', root / 'libinterp/xec.c')).read_text()
heap = (root / 'libinterp/heap.c').read_text()
parts = [function(heap, result+'\n'+name+'(') for result, name in
         [('void','incmem'),('void','freeptrs'),('void','initmem'),('void','acheck'),
          ('Heap*','nheap'),('Heap*','heaparray'),('void','initarray'),('void','freearray')]]
parts += [function(source, 'static Type*\nmoduletype('), function(source, 'OP(movmp)'), function(source, 'void\nmovtmp(')]
parts += [function(source, 'static int\n'+name+'(') for name in ('cgetb','cputb')]
parts += [function(source, 'void\n'+name+'(') for name in ('cqadd','cqdel')]
parts += [function(source, 'OP('+name+')') for name in ('isend','irecv')]
unit = (root / 'libinterp/tests/mapped-moves.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='mapped-moves-') as directory:
    td = Path(directory)
    target = td / 'moves.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        includes = []
        if bits == 32:
            model = td / 'word32'
            model.mkdir(exist_ok=True)
            header = (root / 'include/interp.h').read_text()
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (model / 'interp.h').write_text(header)
            includes = ['-I'+str(model)]
        includes += ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td / 'moves'
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)]+os.environ.get('MAPPED_MOVE_CASE','').split(),check=True)
        print('WORD'+str(bits), flush=True)
