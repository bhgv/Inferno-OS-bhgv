#!/usr/bin/env python3
"""Compile production PEM parsing and base64 decoding with fault injection."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parts = []
for path in ('lib9/u64.c', 'libsec/port/decodepem.c'):
    source = Path(os.environ.get('PEM_SOURCE', root / path)) if 'decodepem' in path else root / path
    parts.append('\n'.join(line for line in source.read_text().splitlines()
                           if not line.startswith('#include')))
unit = (root / 'libsec/tests/pem.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='pem-') as directory:
    target = Path(directory) / 'pem.c'
    target.write_text(unit)
    binary = Path(directory) / 'pem'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd + [str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=20)
