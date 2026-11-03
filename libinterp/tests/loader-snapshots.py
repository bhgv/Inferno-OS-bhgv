#!/usr/bin/env python3
"""Exercise real Loader snapshot functions, heap bounds and partial ownership."""
import os
from pathlib import Path
import re
import runpy
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
parts += [loader[loader.index('static WORD\nloaderpc'):loader.index('typedef struct LoaderModule')]]
support = runpy.run_path(str(root/'utils/tests/native64-builtins.py'))
harness = (root/'libinterp/tests/loader-snapshots.c').read_text()
harness = harness.replace('#include <assert.h>', '#define snapshotcheck(test) do { if(!(test)) { fprintf(stderr,"Loader snapshot predicate failed at %d\\n",__LINE__); abort(); } } while(0) // Keep all optimized predicates active.')
harness = harness.replace('assert(', 'snapshotcheck(')
unit = support['portfixture'](harness.replace('IMPLEMENTATION', '\n'.join(parts)))
runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
arch = os.environ.get('NATIVE64_ARCH', 'amd64')
with tempfile.TemporaryDirectory(prefix='loader-snapshots-') as directory:
    td = Path(directory); target = td/'test.c'; target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('NATIVE64_CFLAGS', os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all')))
    includes = ['-I'+str(root/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    for model in ('native', 'word32'):
        override = []
        if model == 'word32':
            modeldir = td/model; modeldir.mkdir()
            header = (root/'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or m != 1:
                raise RuntimeError('Loader snapshot word-model override drift')
            (modeldir/'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td/model
        if model == 'word32':
            binary = td/'test-word32'
        subprocess.run(cmd+override+includes+[str(target), '-o', str(binary)], check=True)
        print(model, flush=True)
        subprocess.run(runner+[str(binary)], check=True, timeout=30)
