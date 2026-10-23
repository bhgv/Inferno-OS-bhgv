#!/usr/bin/env python3
"""Run both real quote formatters with finite-span and end-to-end regressions."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
names = ('rune', 'snprint', 'vsnprint', 'dofmt', 'fmt', 'fmtlock', 'errfmt',
         'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'fmtquote', 'tokenize', 'utfrune',
         'runeseprint', 'runevseprint')
with tempfile.TemporaryDirectory(prefix='quote-bounds-') as directory:
    for library in ('lib9', 'libkern'):
        binary = Path(directory) / library
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'lib9')]
        sources = []
        for name in names:
            source = root / library / (name+'.c')
            if not source.exists():
                source = root / 'lib9' / (name+'.c')
            if name == 'fmtquote':
                source = Path(os.environ.get('QUOTE_SOURCE', source))
            sources.append(str(source))
        subprocess.run(cmd+[str(root / 'lib9/tests/quote-bounds.c')]+sources+['-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=60)
