#!/usr/bin/env python3
"""Check numeric folding values, result types and retained evaluations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('NODES_SOURCE', root / 'limbo/nodes.c')).read_text()


# Extract complete functions without counting braces in comments and strings.
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


signatures = ['static vlong\nipow(', 'double\nrpow(', 'int\nsametree(',
              'int\nhasside(', 'Node*\nfolds(', 'Node*\nefold(',
              'Node*\nfoldvc(', 'Node*\nfoldc(', 'Node*\nfoldr(']
if 'static Long\nfoldstringlen(' in source:
    signatures.insert(0, 'static Long\nfoldstringlen(')
parts = [function(signature) for signature in signatures]
ops = sorted(set(re.findall(r'\bO[a-zA-Z0-9_]+\b', '\n'.join(parts))) | {'Ocall'})
unit = (root / 'limbo/tests/fold-identities.c').read_text()
unit = unit.replace('OPS', 'enum {' + ','.join(ops) + ', Nops};')
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))
unit = unit.replace('main(void)', 'identity_main(void)')
unit += (root / 'limbo/tests/fold-result-types.c').read_text()
with tempfile.TemporaryDirectory(prefix='fold-result-types-') as directory:
    td = Path(directory)
    (td / 'types.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd + [str(td / 'types.c'), '-lm', '-o', str(td / 'types')], check=True)
    subprocess.run([str(td / 'types')], check=True)
