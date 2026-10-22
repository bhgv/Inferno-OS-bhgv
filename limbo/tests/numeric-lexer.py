#!/usr/bin/env python3
"""Run production numeric scanning and conversion with bounded input fixtures."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'limbo/lex.c').read_text()


# Ignore comments and strings while locating a complete production function.
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


constants = source[source.index('enum\n'):source.index('typedef\tstruct Keywd')]
parts = [function(signature) for signature in ['int\ncmap(', 'Long\nstrtoi(',
         'static int\ndigit(', 'double\nstrtodb(', 'int\nlexnum(']]
unit = (root / 'limbo/tests/numeric-lexer.c').read_text()
unit = unit.replace('CONSTANTS', constants)
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='numeric-lexer-') as directory:
    td = Path(directory)
    (td / 'lexer.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    subprocess.run(cmd + [str(td / 'lexer.c'), '-lm', '-o', str(td / 'lexer')], check=True)
    subprocess.run([str(td / 'lexer')], check=True)
