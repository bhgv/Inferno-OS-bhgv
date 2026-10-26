#!/usr/bin/env python3
"""Compare actual string stores and published AMD64 leaves using real heap declarations."""
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


signatures = ['String*\nnewstring(', 'String*\nnewrunes(', 'void\ncvtup(',
              'String*\nstringdup(', 'String*\naddstring(', 'String*\nslicer(', 'String*\nsplitc(', 'OP(insc)', 'OP(indc)', 'OP(cvtca)',
              'OP(cvtac)', 'OP(cvtwc)', 'char*\nstring2c(', 'String*\nc2string(']
parts = [function(source, signature) for signature in signatures]
parts += [function((root / 'libinterp/runt.c').read_text(), 'Array*\nmem2array(')]
unit = (root / 'libinterp/tests/string-codecs.c').read_text()
unit = re.sub(r'^#include "[^"]+"\n', '', unit, flags=re.M)
unit = '#include "comp-amd64.c"\n#include "tests/leaf-plain-stubs.h"\n#include <fenv.h>\n'+unit.replace('IMPLEMENTATION', '\n'.join(parts)).replace('main(void)', 'codecmain(void)')
unit += (root/'libinterp/tests/string-store-native.c').read_text()
with tempfile.TemporaryDirectory(prefix='string-store-native-') as directory:
    td = Path(directory)
    target = td / 'codecs.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    selected=os.environ.get('STRING_INDEX_CASE','')
    selections={'nil':1,'min':2,'store-nil':3,'store-min':4,'alignment':5,'stores':6}
    if selected and selected not in selections:raise ValueError(selected)
    if selected:cmd += ['-DSTRING_INDEX_CASE='+str(selections[selected])]
    includes = ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    for model in ('native','word32'):
        override = []
        if model == 'word32':
            modeldir = td / model
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;',
                               'typedef unsigned int UWORD;', header, count=1)
            assert n == m == 1
            (modeldir / 'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td / ('codecs-'+model)
        subprocess.run(cmd+override+includes+[str(target),str(root / 'lib9/rune.c'),'-o',str(binary),'-lm'],check=True)
        subprocess.run([str(binary)],check=True)
