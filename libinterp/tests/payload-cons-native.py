#!/usr/bin/env python3
"""Compare raw and pointer list constructors with actual AMD64 allocator-call handlers."""
import os
import platform
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


xec = Path(os.environ.get('CONS_XEC_SOURCE', ROOT/'libinterp/xec.c')).read_text()
heap = Path(os.environ.get('CONS_HEAP_SOURCE', ROOT/'libinterp/heap.c')).read_text()
parts = [function(heap, 'Heap*\nnheap(')]
parts += [function(xec, sig) for sig in ('List*\ncons(', 'OP(consm)', 'OP(consp)')]
unit = (ROOT/'libinterp/tests/payload-cons-native.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
backend = os.environ.get('NATIVE64_ARCH') or {'x86_64': 'amd64', 'amd64': 'amd64', 'aarch64': 'arm64', 'arm64': 'arm64', 'riscv64': 'riscv64'}.get(platform.machine().lower())
if backend not in ('amd64', 'arm64', 'riscv64'):
    raise ValueError('unsupported native cons target')
unit = unit.replace('comp-amd64.c', 'comp-'+backend+'.c')
runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
with tempfile.TemporaryDirectory(prefix='payload-cons-native-') as temporary:
    td = Path(temporary)
    target = td/'cons.c'; target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))+shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (4, 8):
        model = td/str(wb); model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb == 4:
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, u = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or u != 1: raise RuntimeError('WORD32 header mismatch')
        (model/'interp.h').write_text(header)
        binary = td/('cons'+str(wb))
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        subprocess.run(command+includes+[str(target), '-o', str(binary), '-lm'], check=True)
        subprocess.run(runner+[str(binary)], check=True)
