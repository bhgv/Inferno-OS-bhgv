#!/usr/bin/env python3
"""Execute production boolean folding and compiler/VM power implementations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Match balanced bodies while ignoring braces in strings and comments.
def function(path, signature):
    source = (root / path).read_text()
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


folder = function('limbo/nodes.c', 'Node*\nfoldvc(')
ops = list(dict.fromkeys(re.findall(r'case (O\w+):', folder)))
ops += ['Onot', 'Oname', 'Olt', 'Oconst']
parts = [function('limbo/nodes.c', 'static vlong\nipow('),
         function('limbo/nodes.c', 'double\nrpow('), folder]
parts += [function('libinterp/xec.c', 'OP(' + name + ')')
          for name in ['iexpw', 'iexpl', 'iexpf']]
unit = (root / 'limbo/tests/fold-effects.c').read_text()
unit = unit.replace('OPS', 'enum {' + ','.join(ops) + '};')
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='fold-effects-') as directory:
    td = Path(directory)
    (td / 'effects.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    for width in (32, 64):
        binary = td / ('effects' + str(width))
        subprocess.run(cmd + ['-DWORD_BITS=' + str(width), str(td / 'effects.c'),
                              '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=15)
