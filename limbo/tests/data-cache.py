#!/usr/bin/env python3
"""Exercise production Dis data writers, cache growth and canonical output."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('DIS_DATA_SOURCE', root / 'limbo/dis.c')).read_text()


# Extract complete function bodies without interpreting braces in comments.
def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


names = [('void', 'discon'), ('void', 'disdata')]
names += [('static void', n) for n in ('disbyte', 'disbytes', 'disint', 'disbig',
                                      'disreal', 'disstring', 'disflush')]
parts = [function(source, result + '\n' + name + '(') for result, name in names]
parts.insert(0, function((root / 'limbo/com.c').read_text(), 'Long\ncaseend('))
parts.insert(0, function((root / 'limbo/dtocanon.c').read_text(), 'void\ndtocanon('))
unit = (root / 'limbo/tests/data-cache.c').read_text()
with tempfile.TemporaryDirectory(prefix='dis-data-cache-') as directory:
    td = Path(directory)
    target = td / 'cache.c'
    target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS',
                       '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    binary = td / 'cache'
    subprocess.run(cmd + ['-I' + str(root / 'include'), str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)] + shlex.split(os.environ.get('DIS_DATA_CASE', '')),
                   check=True, timeout=30)
    if os.environ.get('DIS_DATA_CASE', '') in ('', 'interface'):
        prefix = source[source.index('static\tvoid\tdisbig'):source.index('/* Reject values')]
        bodies = parts + [function(source, 'void\ndisword('),
                          function(source, 'static void\ndisdatum('),
                          function(source, 'void\ndisvar(')]
        realunit = (root / 'limbo/tests/data-cache-interface.c').read_text()
        target.write_text(realunit.replace('IMPLEMENTATION', prefix+'\n'+'\n'.join(bodies)))
        includes = ['-I' + str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
        subprocess.run(cmd + includes + [str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=30)
