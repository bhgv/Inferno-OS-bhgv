#!/usr/bin/env python3
"""Test both production numeric callback scanners with exact token consumption."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='charstod-') as directory:
    td = Path(directory)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include','include')]
    for library in ('lib9','libkern'):
        source = Path(os.environ.get('CHARSTOD_SOURCE', root / library / 'charstod.c')).read_text()
        unit = (root / 'lib9/tests/charstod.c').read_text().replace('IMPLEMENTATION', source)
        target = td / (library+'.c'); target.write_text(unit)
        binary = td / library
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
