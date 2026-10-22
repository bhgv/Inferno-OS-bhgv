#!/usr/bin/env python3
"""Exercise production Bseek/Boffset/Bflush with exact buffer/position snapshots."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parts = []
for name in ('bseek', 'boffset', 'bflush'):
    source = Path(os.environ.get('BIO_'+name.upper()+'_SOURCE', root / ('libbio/'+name+'.c'))).read_text()
    parts.append(source.replace('#include\t"lib9.h"','').replace('#include\t<bio.h>',''))
unit = (root / 'libbio/tests/seek.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='bio-seek-') as directory:
    target = Path(directory) / 'seek.c'; target.write_text(unit)
    binary = Path(directory) / 'seek'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include','include')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
