#!/usr/bin/env python3
"""Run real native formatters against independent integer and field-bound checks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
names = ('rune', 'snprint', 'vsnprint', 'dofmt', 'fmt', 'fmtlock', 'errfmt',
         'utflen', 'utfnlen', 'runestrlen', 'dorfmt')
with tempfile.TemporaryDirectory(prefix='format-bounds-') as directory:
    binary = Path(directory) / 'format'
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'lib9')]
    sources = [Path(os.environ.get('FORMAT_'+name.upper()+'_SOURCE', root / ('lib9/'+name+'.c'))) for name in names]
    command += [str(root / 'lib9/tests/format-bounds.c')] + [str(p) for p in sources]
    subprocess.run(command+['-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
