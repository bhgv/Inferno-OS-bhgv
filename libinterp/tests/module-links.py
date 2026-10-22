#!/usr/bin/env python3
"""Check real export lookup and module linking with owned Modlink allocations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Extract actual functions while ignoring braces in comments and literals.
def function(text, signature):
    start = text.index(signature); brace = text.index('{', start); depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


link = Path(os.environ.get('MODULE_LINK_SOURCE', root/'libinterp/link.c')).read_text()
parts = [function(link, sig) for sig in ('static int\nlinkm(', 'Modlink*\nmklinkmod(', 'Modlink*\nlinkmod(')]
unit = (root/'libinterp/tests/module-links.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='module-links-') as directory:
    td = Path(directory); target = td/'test.c'; target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
    subprocess.run(cmd+[str(target), '-o', str(td/'test')], check=True)
    subprocess.run([str(td/'test')], check=True)
