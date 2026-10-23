#!/usr/bin/env python3
"""Run actual font parser and file loader with allocation and I/O fault injection."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parts = []
for name in ('buildfont', 'openfont'):
    path = Path(os.environ.get(name.upper()+'_SOURCE', root/'libdraw'/(name+'.c')))
    parts.append(re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M))
unit = (root/'libdraw/tests/font-loading.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='font-loading-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# Compile existing callers against the shared prototypes, including pointer returns.
cmd = shlex.split(os.environ.get('CC', 'cc'))
cmd += ['-fsyntax-only', '-Werror=implicit-function-declaration', '-Werror=int-conversion']
cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include', 'emu/port')]
cmd += [str(root/'libdraw'/p) for p in
        ('buildfont.c', 'openfont.c', 'getsubfont_ft.c', 'getsubfont.c', 'font.c')]
subprocess.run(cmd, check=True)
