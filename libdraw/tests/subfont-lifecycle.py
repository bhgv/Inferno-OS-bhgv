#!/usr/bin/env python3
"""Run production subfont read/write/cache ownership with injected failures."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
parts = []
for name in ('readsubfont', 'subfont', 'freesubfont', 'subfontcache', 'writesubfont'):
    path = Path(os.environ.get(name.upper()+'_SOURCE', root/'libdraw'/(name+'.c')))
    parts.append(re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M))
s = (root/'libdraw/defont.c').read_text()
parts.append(s[s.index('void\n_unpackinfo'):])
s = Path(os.environ.get('FONT_SOURCE', root/'libdraw/font.c')).read_text()
parts.append(s[s.index('static Subfont*\ncf2subfont'):s.index('/* return 1 if load succeeded')])
unit = (root/'libdraw/tests/subfont-lifecycle.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='subfont-lifecycle-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
