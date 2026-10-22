#!/usr/bin/env python3
"""Exercise real Loader snapshot functions, heap bounds and partial ownership."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Ignore braces in comments and string literals when extracting real functions.
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


loader = Path(os.environ.get('LOADER_SNAPSHOT_SOURCE', root/'libinterp/loader.c')).read_text()
heap = (root/'libinterp/heap.c').read_text()
parts = [function((root/'emu/port/dis.c').read_text(), 'int\nkprotect(')]
parts += [function(heap, result+'\n'+name+'(') for result, name in (
    ('void', 'freeptrs'), ('void', 'initmem'), ('void', 'acheck'),
    ('Heap*', 'nheap'), ('Heap*', 'heaparray'), ('void', 'initarray'))]
parts += [function((root/'libinterp/runt.c').read_text(), 'Array*\nmem2array(')]
parts += [loader[loader.index('static void\nbrunpatch'):loader.index('typedef struct LoaderModule')]]
unit = (root/'libinterp/tests/loader-snapshots.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='loader-snapshots-') as directory:
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
            assert n == m == 1
            (modeldir/'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td/model
        if model == 'word32':
            binary = td/'test-word32'
        subprocess.run(cmd+override+includes+[str(target), '-o', str(binary)], check=True)
        print(model, flush=True)
        subprocess.run([str(binary)], check=True, timeout=30)
