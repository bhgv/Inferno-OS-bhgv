#!/usr/bin/env python3
"""Exercise production cast folding with bounded AST and dependency models."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'limbo/nodes.c').read_text()


# Extract a complete function without counting braces inside comments or literals.
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


parts = [function('static Long\nfoldint32(')] if 'static Long\nfoldint32(' in source else []
if 'static int\nfoldrealint(' in source:
    parts.append(function('static int\nfoldrealint('))
parts.append(function('double\nfix2real('))
parts += [function('Node*\nfoldcast('), function('Node*\nfoldcasti(')]
unit = (root / 'limbo/tests/fold-casts.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='fold-casts-') as directory:
    td = Path(directory)
    (td / 'casts.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    subprocess.run(cmd + [str(td / 'casts.c'), '-lm', '-o', str(td / 'casts')], check=True)
    subprocess.run([str(td / 'casts')], check=True)
