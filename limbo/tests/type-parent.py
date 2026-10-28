#!/usr/bin/env python3
"""Check parent-Type predicates and lazy reconstruction against shallow historical rules."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract definitions without interpreting braces in comments or literals as C nesting.
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


types = Path(os.environ.get('TYPE_PARENT_TYPES', ROOT/'limbo/types.c')).read_text()
decls = (ROOT/'limbo/decls.c').read_text()
parts = []
if 'struct ParentFields {' in types:
    start = types.index('/* Deferred tuple fields')
    parts.append(types[start:types.index('/* Check structural parent compatibility', start)])
parts += [function(types, signature) for signature in ('static int\ntparent0(', 'static Type*\ntparent1(', 'Type*\ntparent(')]
support = [function(types, 'Type*\nmktype('), function(decls, 'Decl*\ndupdecl('), function(decls, 'Decl*\ndupdecls(')]
with tempfile.TemporaryDirectory(prefix='type-parent-') as temporary:
    work = Path(temporary)
    source = work/'parent.c'
    binary = work/'parent'
    fixture = (ROOT/'limbo/tests/type-parent.c').read_text()
    fixture = fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('SUPPORT', '\n\n\n'.join(support))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/type-parent-reference.c').read_text())
    source.write_text(fixture)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(command+[str(source), '-o', str(binary)], check=True)
    for family in ([os.environ['TYPE_PARENT_CASE']] if os.environ.get('TYPE_PARENT_CASE') else ['shallow', 'check', 'build', 'tuple', 'identity', 'nil']):
        subprocess.run([str(binary), family], check=True)
