#!/usr/bin/env python3
"""Check real stat sizing/packing against independent 16-bit wire limits."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
for library in ('lib9', 'libkern'):
    parts = []
    for part in ('D2M', 'M2D'):
        path = Path(os.environ.get('STAT_'+part+'_SOURCE', root/library/('conv'+part+'.c')))
        parts.append(re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M))
    unit = (root/'lib9/tests/stat-size.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
    with tempfile.TemporaryDirectory(prefix='stat-size-') as directory:
        td = Path(directory); target = td/'test.c'; target.write_text(unit)
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
        subprocess.run(cmd+[str(target), '-o', str(td/'test')], check=True)
        print(library, flush=True)
        subprocess.run([str(td/'test')], check=True, timeout=90)
