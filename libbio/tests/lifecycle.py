#!/usr/bin/env python3
"""Run production buffer initialization, byte I/O and cleanup with fault adapters."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parts = []
for name in ('binit', 'bputc', 'bgetc', 'bflush'):
    path = Path(os.environ.get('BIO_' + name.upper() + '_SOURCE',
                               root / ('libbio/' + name + '.c')))
    source = path.read_text()
    parts.append(source.replace('#include\t"lib9.h"', '')
                 .replace('#include\t<bio.h>', ''))
unit = (root / 'libbio/tests/lifecycle.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='bio-lifecycle-') as directory:
    target = Path(directory) / 'lifecycle.c'
    target.write_text(unit)
    binary = Path(directory) / 'lifecycle'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS',
                       '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I' + str(root / p) for p in ('Linux/arm/include', 'include')]
    subprocess.run(cmd + [str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)] + shlex.split(os.environ.get('BIO_LIFECYCLE_CASE', '')),
                   check=True, timeout=15)
