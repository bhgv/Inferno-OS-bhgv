#!/usr/bin/env python3
"""Check production communication rewrite callbacks and ordered tuple alias queries."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract definitions without interpreting comment/literal braces as C nesting.
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


com = Path(os.environ.get('COMM_WALK_SOURCE', ROOT/'limbo/com.c')).read_text()
ecom = Path(os.environ.get('ALIAS_WALK_SOURCE', ROOT/'limbo/ecom.c')).read_text()
parts = []
if 'struct CommRewrite {' in com:
    start = com.index('/* Deferred operand cells')
    parts.append(com[start:com.index('/* Compare local groups', start)])
else:
    parts.append(function(com, 'Node*\nrewritecomm('))
if 'aliassave(Node ***work' in ecom:
    parts.append(function(ecom, 'static void\naliassave('))
parts += [function(ecom, 'int\n'+name+'(') for name in ('tupaliased', 'tupsaliased')]
with tempfile.TemporaryDirectory(prefix='comm-alias-walks-') as temporary:
    work = Path(temporary)
    source = work/'walks.c'
    fixture = (ROOT/'limbo/tests/comm-alias-walks.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/comm-alias-reference.c').read_text())
    source.write_text(fixture)
    for width in (4, 8):
        model = work/str(width)
        model.mkdir()
        isa = (ROOT/'include/isa.h').read_text()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model/'walks'
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        families = [os.environ['COMM_ALIAS_CASE']] if os.environ.get('COMM_ALIAS_CASE') else ['shallow', 'mutation', 'rewrite', 'alias', 'tuple', 'edges']
        for family in families:
            subprocess.run([str(binary), family], check=True)
