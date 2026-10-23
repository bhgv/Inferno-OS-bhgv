#!/usr/bin/env python3
"""Compile actual tokenization, slicing and error protection with guarded VM storage."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(text, signature):
    """Extract a production function, ignoring braces in comments and literals."""
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


runtime = Path(os.environ.get('TOKENIZE_SOURCE', root/'libinterp/runt.c')).read_text()
strings = (root/'libinterp/string.c').read_text()
guard = (root/'emu/port/dis.c').read_text()
xec = (root/'libinterp/xec.c').read_text()
parts = [function(guard, 'int\nkprotect('), function(guard, 'void\nkrethrow(')]
parts += [function(strings, name) for name in ('String*\nnewstring(', 'String*\nnewrunes(', 'String*\nslicer(')]
parts += [function(xec, 'List*\ncons('), function(runtime, 'static int\ntokdelim(')]
if 'typedef struct TokenizeResult' in runtime:
    start = runtime.index('typedef struct TokenizeResult')
    end = runtime.index('/* Keep partial words', start)
    parts.append(runtime[start:end])
    parts.append(function(runtime, 'static int\nbuildtokens('))
parts.append(function(runtime, 'void\nSys_tokenize('))
unit = (root/'libinterp/tests/sys-tokenize-matrix.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='sys-tokenize-') as directory:
    td = Path(directory)
    target = td/'tokenize.c'
    target.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
    for model in ('native', 'word32'):
        override = []
        if model == 'word32':
            modeldir = td/model
            modeldir.mkdir()
            header = (root/'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            assert n == m == 1
            (modeldir/'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td/('tokenize-'+model)
        subprocess.run(command+override+includes+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+shlex.split(os.environ.get('TOKENIZE_CASE', '')), check=True)
