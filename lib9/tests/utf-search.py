#!/usr/bin/env python3
"""Check real native UTF length/search helpers with exact prefix/position vectors."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='utf-search-') as directory:
    binary = Path(directory) / 'utf'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include')]
    sources = [Path(os.environ.get('UTF_'+name.upper()+'_SOURCE', root / ('lib9/'+name+'.c')))
               for name in ('rune', 'utfnlen', 'utfrune', 'utfrrune')]
    subprocess.run(cmd+[str(root / 'lib9/tests/utf-search.c')]+[str(p) for p in sources]+['-o',str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
