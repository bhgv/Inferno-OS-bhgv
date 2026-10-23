#!/usr/bin/env python3
"""Check production compiler name builders, failure diagnostics and list allocation."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]

# Match complete functions while ignoring braces inside strings and comments.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return source[start:brace+token.end()]
    raise ValueError(signature)

parts = []
for filename, signatures in [('typecheck.c', ['static char*\nexname(']),
                             ('types.c', ['static char*\nuname(']),
                             ('optim.c', ['static void\nwarning(', 'static void\naddlist(']),
                             ('lex.c', ['void*\nallocmem('])]:
    source = Path(os.environ.get('NAMES_'+filename.split('.')[0].upper(), root/'limbo'/filename)).read_text()
    parts += [function(source, signature) for signature in signatures]
unit = (root/'limbo/tests/compiler-names.c').read_text()
bodies = []
with tempfile.TemporaryDirectory(prefix='compiler-names-') as directory:
    for filename in ('limbo.y', 'y.tab.c'):
        source = Path(os.environ.get('NAMES_DRIVER', root/'limbo'/filename)).read_text()
        body = function(source, 'static char*\nmkfileext(')
        bodies.append(body)
        target = Path(directory)/'test.c'
        target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts+[body])))
        binary = Path(directory)/'test'
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    assert bodies[0] == bodies[1], 'grammar and generated output-name helper differ'
