#!/usr/bin/env python3
"""Exercise moving module type tables and ten real VM consumers."""
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


xec = Path(os.environ.get('MODULE_TYPES_XEC_SOURCE', root/'libinterp/xec.c')).read_text()
loader = (root/'libinterp/loader.c').read_text()
heap = (root/'libinterp/heap.c').read_text()
parts = [function((root/'emu/port/dis.c').read_text(), 'int\nkprotect(')]
parts += [function(heap, result+'\n'+name+'(') for result, name in (
    ('Type*', 'dtype'), ('void', 'freetype'), ('void', 'acheck'),
    ('Heap*', 'nheap'), ('Heap*', 'heap'), ('Heap*', 'heapz'),
    ('Heap*', 'heaparray'), ('void', 'initarray'), ('void', 'initmem'), ('void', 'incmem'), ('void', 'freeptrs'))]
parts += [function(loader, 'static void\nloaderword('),
          function(loader, 'void\nLoader_tnew(')]
if 'static Type*\nmoduletype(' in xec:
    parts += [function(xec, 'static Type*\nmoduletype(')]
else:
    # Let the independent direct lookup case compile against the previous source.
    parts += ['static Type *moduletype(Modlink *ml, WORD index) { return ml->type[index]; }']
parts += [function(xec, 'static void\nnewrecord('), function(xec, 'static void\nnewarray(')]
parts += [function(xec, signature) for signature in ('static int\nframefits(', 'static int\nmoduleentry(', 'List*\ncons(')]
parts += [function(xec, 'OP('+name+')') for name in
          ('new', 'newz', 'mnewz', 'newa', 'newaz', 'newcmp', 'consmp', 'movmp', 'frame', 'mcall')]
unit = (root/'libinterp/tests/module-types.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='module-types-') as directory:
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
            if n != 1 or m != 1:
                raise RuntimeError("WORD32 fixture header mismatch")
            (modeldir/'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td/model
        if model == 'word32':
            binary = td/'test-word32'
        subprocess.run(cmd+override+includes+[str(target), '-o', str(binary)], check=True)
        print(model, flush=True)
        subprocess.run([str(binary)]+(['calls'] if os.environ.get('MODULE_CASES') == 'calls' else []), check=True, timeout=30)
        if os.environ.get('MODULE_CASES') != 'calls':subprocess.run([str(binary),'calls'],check=True,timeout=30)
