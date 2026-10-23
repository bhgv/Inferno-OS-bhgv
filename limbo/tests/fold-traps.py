#!/usr/bin/env python3
"""Compare real recursive folding with actual VM arithmetic/list/slice error behavior."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('NODES_SOURCE', root / 'limbo/nodes.c')).read_text()
tables = Path(os.environ.get('OPTAB_SOURCE', root / 'limbo/optab.c')).read_text()


def function(text, signature):
    """Extract complete C functions while ignoring literal and comment braces."""
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


signatures = ['static vlong\nipow(', 'double\nrpow(', 'int\nsametree(',
              'int\nhasside(', 'Node*\nfolds(', 'Node*\nefold(',
              'Node*\nfoldvc(', 'Node*\nfoldc(', 'Node*\nfoldr(']
if 'static Long\nfoldstringlen(' in source:
    signatures.insert(0, 'static Long\nfoldstringlen(')
parts = [function(source, name) for name in signatures]
a = tables.index('int setsideeffect[] = {')
b = tables.index('\n};', a)+3
effects = tables[a:b].replace('int setsideeffect', 'static int effectops')
ops = sorted(set(re.findall(r'\bO\w+\b', '\n'.join(parts)+effects)) |
             {'Ocall', 'Ohd', 'Otl', 'Oslice', 'Olsh', 'Orsh', 'Oexp', 'Oseq'})
base = (root / 'limbo/tests/fold-identities.c').read_text()
base = base.replace('OPS', 'enum {'+','.join(ops)+',Nops};')
base = base.replace('Texception};', 'Texception,Tlist};')
base = base.replace('IMPLEMENTATION', '\n'.join(parts))
base = base.replace('main(void)', 'identity_main(void)')
runtime = (root / 'libinterp/xec.c').read_text()
native = [function(runtime, 'static WORD\nshiftcount(')]
native += [function(runtime, 'OP('+name+')') for name in
           ('shlb', 'shrb', 'shlw', 'shrw', 'shll', 'shrl', 'iexpw', 'iexpl', 'headw', 'tail')]
native.append(function((root / 'libinterp/string.c').read_text(), 'String*\nslicer('))
native = re.sub(r'\berror\(', 'vmerror(', '\n'.join(native))
unit = base+(root / 'limbo/tests/fold-traps.c').read_text()
unit = unit.replace('EFFECT_TABLE', effects).replace('VM_IMPLEMENTATION', native)
with tempfile.TemporaryDirectory(prefix='fold-traps-') as directory:
    td = Path(directory)
    target = td / 'traps.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (32, 64):
        binary = td / ('traps'+str(width))
        subprocess.run(cmd+['-DWORD_BITS='+str(width), str(target), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+os.environ.get('FOLD_TRAPS_CASE', '').split(), check=True, timeout=30)
