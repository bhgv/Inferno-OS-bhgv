#!/usr/bin/env python3
"""Exercise actual declaration construction, tuple grouping and nested := order."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract definitions while ignoring braces in comments and literals.
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


decls = Path(os.environ.get('DECL_WALK_SOURCE', ROOT/'limbo/decls.c')).read_text()
ecom = Path(os.environ.get('DECL_WALK_ECOM', ROOT/'limbo/ecom.c')).read_text()
types = (ROOT/'limbo/types.c').read_text()
parts = []
if 'declrefsum(int count' in decls:
    parts.append(function(decls, 'int\ndeclrefsum('))
if 'struct DeclWalk {' in decls:
    start = decls.index('/* Pending declaration lists')
    parts.append(decls[start:decls.index('int\ndasdecl(', start)])
else:
    parts += [function(decls, sig) for sig in ('static int\nrecdasdecl(', 'static int\nrecmark(')]
parts += [function(decls, sig) for sig in ('int\ndasdecl(', 'void\ngdasdecl(')]
parts.append(function(ecom, 'static int\ntupblk0('))
support = [function(decls, sig) for sig in ('Decl*\nmkdecl(', 'Decl*\nmkids(', 'void\nredecl(', 'void\ninstallids(', 'long\nidoffsets(')]
support += [function(types, sig) for sig in ('long\naddsize(', 'long\nalign(')]
with tempfile.TemporaryDirectory(prefix='declaration-walks-') as temporary:
    work = Path(temporary)
    source = work/'walks.c'
    fixture = (ROOT/'limbo/tests/declaration-walks.c').read_text()
    fixture = fixture.replace('SUPPORT', '\n\n\n'.join(support))
    fixture = fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/declaration-walks-reference.c').read_text())
    source.write_text(fixture)
    for width in (4, 8):
        model = work/str(width)
        model.mkdir()
        isa = (ROOT/'include/isa.h').read_text()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1))
        header = Path(os.environ.get('DECL_WALK_HEADER', ROOT/'limbo/limbo.h')).read_text()
        (model/'limbo.h').write_text(header)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model/'walks'
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        families = [os.environ['DECL_WALK_CASE']] if os.environ.get('DECL_WALK_CASE') else ['shallow', 'edges', 'declare', 'mark', 'global', 'block', 'groups', 'overflow']
        for family in families:
            subprocess.run([str(binary), family], check=True)
