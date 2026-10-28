#!/usr/bin/env python3
"""Check production copying, communication search and global declaration traversal."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NODES = Path(os.environ.get('NODE_TRANSFORM_NODES', ROOT/'limbo/nodes.c')).read_text()
TYPES = Path(os.environ.get('NODE_TRANSFORM_TYPES', ROOT/'limbo/typecheck.c')).read_text()


# Extract definitions without treating literal or comment braces as C structure.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


parts = [function(NODES, 'Node*\ndupn('), function(TYPES, 'Node*\nhascomm('), function(TYPES, 'void\ngsort(')]
if 'struct NodeCopy {' in NODES:
    start = NODES.index('typedef struct NodeCopy NodeCopy;')
    stop = NODES.index('\n};', start)+3
    parts.insert(0, NODES[start:stop])
with tempfile.TemporaryDirectory(prefix='limbo-node-transforms-') as temporary:
    work = Path(temporary)
    source = work/'transforms.c'
    binary = work/'transforms'
    source.write_text((ROOT/'limbo/tests/node-transforms.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(command+[str(source), '-o', str(binary)], check=True)
    for family in ([os.environ['NODE_TRANSFORM_CASE']] if os.environ.get('NODE_TRANSFORM_CASE') else ['small', 'copy', 'comm', 'sort']):
        subprocess.run([str(binary), family], check=True)
