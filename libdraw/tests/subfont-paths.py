#!/usr/bin/env python3
"""Verify full subfont paths and the exact depth-search order."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
path = Path(os.environ.get('SUBFONTNAME_SOURCE', root/'libdraw/subfontname.c'))
source = re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M)
unit = (root/'libdraw/tests/subfont-paths.c').read_text().replace('IMPLEMENTATION', source)
with tempfile.TemporaryDirectory(prefix='subfont-paths-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
