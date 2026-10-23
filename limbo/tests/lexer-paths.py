#!/usr/bin/env python3
"""Exercise production include resolution and source-line directives at their bounds."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('LEX_PATHS_SOURCE', root / 'limbo/lex.c')).read_text()


# Extract complete production bodies, ignoring braces inside literals and comments.
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
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = []
for signature in ('static Biobuf*\nopeninclude(', 'static int\nsourceline('):
    if signature in source:
        parts.append(function(signature))
parts += [function(signature) for signature in ('void\nincludef(', 'Fline\nfline(', 'static int\nlexcom(')]
unit = (root / 'limbo/tests/lexer-paths.c').read_text()
with tempfile.TemporaryDirectory(prefix='lexer-paths-') as directory:
    td = Path(directory)
    target = td / 'paths.c'
    target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS',
                       '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd + [str(target), '-o', str(td / 'test')], check=True)
    subprocess.run([str(td / 'test')], check=True, timeout=15)
