#!/usr/bin/env python3
"""Run real directory conversion, layouts and cleanup with exhaustive allocation failure."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(text, signature):
    """Extract complete C functions without counting quoted or commented braces."""
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


heap = (root / 'libinterp/heap.c').read_text()
strings = (root / 'libinterp/string.c').read_text()
runt = (root / 'libinterp/runt.c').read_text()
parts = [function(heap, result+'\n'+name+'(') for result, name in
         [('void','freeptrs'),('void','initmem'),('void','acheck'),('void','initarray'),
          ('Heap*','heaparray'),('void','freearray')]]
parts += [function(strings, 'String*\n'+name+'(') for name in ('newstring','newrunes','c2string')]
parts += [function(runt, 'void\nretstr(')]
with tempfile.TemporaryDirectory(prefix='sys-dirread-') as directory:
    td = Path(directory)
    for platform in ('emu','os'):
        source = Path(os.environ.get('DIRREAD_SOURCE', root / platform / 'port/inferno.c')).read_text()
        guard = (root / platform / 'port/dis.c').read_text()
        bodies = [function(guard, 'int\nkprotect(')]
        bodies += [function(source, sig) for sig in ('void\nsysinit(', 'static void\nunpackdir(', 'void\nSys_dirread(')]
        target = td / 'directory.c'
        template = (root / 'libinterp/tests/sys-dirread.c').read_text()
        target.write_text(template.replace('IMPLEMENTATION','\n'.join(parts+bodies)))
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
            includes += ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary = td / 'directory'
            subprocess.run(cmd+includes+[str(target),str(root / 'lib9/rune.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)]+os.environ.get('DIRREAD_CASE','').split(),check=True)
            print(platform, 'WORD'+str(bits), flush=True)
