#!/usr/bin/env python3
"""Check C stub Type/name printing and layout arithmetic against production sources."""
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


types = (ROOT/'limbo/types.c').read_text()
stubs = Path(os.environ.get('STUB_PRINT_SOURCE', ROOT/'limbo/stubs.c')).read_text()
lex = (ROOT/'limbo/lex.c').read_text()
parts = [types[types.index('char *kindname[Tend]'):types.index('Tattr tattr')]]
parts += [function(types, 'long\naddsize('), function(stubs, 'static long\nstubalign(')]
parts.append(function(stubs, 'char*\ndotprint('))
parts.append(stubs[stubs.index('char *ckindname[Tend]'):stubs.index('};', stubs.index('char *ckindname[Tend]'))+2])
if 'struct StubPrint {' in stubs:
    start = stubs.index('/* A C spelling continuation')
    parts.append(stubs[start:stubs.index('static void\npickadtstub(', start)])
else:
    parts.append(function(stubs, 'char*\nctprint('))
parts += [function(lex, 'char*\nsecpy('), function(lex, 'char*\nseprint(')]
with tempfile.TemporaryDirectory(prefix='stub-print-') as temporary:
    work = Path(temporary)
    source = work/'print.c'
    binary = work/'print'
    fixture = (ROOT/'limbo/tests/stub-print.c').read_text()
    fixture = fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/stub-print-reference.c').read_text())
    source.write_text(fixture)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo', 'lib9')]
    support = [str(ROOT/'lib9'/(name+'.c')) for name in ('rune', 'dofmt', 'fmt', 'fmtlock', 'errfmt', 'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'vseprint')]
    subprocess.run(command+[str(source)]+support+['-o', str(binary)], check=True)
    for family in ([os.environ['STUB_PRINT_CASE']] if os.environ.get('STUB_PRINT_CASE') else ['shallow', 'dots', 'refs', 'picks', 'tuples', 'alignment', 'layouts']):
        for width in (4, 8):
            subprocess.run([str(binary), family, str(width)], check=True)
    if not os.environ.get('STUB_PRINT_CASE'):
        for width in (4, 8):
            emitted = subprocess.run([str(binary), 'emit', str(width)], check=True, text=True, stdout=subprocess.PIPE).stdout
            target = work/('emitted-'+str(width)+'.c')
            target.write_text(emitted)
            subprocess.run(shlex.split(os.environ.get('CC', 'cc'))+['-std=c99', '-pedantic-errors', '-Werror', '-fsyntax-only', str(target)], check=True)
            print('stub print emitted width '+str(width)+': 1000 declarations pass strict C99'+(' and native sizeof checks' if width == 8 else ''), flush=True)
