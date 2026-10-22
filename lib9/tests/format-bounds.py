#!/usr/bin/env python3
"""Run real native formatters against independent integer and field-bound checks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
library = os.environ.get('NATIVE_LIBRARY', 'lib9')
names = ('rune', 'snprint', 'vsnprint', 'dofmt', 'fmt', 'fmtlock', 'errfmt',
         'utflen', 'utfnlen', 'runestrlen', 'dorfmt')
with tempfile.TemporaryDirectory(prefix='format-bounds-') as directory:
    binary = Path(directory) / 'format'
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'lib9')]
    sources = []
    for name in names:
        default = root / library / (name+'.c')
        if not default.exists():
            default = root / 'lib9' / (name+'.c')
        sources.append(Path(os.environ.get('FORMAT_'+name.upper()+'_SOURCE', default)))
    command += [str(root / 'lib9/tests/format-bounds.c')] + [str(p) for p in sources]
    subprocess.run(command+['-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
