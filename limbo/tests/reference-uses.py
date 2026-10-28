#!/usr/bin/env python3
"""Exercise actual declaration use and temporary-retention entry points."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract complete definitions while ignoring braces in comments and literals.
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


sources = {name: Path(os.environ.get('REF_USE_'+name.upper(), ROOT/('limbo/'+name+'.c'))).read_text() for name in ('decls', 'nodes', 'types', 'typecheck', 'ecom', 'gen')}
parts = [function(sources['decls'], sig) for sig in ('int\ndeclrefsum(', 'Decl*\nmkdecl(', 'Decl*\nmkids(')]
parts += [function(sources['nodes'], 'Node*\n'+name+'(') for name in ('mkn', 'mkdeclname')]
parts += [function(sources['typecheck'], 'static void\nincrefs('), function(sources['types'], 'void\nmodrefable(')]
parts += [function(sources['gen'], signature) for signature in ('Node*\ntalloc(', 'void\ntfree(', 'void\ntfreelater(', 'void\ntfreenow(', 'Node*\ntacquire(', 'void\ntrelease(')]
parts.append(function(sources['ecom'], 'Decl*\nglobalsconst('))
# Inspect all production arithmetic sites; initialization/copy assignments remain ordinary.
if not any(key.startswith('REF_USE_') for key in os.environ):
    for name, source in sources.items():
        if re.search(r'->(?:refs|tref)\s*(?:\+\+|--|[+*/-]=)|(?:\+\+|--)\s*[\w\[\]>\-]+->(?:refs|tref)', source):
            raise RuntimeError('unchecked compiler reference arithmetic in '+name)
with tempfile.TemporaryDirectory(prefix='reference-uses-') as temporary:
    work = Path(temporary)
    source = work/'uses.c'
    source.write_text((ROOT/'limbo/tests/reference-uses.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
    for width in (4, 8):
        model = work/str(width)
        model.mkdir()
        isa = (ROOT/'include/isa.h').read_text()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), isa, count=1))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        binary = model/'uses'
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        families = [os.environ['REF_USE_CASE']] if os.environ.get('REF_USE_CASE') else ['name', 'imports', 'module', 'string', 'allocate', 'acquire', 'release', 'free', 'later', 'deferred', 'chain', 'sequences', 'edges']
        for family in families:
            subprocess.run([str(binary), family], check=True)
