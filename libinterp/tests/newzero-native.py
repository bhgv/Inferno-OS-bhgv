#!/usr/bin/env python3
"""Compare production NEWZ/MNEWZ C reuse and mapped allocation with real x64 RX code."""
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


xec = Path(os.environ.get('ZERO_XEC_SOURCE', ROOT/'libinterp/xec.c')).read_text()
heap = Path(os.environ.get('ZERO_HEAP_SOURCE', ROOT/'libinterp/heap.c')).read_text()
parts = [function(heap, sig) for sig in ('void\nfreeptrs(', 'void\ninitmem(', 'void\nfreeheap(', 'Heap*\nheapz(', 'Heap*\nheap(')]
parts += [function(xec, sig) for sig in ('static Type*\nmoduletype(', 'static void\nnewrecord(', 'OP(new)', 'OP(newz)', 'OP(mnewz)')]
unit = (ROOT/'libinterp/tests/newzero-native.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='newzero-native-') as temporary:
    td = Path(temporary)
    target = td/'zero.c'; target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (4, 8):
        model = td/str(wb); model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb == 4:
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, u = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or u != 1: raise RuntimeError('WORD32 header mismatch')
        (model/'interp.h').write_text(header)
        binary = td/('zero'+str(wb))
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        subprocess.run(command+includes+[str(target), '-o', str(binary), '-lm'], check=True)
        subprocess.run([str(binary)], check=True)
