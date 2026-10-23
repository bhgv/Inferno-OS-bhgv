#!/usr/bin/env python3
"""Execute file2chan construction, native descriptors and every allocation failure."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(text, signature):
    """Extract complete source functions without counting quoted/comment braces."""
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
xec = (root / 'libinterp/xec.c').read_text()
parts = [function(heap, result+'\n'+name+'(') for result, name in
         [('Type*','dtype'),('void','freetype'),('void','acheck'),('void','initmem'),
          ('void','freeptrs'),('Heap*','nheap'),('Heap*','heap'),('Heap*','heaparray'),
          ('void','initarray'),('void','freechan'),('void','freearray'),('void','freeheap'),('void','freestring')]]
parts += [function(strings, 'String*\n'+name+'(') for name in ('newstring','newrunes','c2string')]
parts += [function(strings, 'char*\nstring2c(')]
start = xec.index('typedef struct ChannelBuild ChannelBuild;')
end = xec.index('\nChannel*\nnewc(', start)
parts += [xec[start:end]]
with tempfile.TemporaryDirectory(prefix='sys-file2chan-') as directory:
    td = Path(directory)
    for platform in ('emu','os'):
        source = Path(os.environ.get('FILE2CHAN_SOURCE', root / platform / 'port/inferno.c')).read_text()
        service = Path(os.environ.get('FILE2CHAN_SERVICE', root / platform / 'port/devsrv.c')).read_text()
        guard = (root / platform / 'port/dis.c').read_text()
        bodies = [function(guard, sig) for sig in ('int\nkprotect(', 'void\nkrethrow(')]
        bodies += [function(source, sig) for sig in ('void\nsysinit(', 'void\nfreeFileIO(', 'void\nSys_file2chan(')]
        bodies += [function(service, 'static void\nsrvinit(')]
        target = td / 'file2chan.c'
        template = (root / 'libinterp/tests/sys-file2chan.c').read_text()
        start = source.index('typedef struct FD FD;')
        end = source.index('\n};', start)+3
        template = template.replace('FD_DECLARATION', source[start:end])
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
            binary = td / 'file2chan'
            subprocess.run(cmd+includes+[str(target),str(root / 'lib9/rune.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)]+os.environ.get('FILE2CHAN_CASE','').split(),check=True)
            print(platform, 'WORD'+str(bits), flush=True)
