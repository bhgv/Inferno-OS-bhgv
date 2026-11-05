#!/usr/bin/env python3
"""Exercise production module-file reads and all native ownership transitions."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
path = Path(os.environ.get('READMOD_IO_SOURCE', root/'libinterp/readmod.c'))
source = re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M)
unit = (root/'libinterp/tests/readmod-io.c').read_text().replace('IMPLEMENTATION', source)
with tempfile.TemporaryDirectory(prefix='readmod-io-') as directory:
    target = Path(directory)/'test.c'
    target.write_text(unit)
    binary = Path(directory)/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    arch = os.environ.get('NATIVE64_ARCH', 'arm')
    runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
    cmd += ['-I'+str(root/p) for p in (f'Linux/{arch}/include', 'Linux/386/include', 'include')]
    for mode in ([], ['-DDIS_NATIVE64_SCALAR_BOOT=1']):
        subprocess.run(cmd+mode+[str(target), '-o', str(binary)], check=True)
        subprocess.run(runner+[str(binary)], check=True)
