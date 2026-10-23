#!/usr/bin/env python3
"""Run production string lexer and interning under signed/unsigned char UBSan."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('LEX_STRINGS_SOURCE', root / 'limbo/lex.c')).read_text()


# Locate whole production functions, ignoring braces inside literals and comments.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace + token.end()]
    raise ValueError(signature)


parts = [function(s) for s in ('Sym*\nenter(', 'Sym*\nenterstring(',
         'Sym*\nstringcat(', 'int\nlexid(', 'void\nlexstring(', 'char*\nsecpy(', 'void\nfatal(')]
unit = (root / 'limbo/tests/lexer-strings.c').read_text()
with tempfile.TemporaryDirectory(prefix='lexer-strings-') as directory:
    td = Path(directory)
    target = td / 'strings.c'
    target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
    for mode in ('-fsigned-char', '-funsigned-char'):
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS',
                           '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd + [mode, str(target), '-o', str(td / 'test')], check=True)
        subprocess.run([str(td / 'test')], check=True, timeout=30)
