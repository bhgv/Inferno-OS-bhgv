#!/usr/bin/env python3
"""Check production GUI descriptor maps with native records and real pointer ownership."""
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(text, signature):
    """Extract complete functions while ignoring braces inside comments and strings."""
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


draw = Path(os.environ.get('DRAW_MAP_SOURCE', root / 'libinterp/draw.c')).read_text()
prefab = Path(os.environ.get('PREFAB_MAP_SOURCE', root / 'libinterp/prefab.c')).read_text()
font = (root/'libinterp/freetype.c').read_text()
heap = (root / 'libinterp/heap.c').read_text()
start = draw.index('typedef struct Cache Cache;')
end = draw.index('\nCache*\tsfcache', start)
declarations = draw[start:end]
parts = [function(heap, result+'\n'+name+'(') for result, name in
         [('Type*','dtype'), ('void','freetype'), ('void','initmem'), ('void','freeptrs'),
          ('void','incmem'), ('Heap*','heap')]]
parts += [function(draw, 'void\ndrawmodinit('), function(prefab, 'void\nprefabmodinit('), function(font, 'void\nfreetypemodinit(')]
unit = (root / 'libinterp/tests/gui-type-maps.c').read_text()
unit = unit.replace('WRAPPERS', declarations).replace('FONT_WRAPPER', font[font.index('typedef struct Face Face;'):font.index('Type*\tTMatrix')]).replace('IMPLEMENTATION','\n'.join(parts))
unit = unit.replace('#include <assert.h>', '#define mapcheck(test) do { if(!(test)) { fprintf(stderr,"GUI map predicate failed at %d\\n",__LINE__); abort(); } } while(0) // Active optimized predicates.')
unit = unit.replace('assert(', 'mapcheck(')
arch = os.environ.get('NATIVE64_ARCH','amd64')
runner = shlex.split(os.environ.get('NATIVE64_RUNNER',''))
with tempfile.TemporaryDirectory(prefix='gui-type-maps-') as directory:
    td = Path(directory)
    target = td / 'maps.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('NATIVE64_CFLAGS',os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all')))
    for bits in (64,32):
        includes = []
        if bits == 32:
            model = td / 'word32'
            model.mkdir(exist_ok=True)
            header = (root / 'include/interp.h').read_text()
            header, words = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, unsigned = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if words != 1 or unsigned != 1:
                raise RuntimeError('GUI map word-model override drift')
            (model / 'interp.h').write_text(header)
            includes = ['-I'+str(model)]
        includes += ['-I'+str(root / p) for p in ('Linux/'+arch+'/include','Linux/386/include','include','libinterp')]
        binary = td / 'maps'
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run(runner+[str(binary)]+os.environ.get('GUI_MAP_CASE','').split(),check=True)
        print('WORD'+str(bits), flush=True)
