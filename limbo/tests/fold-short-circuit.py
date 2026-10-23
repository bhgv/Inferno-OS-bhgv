#!/usr/bin/env python3
"""Check selected/dead-arm folding with production compiler diagnostics and VM arithmetic."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('NODES_SOURCE', root / 'limbo/nodes.c')).read_text()


def function(text, signature):
    """Extract complete functions while ignoring braces in literals and comments."""
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


signatures = ['static vlong\nipow(', 'double\nrpow(', 'static Long\nfoldstringlen(',
              'int\nsametree(', 'int\nhasside(', 'Node*\nfolds(', 'Node*\nefold(',
              'Node*\nfoldvc(', 'Node*\nfoldc(', 'Node*\nfoldr(']
parts = '\n'.join(function(source, signature) for signature in signatures)
base = (root / 'limbo/tests/fold-identities.c').read_text()
for name in ('nerror', 'nwarn'):
    old = function(base, 'static void\n'+name+'(')
    counter = 'diagnostics' if name == 'nerror' else 'warnings'
    replacement = old.replace('abort();', counter+'++;')
    base = base.replace(old, replacement)
base = base.replace('/* Reject unexpected constant-expression diagnostics. */',
                    'static int diagnostics, warnings;\n/* Count actual constant-expression diagnostics, including selected-arm failures. */')
base = base.replace('/* Tested shift amounts never need compiler warnings. */',
                    '/* Keep shift recovery warnings observable without terminating the fixture. */')
ops = sorted(set(re.findall(r'\bO\w+\b', parts)) | {'Ocall', 'Oseq'})
base = base.replace('OPS', 'enum {'+','.join(ops)+',Nops};')
base = base.replace('IMPLEMENTATION', parts).replace('main(void)', 'identity_main(void)')
base = base.replace('Texception};', 'Texception,Tfn};').replace('Dlocal};', 'Dlocal,Dfn};')
base = base.replace(function(base, 'static Node*\nrewrite('), 'REWRITE_ADAPTER')
ecom = Path(os.environ.get('ECOM_SOURCE', root / 'limbo/ecom.c')).read_text()
control = ''
if 'static Node*\nrewritecontrol(' in ecom:
    control = '#define HAVE_REWRITE_CONTROL 1\n'+function(ecom, 'static Node*\nrewritecontrol(')
adapter = (root / 'limbo/tests/fold-short-rewrite.c').read_text().replace('CONTROL_IMPLEMENTATION', control)
base = base.replace('REWRITE_ADAPTER', adapter)

runtime = (root / 'libinterp/xec.c').read_text()
vm = [function(runtime, 'static WORD\nshiftcount(')]
vm += [function(runtime, 'OP('+name+')') for name in
       ('divb', 'modb', 'divw', 'modw', 'divl', 'modl', 'iexpw', 'iexpl',
        'shlb', 'shrb', 'shlw', 'shrw', 'shll', 'shrl')]
vm = re.sub(r'\berror\(', 'vmerror(', '\n'.join(vm))
unit = base+(root / 'limbo/tests/fold-short-circuit.c').read_text()
unit = unit.replace('SHORT_VM', vm)
with tempfile.TemporaryDirectory(prefix='fold-short-circuit-') as directory:
    td = Path(directory)
    target = td / 'short.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for width in (32, 64):
        binary = td / ('short'+str(width))
        subprocess.run(cmd+['-DWORD_BITS='+str(width), str(target), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+os.environ.get('SHORT_CIRCUIT_CASE', '').split(), check=True, timeout=30)
