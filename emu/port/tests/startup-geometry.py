#!/usr/bin/env python3
"""Execute both geometry parsers against independently labelled spelling/range cases."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, signature):
    """Extract complete source functions, ignoring braces in comments and literals."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = []
for name, path in [('portable', 'emu/port/main.c'), ('android', 'emu/Android/amain.c')]:
    source = Path(os.environ.get(name.upper()+'_SOURCE', root / path)).read_text()
    functions = '\n'.join(function(source, sig) for sig in ['static int\nisnum(', 'static int\ngeom('])
    functions = re.sub(r'\bisnum\b', name+'num', functions)
    functions = re.sub(r'\bgeom\b', name, functions)
    parts.append(functions)
unit = (root / 'emu/port/tests/startup-geometry.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='startup-geometry-') as directory:
    td = Path(directory)
    target = td / 'geometry.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    binary = td / 'geometry'
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
