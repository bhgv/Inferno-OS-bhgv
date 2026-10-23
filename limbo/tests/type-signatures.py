#!/usr/bin/env python3
"""Check production signature serialization and MD5 folding against hashlib."""
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('SIGNATURE_TYPES', root / 'limbo/types.c')).read_text()
start = source.index('/* Reserve one byte') if '/* Reserve one byte' in source else source.index('/*\n * create type signatures')
end = source.index('/* must a variable', start)
unit = (root / 'limbo/tests/type-signatures.c').read_text().replace('IMPLEMENTATION', source[start:end])
# Compile the repository MD5 implementation with the same public host types.
for name in ('md5.c', 'md5block.c'):
    unit += '\n' + (root / 'libsec/port' / name).read_text().replace('#include "os.h"', '').replace('#include <libsec.h>', '')
with tempfile.TemporaryDirectory(prefix='type-signatures-') as directory:
    c = Path(directory) / 'test.c'
    c.write_text(unit)
    binary = Path(directory) / 'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(cmd + [str(c), '-o', str(binary)], check=True)
    result = subprocess.check_output([str(binary)], text=True)
    for line in result.splitlines():
        text, actual = line.rsplit(' ', 1)
        digest = hashlib.md5(text.encode()).digest()
        expected = 0
        for i in range(0, 16, 4):
            expected ^= int.from_bytes(digest[i:i+4], 'little')
        assert int(actual, 16) == expected, (text, actual, expected)
    print('type signatures: bounds, recursion, sentinels and hashlib vectors passed')
