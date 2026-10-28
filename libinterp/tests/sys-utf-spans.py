#!/usr/bin/env python3
"""Exercise complete Sys UTF frame, metadata, payload and result spans."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('SYS_UTF_SOURCE', root / 'libinterp/runt.c')).read_text()


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


signatures = ['static int\nsysutfarray(', 'static uchar*\nsysutfresult(', 'static int\nutfnleng(',
              'void\nSys_utfbytes(', 'void\nSys_byte2char(', 'void\nSys_char2byte(']
parts = [function(source, signature) for signature in signatures if signature in source]
fixture = (root / 'libinterp/tests/sys-utf.c').read_text()
unit = fixture.split('IMPLEMENTATION')[0].replace('#include <assert.h>', '#include <pthread.h>\n#include <stddef.h>')
unit += '\n'.join(parts)+'\n'+function(fixture, 'static int\nencode(')
unit += (root/'libinterp/tests/sys-utf-spans.c').read_text()
with tempfile.TemporaryDirectory(prefix='sys-utf-') as directory:
    td = Path(directory)
    target = td / 'codecs.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    if os.environ.get('SYS_UTF_FAMILY') not in (None, 'packed', 'errors', 'results', 'frames'):
        raise ValueError('unknown Sys UTF family')
    if os.environ.get('SYS_UTF_OP') not in (None, '0', '1', '2'):
        raise ValueError('unknown Sys UTF operation')
    models = os.environ.get('SYS_UTF_MODELS', 'native word32').split()
    if not models:
        raise ValueError('no scalar models selected')
    for model in models:
        if model not in ('native', 'word32'):
            raise ValueError(model)
        override = []
        if model == 'word32':
            modeldir = td / model
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;',
                               'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or m != 1:
                raise ValueError("WORD32 header mismatch")
            (modeldir / 'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td / ('codecs-'+model)
        subprocess.run(cmd+override+includes+[str(target),str(root / 'lib9/rune.c'),'-pthread','-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
