#!/usr/bin/env python3
"""Exercise production tokenize/list/string builders under allocation failure."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('TOKENIZE_SOURCE', root / 'libinterp/runt.c')).read_text()


def function(text, signature):
    """Extract complete functions without treating quoted/comment braces as code."""
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


parts = [function(source, 'static int\ntokdelim(')]
if 'struct TokenizeResult\n' in source:
    start = source.index('typedef struct TokenizeResult TokenizeResult;')
    end = source.index('\n/* Keep partial words', start)
    parts += [source[start:end], function(source, 'static int\nbuildtokens(')]
parts += [function(source, 'void\nSys_tokenize(')]
strings = (root / 'libinterp/string.c').read_text()
parts = [function(strings, 'String*\n'+name+'(') for name in ('newstring','newrunes','slicer')] + parts
parts.insert(0, function((root / 'libinterp/xec.c').read_text(), 'List*\ncons('))
parts.insert(0, function((root / 'libinterp/heap.c').read_text(), 'void\nfreeptrs('))
template = (root / 'libinterp/tests/sys-tokenize.c').read_text()
with tempfile.TemporaryDirectory(prefix='sys-tokenize-') as directory:
    td = Path(directory)
    for runtime in ('emu/port/dis.c','os/port/dis.c'):
        guard = (root / runtime).read_text()
        body = '\n'.join(function(guard, signature) for signature in ('int\nkprotect(', 'void\nkrethrow('))
        target = td / 'tokens.c'
        target.write_text(template.replace('IMPLEMENTATION', body+'\n'+'\n'.join(parts)))
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
            binary = td / 'tokens'
            cmd = shlex.split(os.environ.get('CC','cc'))
            cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            subprocess.run(cmd+includes+[str(target),'-o',str(binary)],check=True)
            subprocess.run([str(binary)]+os.environ.get('TOKENIZE_CASE','').split(),check=True)
            print('Runtime:', runtime, 'WORD'+str(bits), flush=True)
