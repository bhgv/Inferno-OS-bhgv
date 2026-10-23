#!/usr/bin/env python3
"""Run production native-stub generators and compile their emitted Runtab rows."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('NATIVE_STUB_SOURCE', root / 'limbo/stubs.c')).read_text()


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
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [function('static char*\nlowercase('), function('void\nmodcode(')]
if 'enum { NativeMapBytes' in source:
    start = source.index('enum { NativeMapBytes')
    parts.append(source[start:source.index(';', start)+1])
parts.append(function('void\nmodtab('))
with tempfile.TemporaryDirectory(prefix='native-stubs-') as directory:
    directory = Path(directory)
    target = directory / 'test.c'
    unit = (root / 'limbo/tests/native-stubs.c').read_text()
    target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
    binary = directory / 'test'
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(directory)], check=True)
    tables = sorted(directory.glob('table-*.c'))
    assert len(tables) == 10
    for table in tables:
        subprocess.run(shlex.split(os.environ.get('CC', 'cc'))+['-std=c99', '-pedantic-errors', '-Werror', '-fsyntax-only', str(table)], check=True)
    print('native stubs: 10 emitted tables pass strict C99 syntax')
