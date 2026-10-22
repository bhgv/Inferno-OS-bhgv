#!/usr/bin/env python3
"""Exercise each real Styx codec with valid frames and invalid size/count fields."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
for library in ('lib9', 'libkern', 'emu'):
    if library == 'emu':
        paths = [Path(os.environ.get('STYX_EMU_SOURCE', root/'emu/port/styx.c'))]
    else:
        paths = [Path(os.environ.get('STYX_'+part.upper()+'_SOURCE', root/library/('conv'+part+'.c')))
                 for part in ('M2S', 'S2M')]
    bodies = '\n'.join(re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M) for path in paths)
    unit = (root/'lib9/tests/styx-bounds.c').read_text().replace('IMPLEMENTATION', bodies)
    with tempfile.TemporaryDirectory(prefix='styx-bounds-') as directory:
        td = Path(directory); target = td/'test.c'; target.write_text(unit)
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
        subprocess.run(cmd+[str(target), '-o', str(td/'test')], check=True)
        print(library, flush=True)
        subprocess.run([str(td/'test')], check=True, timeout=90)
