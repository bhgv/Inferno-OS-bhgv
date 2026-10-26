#!/usr/bin/env python3
"""Exercise production scalar list/length checks with reduced address arithmetic."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Retain complete production bodies while ignoring braces in comments and literals.
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


xec = Path(os.environ.get('VIEW_XEC_SOURCE', ROOT/'libinterp/xec.c')).read_text()
strings = Path(os.environ.get('VIEW_STRING_SOURCE', ROOT/'libinterp/string.c')).read_text()
parts = [function(xec, 'static List*\nlisthead(')]
parts += [function(xec, 'OP('+op+')') for op in ('headb', 'headw', 'headl', 'headf', 'lenl')]
parts += [function(strings, 'OP(lenc)')]
# Narrow only the guard arithmetic; dereferences and fixture owners stay native.
parts = [part.replace('(uintptr)l > (uintptr)-1', '(ViewAddress)(uintptr)l > (ViewAddress)-1')
         .replace('(uintptr)a > (uintptr)-1', '(ViewAddress)(uintptr)a > (ViewAddress)-1')
         .replace('(uintptr)ss > (uintptr)-1', '(ViewAddress)(uintptr)ss > (ViewAddress)-1') for part in parts]
fixture = (ROOT/'libinterp/tests/scalar-view-spans-c.c').read_text()
fixture = fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='scalar-view-c-') as temporary:
    td = Path(temporary)
    target = td/'view.c'
    target.write_text(fixture)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for wb in (4, 8):
        model = td/str(wb)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb == 4:
            header, count = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, ucount = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            if count != 1 or ucount != 1:
                raise RuntimeError('WORD32 header mismatch')
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        binary = td/('view'+str(wb))
        if not os.environ.get('VIEW_NATIVE_ONLY'):
            target.write_text(fixture)
            subprocess.run(command+includes+[str(target), '-o', str(binary), '-lm'], check=True)
            subprocess.run([str(binary)], check=True)
        native = fixture.replace('typedef unsigned short ViewAddress;', 'typedef uintptr ViewAddress;').replace('main(void)', 'reducedmain(void)')
        native += (ROOT/'libinterp/tests/scalar-view-spans-native.c').read_text()
        target.write_text(native)
        subprocess.run(command+includes+[str(target), '-o', str(binary), '-lm'], check=True)
        subprocess.run([str(binary)], check=True)
