#!/usr/bin/env python3
"""Exercise production pointer maps and zero-initialization queries at both slot widths."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract complete definitions while ignoring braces in C comments and literals.
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


types = Path(os.environ.get('TYPE_WALK_TYPES', ROOT/'limbo/types.c')).read_text()
parts = [function(types, 'long\naddsize('), function(types, 'static long\nmapmark(')]
if 'struct MapWalk {' in types:
    start = types.index('/* A descriptor continuation')
    parts.append(types[start:types.index('/* Return the highest marked offset independently', start)])
if 'zerowalk(Type *type' in types:
    parts.append(function(types, 'static int\nzerowalk('))
for name in ('long\ndescmap(', 'long\ntdescmap(', 'int\ntmustzero(', 'int\nmustzero('):
    parts.append(function(types, name))
with tempfile.TemporaryDirectory(prefix='type-walks-') as temporary:
    work = Path(temporary)
    source = work/'walks.c'
    source.write_text((ROOT/'limbo/tests/type-walks.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
    widths = [int(os.environ['TYPE_WALK_WIDTH'])] if os.environ.get('TYPE_WALK_WIDTH') else [4, 8]
    for width in widths:
        if width not in (4, 8):
            raise ValueError(width)
        model = work/str(width)
        model.mkdir()
        isa = (ROOT/'include/isa.h').read_text()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model/'walks'
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        for family in ([os.environ['TYPE_WALK_CASE']] if os.environ.get('TYPE_WALK_CASE') else ['shallow', 'map', 'zero', 'reject']):
            print('pointer slots '+str(width)+' '+family, flush=True)
            subprocess.run([str(binary), family], check=True)
