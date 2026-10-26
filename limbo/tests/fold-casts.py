#!/usr/bin/env python3
"""Exercise production cast folding with bounded AST and dependency models."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('NODES_SOURCE',root / 'limbo/nodes.c')).read_text()


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
if 'static int\nfoldrealtofixed(' in source:
    parts.append(function('static int\nfoldrealtofixed('))
if 'static int\nfoldfixedcast(' in source:
    parts.append(function('static int\nfoldfixedcast('))
for signature in ('Long\nreal2fix(', 'Long\nfix2fix('):
    if signature in source:
        parts.append(function(signature))
if 'static int\nfoldstringint(' in source:
    parts.append(function('static int\nfoldstringint('))
parts.append(function('double\nfix2real('))
parts += [function('Node*\nfoldcast('), function('Node*\nfoldcasti(')]
unit = (root / 'limbo/tests/fold-casts.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
types = (root / 'limbo/types.c').read_text()
a = types.index('#define I(x)')
b = types.index('\nint\nispoly(', a)
unit = unit.replace('SELECTBODY', types[a:b])
runtime = (root / 'libinterp/xec.c').read_text()
a = runtime.index('OP(cvtfx)')
b = runtime.index('OP(cvtxf)', a)
vm = runtime[a:b]
a = runtime.index('static WORD\nfixedword(')
b = runtime.index('/*\nOP(cvtxx)\n', a)
unit = unit.replace('VMBODY', runtime[a:b]+'\n'+vm)
with tempfile.TemporaryDirectory(prefix='fold-casts-') as directory:
    td = Path(directory)
    (td / 'casts.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    subprocess.run(cmd + ['-I'+str(root/'include'),str(td / 'casts.c'), '-lm', '-o', str(td / 'casts')], check=True)
    subprocess.run([str(td / 'casts')], check=True)
