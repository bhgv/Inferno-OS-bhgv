#!/usr/bin/env python3
"""Check lexical scope bounds, global binding continuations and label-stack growth."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract whole definitions while ignoring braces in comments and literals.
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


decls = Path(os.environ.get('SCOPE_BIND_DECLS', ROOT/'limbo/decls.c')).read_text()
checker = Path(os.environ.get('SCOPE_BIND_CHECKER', ROOT/'limbo/typecheck.c')).read_text()
support = [function(decls, sig) for sig in ('int\ndeclrefsum(', 'Decl*\nmkdecl(', 'Decl*\nmkids(', 'void\nredecl(', 'void\ninstallids(', 'Decl*\nundefed(', 'void\npushscope(', 'void\nrepushids(', 'Decl*\npopscope(', 'Decl*\npopids(', 'static void\nfreeloc(')]
parts = []
if 'struct GlobalBind {' in checker:
    start = checker.index('/* Binding continuations')
    parts.append(checker[start:checker.index('/*\n * check all of the > declarations', start)])
else:
    parts.append(function(checker, 'void\ngbind('))
parts.append(function(checker, 'void\npushlabel('))
with tempfile.TemporaryDirectory(prefix='scope-binding-') as temporary:
    work = Path(temporary)
    source = work/'binding.c'
    fixture = (ROOT/'limbo/tests/scope-binding.c').read_text().replace('SUPPORT', '\n\n\n'.join(support))
    fixture = fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/scope-binding-reference.c').read_text())
    source.write_text(fixture)
    for width in (4, 8):
        model = work/str(width)
        model.mkdir()
        isa = (ROOT/'include/isa.h').read_text()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model/'binding'
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        families = [os.environ['SCOPE_BIND_CASE']] if os.environ.get('SCOPE_BIND_CASE') else ['shallow', 'mutation', 'edges', 'nested', 'scope', 'repush', 'sequence', 'type', 'labels', 'labelbounds']
        for family in families:
            subprocess.run([str(binary), family], check=True)
