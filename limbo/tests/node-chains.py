#!/usr/bin/env python3
"""Verify real declaration/label chain builders without depth-proportional C stacks."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get('NODE_CHAIN_SOURCE', ROOT / 'limbo/nodes.c')).read_text()


# Extract complete production bodies without counting braces in literals or comments.
def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, SOURCE[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return SOURCE[start:brace+token.end()]
    raise RuntimeError(signature)


parts = [function('Node*\n'+name+'(') for name in
         ('mkn', 'mkdeclname', 'mkbin', 'varinit', 'caselist')]
parts += [function('int\noccurs(')]
fixture = (ROOT / 'limbo/tests/node-chains.c').read_text()
with tempfile.TemporaryDirectory(prefix='limbo-node-chains-') as temporary:
    work = Path(temporary)
    source = work / 'chains.c'
    binary = work / 'chains'
    source.write_text(fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS',
        '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT / path) for path in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(command+includes+[str(source), '-o', str(binary)], check=True)
    families = [os.environ['NODE_CHAIN_CASE']] if os.environ.get('NODE_CHAIN_CASE') else ['small', 'varinit', 'caselist', 'occurs', 'occurs-left']
    for family in families:
        print('compiler chains '+family, flush=True)
        subprocess.run([str(binary)]+([] if family == 'small' else [family]), check=True)
