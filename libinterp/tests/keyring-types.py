#!/usr/bin/env python3
"""Run actual Keyring object validators against minimal and initialized native heaps."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract complete functions while ignoring braces in literals and comments."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


source = Path(os.environ.get('KEYRING_TYPES_SOURCE', root / 'libinterp/keyring.c')).read_text()
parts = [function(source, signature) for signature in
         ('static SigAlg*\ncheckSigAlg(', 'static SK*\ncheckSK(',
          'static PK*\ncheckPK(', 'Certificate*\ncheckCertificate(')]
unit = (root / 'libinterp/tests/keyring-types.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='keyring-types-') as directory:
    target = Path(directory) / 'types.c'
    target.write_text(unit)
    binary = Path(directory) / 'types'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'libinterp', 'libkeyring')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)]+os.environ.get('KEYRING_TYPES_CASE', '').split(), check=True, timeout=30)
