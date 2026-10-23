#!/usr/bin/env python3
"""Check production type/frame layout with bounded integer oracles and fatal cases."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract complete C functions without counting comment/string braces."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


types = Path(os.environ.get('LAYOUT_TYPES_SOURCE', root / 'limbo/types.c')).read_text()
decls = Path(os.environ.get('LAYOUT_DECLS_SOURCE', root / 'limbo/decls.c')).read_text()
parts = []
for signature in ('long\naddsize(', 'static long\nsizetable('):
    if signature in types:
        parts.append(function(types, signature))
    elif 'addsize' in signature:
        parts.append(function((root / 'limbo/types.c').read_text(), signature))
parts += [function(types, signature) for signature in ('void\nsizetype(', 'Szal\nsizeids(', 'long\nalign(')]
parts.append(function(decls, 'long\nidoffsets('))
unit = (root / 'limbo/tests/layout-bounds.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='layout-bounds-') as directory:
    td = Path(directory)
    target = td / 'layout.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (4, 8):
        model = td / str(width)
        model.mkdir()
        isa = (root / 'include/isa.h').read_text()
        isa = re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1)
        (model / 'isa.h').write_text(isa)
        includes = ['-I'+str(model)] + ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model / 'layout'
        subprocess.run(cmd+includes+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+os.environ.get('LAYOUT_CASE', '').split(), check=True, timeout=30)
