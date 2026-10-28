#!/usr/bin/env python3
"""Check iterative Type diagnostics against shallow spelling and deep input Types."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Ignore braces inside C comments and literals while extracting actual definitions.
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


types = Path(os.environ.get('TYPE_PRINT_TYPES', ROOT/'limbo/types.c')).read_text()
lex = (ROOT/'limbo/lex.c').read_text()
parts = [types[types.index('char *kindname[Tend]'):types.index('Tattr tattr')]]
if 'struct TypePrint {' in types:
    start = types.index('/* Continuation kinds')
    parts.append(types[start:types.index('char*\nstprint(', start)])
else:
    parts.append(function(types, 'char*\ntprint('))
parts += [function(types, 'Type*\nvaltmap(')]
parts += [function(lex, 'char*\nsecpy('), function(lex, 'char*\nseprint(')]
with tempfile.TemporaryDirectory(prefix='type-print-') as temporary:
    work = Path(temporary)
    source = work/'print.c'
    binary = work/'print'
    fixture = (ROOT/'limbo/tests/type-print.c').read_text()
    fixture = fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/type-print-reference.c').read_text())
    source.write_text(fixture)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo', 'lib9')]
    support = [str(ROOT/'lib9'/(name+'.c')) for name in ('rune', 'dofmt', 'fmt', 'fmtlock', 'errfmt', 'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'vseprint')]
    subprocess.run(command+[str(source)]+support+['-o', str(binary)], check=True)
    for family in ([os.environ['TYPE_PRINT_CASE']] if os.environ.get('TYPE_PRINT_CASE') else ['shallow', 'inst', 'arrow', 'deep']):
        subprocess.run([str(binary), family], check=True)
