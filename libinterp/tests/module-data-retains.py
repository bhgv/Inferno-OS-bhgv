#!/usr/bin/env python3
"""Check module-data descriptor preflight and failure ownership."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Extract production bodies without treating comments or literals as braces.
def function(text, signature):
    start = text.index(signature); brace = text.index('{', start); depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


heap = Path(os.environ.get('HEAP_CLONE_SOURCE', root/'libinterp/heap.c')).read_text()
dis = (root/'emu/port/dis.c').read_text()
parts = [function(dis, signature) for signature in ('int\nkprotect(', 'void\nkrethrow(')]
parts += [function(heap, result+'\n'+name+'(') for result, name in (
    ('void', 'freeptrs'), ('void', 'initmem'), ('void', 'incmem'),
    ('void', 'freearray'), ('void', 'freeheap'), ('void', 'freemodlink'), ('void', 'acheck'),
    ('Heap*', 'nheap'), ('Heap*', 'heapz'), ('void', 'initarray'), ('Heap*', 'heaparray'))]
if 'typedef struct ArrayCopy' in heap:
    parts += [heap[heap.index('typedef struct ArrayCopy'):heap.index('void*\narraycpy(')]]
parts += [function(heap, 'void*\narraycpy(')]
mp = Path(os.environ.get('HEAP_NEWMP_SOURCE', root/'libinterp/heap.c')).read_text()
parts += [function(mp, 'void\nnewmp(')]
link = Path(os.environ.get('LINK_INSTANCE_SOURCE', root/'libinterp/link.c')).read_text()
parts += [function(link, signature) for signature in ('static int\nlinkm(', 'Modlink*\nmklinkmod(')]
if 'typedef struct LinkModule' in link:
    parts += [link[link.index('typedef struct LinkModule'):link.index('Modlink*\nlinkmod(')]]
parts += [function(link, 'Modlink*\nlinkmod(')]
unit = (root/'libinterp/tests/heap-clones.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts)).replace('main(void)','clonemain(void)')
unit += (root/'libinterp/tests/module-data-retains.c').read_text()
with tempfile.TemporaryDirectory(prefix='heap-clones-') as directory:
    td = Path(directory); target = td/'test.c'; target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
    for model in ('native', 'word32'):
        override = []
        if model == 'word32':
            modeldir = td/model; modeldir.mkdir()
            header = (root/'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or m != 1:raise RuntimeError('WORD32 header mismatch')
            (modeldir/'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td/('test-'+model)
        subprocess.run(cmd+override+includes+[str(target), '-o', str(binary)], check=True)
        print(model, flush=True)
        subprocess.run([str(binary)], check=True, timeout=60)
