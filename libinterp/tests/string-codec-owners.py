#!/usr/bin/env python3
"""Exercise borrowed packed codec sources and validate destination ownership."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('STRING_SOURCE', root / 'libinterp/string.c')).read_text()


# Ignore braces in comments and literals when extracting production functions.
def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


signatures = ['String*\nnewstring(', 'String*\nnewrunes(', 'char*\nstring2c(',
              'String*\nc2string(', 'OP(cvtca)', 'OP(cvtac)']
parts = [function(source, signature) for signature in signatures]
parts += [function((root/'libinterp/runt.c').read_text(), 'Array*\nmem2array(')]
prefix = (root / 'libinterp/tests/string-view-native.c').read_text().split('/* Catch VM errors')[0]
prefix = prefix.replace('IMPLEMENTATION', '#define OP(name) void name(void) // Extract real opcode bodies.\n'+'\n'.join(parts))
prefix = 'static void codecdiag(int);\n'+prefix.replace('fprintf(stderr, \"view check line %d\\n\", __LINE__)', 'codecdiag(__LINE__)')
unit = prefix + (root / 'libinterp/tests/string-codec-owners.c').read_text()
with tempfile.TemporaryDirectory(prefix='string-codec-owners-') as directory:
    td = Path(directory)
    target = td / 'codecs.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    models = os.environ.get('STRING_VIEW_MODELS', 'native word32').split()
    if not models or any(model not in ('native', 'word32') for model in models):
        raise ValueError('unknown scalar model')
    for model in models:
        override = []
        if model == 'word32':
            modeldir = td / model
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;',
                               'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or m != 1:
                raise ValueError("scalar model header mismatch")
            (modeldir / 'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td / ('codecs-'+model)
        subprocess.run(cmd+override+includes+[str(target),str(root / 'lib9/rune.c'),'-pthread','-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
