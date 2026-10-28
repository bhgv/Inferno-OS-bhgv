#!/usr/bin/env python3
"""Run real Bio formatting/flush with real dofmt and deterministic native writes."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parts = []
for name in ('bvprint', 'bprint', 'bflush', 'fmtfd', 'vfprint'):
    source = Path(os.environ.get('BIO_PRINT_'+name.upper(), ROOT/(('lib9/' if name in ('fmtfd', 'vfprint') else 'libbio/')+name+'.c'))).read_text()
    parts.append(re.sub(r'^#include[^\n]*\n', '', source, flags=re.M))
with tempfile.TemporaryDirectory(prefix='bio-print-state-') as temporary:
    work = Path(temporary)
    source = work/'print.c'
    binary = work/'print'
    source.write_text((ROOT/'libbio/tests/print-state.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'lib9')]
    names = ('rune', 'dofmt', 'fmt', 'fmtlock', 'errfmt', 'utflen', 'utfnlen', 'runestrlen', 'dorfmt')
    command += [str(source)]+[str(ROOT/'lib9'/(name+'.c')) for name in names]
    subprocess.run(command+['-o', str(binary)], check=True)
    for family in ([os.environ['BIO_PRINT_CASE']] if os.environ.get('BIO_PRINT_CASE') else ['state', 'flush', 'layout', 'cursor', 'fdinit', 'fdflush', 'fdformat']):
        subprocess.run([str(binary), family], check=True)
