#!/usr/bin/env python3
"""Exercise Sys UTF builtins using real frames and independent Unicode encoding."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
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
unit = (root / 'libinterp/tests/sys-utf.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='sys-utf-') as directory:
    td = Path(directory)
    target = td / 'codecs.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
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
            if n != 1 or m != 1:
                raise RuntimeError('WORD model declaration was not replaced exactly once')
            (modeldir / 'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td / ('codecs-'+model)
        subprocess.run(cmd+override+includes+[str(target),str(root / 'lib9/rune.c'),'-o',str(binary)],check=True)
        subprocess.run(runner+[str(binary)]+shlex.split(os.environ.get('SYS_UTF_CASE','')),check=True)
        # Corrupt one production result; the independent oracle must still fail under NDEBUG.
        controlled = td/'wrong-count.c'
        broken, changed = re.subn(r'result = 1;', 'result = 2;', unit, count=1)
        if changed != 1:
            raise RuntimeError('UTF result control was not applied')
        controlled.write_text(broken)
        negative = td/('wrong-count-'+model)
        subprocess.run(cmd+override+includes+[str(controlled),str(root/'lib9/rune.c'),'-o',str(negative)],check=True)
        result = subprocess.run(runner+[str(negative), 'alignment'], capture_output=True, text=True)
        if result.returncode == 0 or 'UTF predicate failed:' not in result.stderr:
            raise RuntimeError('Wrong UTF count escaped the active semantic oracle: '+result.stderr)
        print('PASS UTF wrong-count control:', model, flush=True)
