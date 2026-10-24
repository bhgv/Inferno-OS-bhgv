#!/usr/bin/env python3
"""Execute production string operations with bounded, guarded allocations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('STRING_SOURCE',str(root / 'libinterp/string.c'))).read_text()


# Extract production bodies so the fixture cannot substitute its own logic.
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


signatures = [
    'String*\nnewstring(',
    'String*\nnewrunes(',
    'void\ncvtup(',
    'String*\nstringdup(',
    'String*\nsplitc(',
    'String*\naddstring(',
    'OP(insc)',
    'OP(lenc)',
    'OP(cvtlc)',
    'char*\nstring2c(',
]
parts = [function(signature) for signature in signatures]
unit = (root / 'libinterp/tests/strings.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='strings-') as directory:
    td = Path(directory)
    (td / 'strings.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g'))
    selected=os.environ.get('STRING_LENGTH_CASE','')
    if selected not in ('','nil','intmin'):raise ValueError(selected)
    if selected:cmd += ['-DSTRING_LENGTH_FIRST='+('1' if selected=='intmin' else '0'),'-DSTRING_LENGTH_LAST='+('2' if selected=='intmin' else '1')]
    includes = ['Linux/arm/include', 'emu/port', 'include', 'libinterp']
    cmd += ['-I' + str(root / path) for path in includes]
    subprocess.run(cmd + [str(td / 'strings.c'), '-o', str(td / 'strings')], check=True)
    subprocess.run([str(td / 'strings')], check=True)
