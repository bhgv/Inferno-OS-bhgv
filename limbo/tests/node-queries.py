#!/usr/bin/env python3
"""Exercise production AST queries with structural oracles and bounded C stacks."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get('NODE_QUERY_SOURCE', ROOT/'limbo/nodes.c')).read_text()


# Extract real functions, ignoring braces inside comments and literals.
def function(name):
    start = SOURCE.index('int\n'+name+'(')
    brace = SOURCE.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', SOURCE[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return SOURCE[start:brace+token.end()]
    raise RuntimeError(name)


with tempfile.TemporaryDirectory(prefix='limbo-node-queries-') as temporary:
    work = Path(temporary)
    source = work/'queries.c'
    binary = work/'queries'
    source.write_text((ROOT/'limbo/tests/node-queries.c').read_text().replace(
        'IMPLEMENTATION', '\n\n\n'.join(function(n) for n in ('hasside', 'hascall', 'hasasgns', 'nodes', 'sametree'))))
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(command+[str(source), '-o', str(binary)], check=True)
    for family in ([os.environ['NODE_QUERY_CASE']] if os.environ.get('NODE_QUERY_CASE') else ['small', 'deep']):
        subprocess.run([str(binary), family], check=True)
