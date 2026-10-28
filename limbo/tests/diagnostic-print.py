#!/usr/bin/env python3
"""Check bounded AST diagnostics against a shallow reference and deep input trees."""
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


nodes = Path(os.environ.get('DIAGNOSTIC_NODES', ROOT/'limbo/nodes.c')).read_text()
lex = (ROOT/'limbo/lex.c').read_text()
parts = [function(nodes, 'int\nexpconv('), function(nodes, 'int\nnodeconv('), function(nodes, 'char*\nnprint(')]
if 'struct NodePrint {' in nodes:
    start = nodes.index('#define NodePrintPlan')
    parts.insert(0, nodes[start:nodes.index('/* Convert a debug tree', start)])
else:
    parts += [function(nodes, 'char*\neprint('), function(nodes, 'char*\neprintlist(')]
parts += [function(lex, 'char*\nsecpy('), function(lex, 'char*\nseprint('), function(lex, 'char*\nstringpr(')]
with tempfile.TemporaryDirectory(prefix='diagnostic-print-') as temporary:
    work = Path(temporary)
    source = work/'print.c'
    binary = work/'print'
    fixture = (ROOT/'limbo/tests/diagnostic-print.c').read_text()
    fixture = fixture.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    fixture = fixture.replace('REFERENCE', (ROOT/'limbo/tests/diagnostic-reference.c').read_text())
    source.write_text(fixture)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo', 'lib9')]
    support = [str(ROOT/'lib9'/(name+'.c')) for name in ('rune', 'dofmt', 'fmt', 'fmtlock', 'errfmt', 'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'vseprint')]
    subprocess.run(command+[str(source)]+support+['-o', str(binary)], check=True)
    for family in ([os.environ['DIAGNOSTIC_CASE']] if os.environ.get('DIAGNOSTIC_CASE') else ['shallow', 'quote', 'deep', 'dump']):
        subprocess.run([str(binary), family], check=True)
