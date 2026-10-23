#!/usr/bin/env python3
"""Check production GUI descriptor maps with native records and real pointer ownership."""
import os
from pathlib import Path
import re
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
heap = (root / 'libinterp/heap.c').read_text()
start = draw.index('typedef struct Cache Cache;')
end = draw.index('\nCache*\tsfcache', start)
declarations = draw[start:end]
parts = [function(heap, result+'\n'+name+'(') for result, name in
         [('Type*','dtype'), ('void','freetype'), ('void','initmem'), ('void','freeptrs'),
          ('void','incmem'), ('Heap*','heap')]]
parts += [function(draw, 'void\ndrawmodinit('), function(prefab, 'void\nprefabmodinit(')]
unit = (root / 'libinterp/tests/gui-type-maps.c').read_text()
unit = unit.replace('WRAPPERS', declarations).replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='gui-type-maps-') as directory:
    td = Path(directory)
    target = td / 'maps.c'
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
        includes += ['-I'+str(root / p) for p in ('Linux/arm/include','include','libinterp')]
        binary = td / 'maps'
        subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)]+os.environ.get('GUI_MAP_CASE','').split(),check=True)
        print('WORD'+str(bits), flush=True)
