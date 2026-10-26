#!/usr/bin/env python3
"""Check actual Type retention and array constructors with the bounded pool fixture."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract production bodies while ignoring braces in comments and literals.
def function(path, signature):
    selected = ROOT/path
    if path == 'libinterp/heap.c':
        selected = Path(os.environ.get('HEAP_TYPE_SOURCE', selected))
    elif path == 'libinterp/link.c':
        selected = Path(os.environ.get('LINK_TYPE_SOURCE', selected))
    elif path == 'libinterp/xec.c':
        selected = Path(os.environ.get('HEAP_XEC_SOURCE', selected))
    source = selected.read_text()
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [function('emu/port/dis.c', signature)
         for signature in ('int\nkprotect(', 'void\nkrethrow(')]
parts += [function('libinterp/heap.c', result+'\n'+name+'(') for name, result in (
    ('incmem', 'void'), ('freeptrs', 'void'), ('acheck', 'void'), ('nheap', 'Heap*'),
    ('heap', 'Heap*'), ('initmem', 'void'), ('heapz', 'Heap*'),
    ('initarray', 'void'), ('heaparray', 'Heap*'))]
heap = Path(os.environ.get('HEAP_TYPE_SOURCE', ROOT/'libinterp/heap.c')).read_text()
parts += [heap[heap.index('typedef struct ArrayCopy'):heap.index('void*\narraycpy(')]]
parts += [function('libinterp/heap.c', signature)
          for signature in ('void*\narraycpy(', 'void\nnewmp(')]
parts += [function('libinterp/link.c', 'Modlink*\nmklinkmod('),
          function('libinterp/xec.c', 'List*\ncons('),
          function('libinterp/xec.c', 'static Type*\nmoduletype('),
          function('libinterp/xec.c', 'static uchar*\narrayspan(')]
parts += [function('libinterp/xec.c', 'static void\nnewarray(')]
parts += [function('libinterp/xec.c', 'OP('+name+')')
          for name in ('newa', 'newaz', 'slicea', 'slicela')]
unit = (ROOT/'libinterp/tests/heap-bounds.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))
unit = unit.replace('main(int argc, char **argv)', 'boundsmain(int argc, char **argv)')
unit = unit.replace('main(void)', 'boundsmain(void)')
unit += (ROOT/'libinterp/tests/type-retains.c').read_text()
with tempfile.TemporaryDirectory(prefix='type-retains-') as directory:
    td = Path(directory)
    source = td/'retains.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64, 32):
        model = td/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header, count = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, unsigned = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if count != 1 or unsigned != 1:
                raise RuntimeError('WORD32 fixture header mismatch')
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/path) for path in
                    ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        binary = td/('retains'+str(bits))
        subprocess.run(command+includes+[str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
