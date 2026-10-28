#!/usr/bin/env python3
"""Verify real inline dependency checks, including deep chains and unrelated cycles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get('INLINE_GRAPH_SOURCE', ROOT/'limbo/com.c')).read_text()
start = SOURCE.index('static int\nckinline0(')
brace = SOURCE.index('{', start)
depth = 0
for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', SOURCE[brace:]):
    if token.group() == '{':
        depth += 1
    elif token.group() == '}':
        depth -= 1
        if depth == 0:
            body = SOURCE[start:brace+token.end()]
            break
else:
    raise RuntimeError('ckinline0 definition')
if 'struct InlineWalk {' in SOURCE:
    begin = SOURCE.index('typedef struct InlineWalk InlineWalk;')
    stop = SOURCE.index('\n};', begin)+3
    body = SOURCE[begin:stop]+'\n\n\n'+body
with tempfile.TemporaryDirectory(prefix='limbo-inline-graphs-') as temporary:
    work = Path(temporary)
    source = work/'graphs.c'
    binary = work/'graphs'
    source.write_text((ROOT/'limbo/tests/inline-graphs.c').read_text().replace('IMPLEMENTATION', body))
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(command+[str(source), '-o', str(binary)], check=True)
    for family in ([os.environ['INLINE_GRAPH_CASE']] if os.environ.get('INLINE_GRAPH_CASE') else ['small', 'tree', 'chain', 'cycle', 'shared']):
        subprocess.run([str(binary), family], check=True)
