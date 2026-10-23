#!/usr/bin/env python3
"""Exercise production dense-case expansion and inclusive range target assignment."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('CASE_RANGE_SOURCE', root / 'limbo/com.c')).read_text()


# Extract complete helpers while retaining the production arithmetic and loops.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [function('static int\ndogoto('), function('static void\nfillrange(')]
unit = (root / 'limbo/tests/case-ranges.c').read_text()
with tempfile.TemporaryDirectory(prefix='case-ranges-') as directory:
    td = Path(directory)
    target = td / 'case.c'
    target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
    binary = td / 'case'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS',
                       '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I' + str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(cmd + [str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)] + shlex.split(os.environ.get('CASE_RANGE_CASE', '')),
                   check=True, timeout=30)
