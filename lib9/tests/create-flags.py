#!/usr/bin/env python3
"""Exercise production exclusive creation and close-on-exec with real host files."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('CREATE_FLAGS_SOURCE', root/'lib9/create.c')).read_text()
source = re.sub(r'^#include[^\n]*', '', source, flags=re.M)
unit = (root/'lib9/tests/create-flags.c').read_text().replace('IMPLEMENTATION', source)
with tempfile.TemporaryDirectory(prefix='create-flags-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    for fallback in (False, True):
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        if fallback: cmd += ['-DFALLBACK_CEXEC']
        cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary), directory], check=True)
