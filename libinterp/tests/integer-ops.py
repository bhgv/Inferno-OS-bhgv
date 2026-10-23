#!/usr/bin/env python3
"""Exercise production VM modular arithmetic and valid left shifts under UBSan."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'libinterp/xec.c').read_text()


# Extract unchanged function bodies, including compact legacy OP definitions.
def function(name):
    signature = 'static WORD\nshiftcount(' if name == 'shiftcount' else 'OP(' + name + ')'
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
    raise ValueError(name)


names = ['shiftcount', 'addw', 'subw', 'mulw', 'addl', 'subl', 'mull', 'shlw', 'shll']
unit = (root / 'libinterp/tests/integer-ops.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(function(name) for name in names))

with tempfile.TemporaryDirectory(prefix='integer-ops-') as directory:
    td = Path(directory)
    (td / 'ops.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    for width in (32, 64):
        binary = td / ('ops' + str(width))
        subprocess.run(cmd + ['-DWORD_BITS=' + str(width), str(td / 'ops.c'),
                              '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
