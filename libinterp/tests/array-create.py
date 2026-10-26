#!/usr/bin/env python3
"""Exercise production NEWA/NEWAZ maps and allocation under both WORD models."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract complete production functions without interpreting comments or literal braces.
def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return text[start:brace+token.end()]
    raise ValueError(signature)


xec = Path(os.environ.get('ARRAY_XEC_SOURCE', ROOT/'libinterp/xec.c')).read_text()
heap = Path(os.environ.get('ARRAY_HEAP_SOURCE', ROOT/'libinterp/heap.c')).read_text()
parts = [function(heap, sig) for sig in ('void\nfreeptrs(', 'void\ninitmem(', 'void\nfreearray(', 'void\nacheck(', 'Heap*\nnheap(', 'void\ninitarray(', 'Heap*\nheaparray(')]
parts += [function(xec, 'static Type*\nmoduletype(')]
if 'static void\nnewarray(' in xec:parts += [function(xec, 'static void\nnewarray(')]
parts += [function(xec, sig) for sig in ('OP(newa)', 'OP(newaz)')]
unit = (ROOT/'libinterp/tests/array-create.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='array-create-') as temporary:
    td = Path(temporary)
    target = td/'arrays.c'; target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (4, 8):
        model = td/str(wb); model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb == 4:
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, u = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or u != 1: raise RuntimeError('WORD32 header mismatch')
        (model/'interp.h').write_text(header)
        binary = td/('arrays'+str(wb))
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        subprocess.run(command+includes+[str(target), '-o', str(binary), '-lm'], check=True)
        subprocess.run([str(binary)], check=True)
