#!/usr/bin/env python3
"""Check 18 production C transfers before malformed slots can affect memory or FP state."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('TRANSFER_SLOT_SOURCE', ROOT/'libinterp/xec.c')).read_text()


# Extract complete production bodies without mistaking comments or strings for braces.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


names = ('movb','movw','movl','movf','cvtbw','cvtwb','cvtws','cvtsw',
         'cvtwl','cvtlw','lea','negf','cvtrf','cvtfr','cvtwf','cvtlf','cvtfl','cvtfw')
parts = [function('OP('+name+')') for name in names]
unit = (ROOT/'libinterp/tests/transfer-slot-native.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='transfer-slot-native-') as directory:
    td = Path(directory)
    target = td/'test.c'
    target.write_text(unit)
    for wb in (4, 8):
        model = td/str(wb)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb == 4:
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, u = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if n != 1 or u != 1:
                raise RuntimeError('WORD32 substitutions did not match')
        (model/'interp.h').write_text(header)
        includes = [model]+[ROOT/p for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
        binary = model/'test'
        subprocess.run(cmd+['-I'+str(p) for p in includes]+[str(target), '-o', str(binary), '-lm'], check=True)
        subprocess.run([str(binary)], check=True)
