#!/usr/bin/env python3
"""Execute production pushback, offset, buffered-count and relative-seek boundaries."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract production definitions while ignoring braces in strings and comments.
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


parts = []
for name, signatures in [('bgetc', ['int\nBgetc(', 'int\nBungetc(']),
                         ('bgetrune', ['int\nBungetrune(']),
                         ('boffset', ['vlong\nBoffset(']),
                         ('bbuffered', ['int\nBbuffered(']),
                         ('bseek', ['vlong\nBseek('])]:
    source = Path(os.environ.get('BIO_BOUND_'+name.upper(), ROOT/('libbio/'+name+'.c'))).read_text()
    parts.extend(function(source, signature) for signature in signatures)
with tempfile.TemporaryDirectory(prefix='bio-boundaries-') as temporary:
    work = Path(temporary)
    source = work/'boundaries.c'
    binary = work/'boundaries'
    source.write_text((ROOT/'libbio/tests/boundaries.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include')]
    subprocess.run(command+[str(source), '-o', str(binary)], check=True)
    for family in ([os.environ['BIO_BOUND_CASE']] if os.environ.get('BIO_BOUND_CASE') else ['byte', 'rune', 'offset', 'buffered', 'seek']):
        subprocess.run([str(binary), family], check=True)
