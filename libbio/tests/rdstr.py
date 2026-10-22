#!/usr/bin/env python3
"""Exercise production Brdstr under moving allocation, short reads and faults."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('BIO_RDSTR_SOURCE', root / 'libbio/brdstr.c')).read_text()
source = source.replace('#include\t"lib9.h"', '').replace('#include\t<bio.h>', '')
unit = (root / 'libbio/tests/rdstr.c').read_text().replace('IMPLEMENTATION', source)
with tempfile.TemporaryDirectory(prefix='bio-rdstr-') as directory:
    target = Path(directory) / 'rdstr.c'
    target.write_text(unit)
    binary = Path(directory) / 'rdstr'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
