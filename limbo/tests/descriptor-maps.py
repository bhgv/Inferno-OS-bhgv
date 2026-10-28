#!/usr/bin/env python3
"""Check production GC maps, descriptor interning and exception cleanup descriptors."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract real C bodies, ignoring braces in strings and comments."""
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


types = Path(os.environ.get('MAP_TYPES_SOURCE', root / 'limbo/types.c')).read_text()
parts = []
for signature in ('long\naddsize(', 'static long\nmapmark(', 'static long\ndescbytes('):
    if signature in types:
        parts.append(function(types, signature))
    elif 'addsize' in signature or 'descbytes' in signature:
        parts.append(function((root / 'limbo/types.c').read_text(), signature))
if 'struct MapWalk {' in types:
    start = types.index('/* A descriptor continuation')
    stop = types.index('/* Return the highest marked offset independently', start)
    parts.append(types[start:stop])
for signature in ('Desc*\ngendesc(', 'Desc*\nmkdesc(', 'Desc*\nmktdesc(',
                  'Desc*\nenterdesc(', 'Desc*\nusedesc(', 'long\ndescmap(', 'long\ntdescmap('):
    parts.append(function(types, signature))
# Adapt the old internal three-argument API only; keep its original map semantics.
parts = '\n'.join(parts)
parts = parts.replace('descmap(Decl *decls, uchar *map, long start)', 'descmap(Decl *decls, uchar *map, long start, long size)')
parts = parts.replace('tdescmap(Type *t, uchar *map, long offset)', 'tdescmap(Type *t, uchar *map, long offset, long size)')
for old, new in [('descmap(d, pmap, 0)', 'descmap(d, pmap, 0, size)'),
                 ('tdescmap(t, pmap, 0)', 'tdescmap(t, pmap, 0, t->size)'),
                 ('tdescmap(d->ty, map, d->offset + start)', 'tdescmap(d->ty, map, d->offset + start, size)'),
                 ('descmap(t->ids, map, offset)', 'descmap(t->ids, map, offset, size)')]:
    parts = parts.replace(old, new)
com = (root / 'limbo/com.c').read_text()
parts += '\n'+function(com, 'static int\ninlist(')+'\n'+function(com, 'static void\nexcdesc(')
unit = (root / 'limbo/tests/descriptor-maps.c').read_text().replace('IMPLEMENTATION', parts)
with tempfile.TemporaryDirectory(prefix='descriptor-maps-') as directory:
    td = Path(directory)
    target = td / 'maps.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for width in (4, 8):
        model = td / str(width)
        model.mkdir()
        isa = (root / 'include/isa.h').read_text()
        isa = re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1)
        (model / 'isa.h').write_text(isa)
        includes = ['-I'+str(model)] + ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model / 'maps'
        subprocess.run(cmd+includes+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+os.environ.get('MAP_CASE', '').split(), check=True, timeout=30)
