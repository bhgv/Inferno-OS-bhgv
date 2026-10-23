#!/usr/bin/env python3
"""Run production native Draw cache hashing and allocation failures."""
import os
from pathlib import Path
import shlex
import re
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('DRAW_CACHE_SOURCE', root/'libinterp/draw.c')).read_text()
source = source[source.index('static int\ndrawhash('):source.index('Image*\nlookupimage(')]
bihash = re.search(r'\bBIHASH\s*=\s*(\d+)', (root/'include/interp.h').read_text()).group(1)
unit = (root/'libinterp/tests/draw-cache.c').read_text().replace('IMPLEMENTATION', source).replace('BIHASH_VALUE', bihash)
with tempfile.TemporaryDirectory(prefix='draw-cache-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    for mode in ('-fsigned-char', '-funsigned-char'):
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        cmd += [mode]+['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
