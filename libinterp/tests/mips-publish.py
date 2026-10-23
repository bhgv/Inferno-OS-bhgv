#!/usr/bin/env python3
"""Exercise the entire MIPS compile driver with failed allocations and stub encoders."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('MIPS_PUBLISH_SOURCE', root/'libinterp/comp-mips.c')).read_text()
start = source.index('int\ncompile(Module *m, int size, Modlink *ml)')
# compile is the last definition in this backend.
body = source[start:]
unit = (root/'libinterp/tests/mips-publish.c').read_text().replace('IMPLEMENTATION', body)
with tempfile.TemporaryDirectory(prefix='mips-publish-') as directory:
    td = Path(directory); target = td/'test.c'; target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc')) + shlex.split(os.environ.get(
        'CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    subprocess.run(cmd+includes+[str(target),'-o',str(td/'test')],check=True)
    subprocess.run([str(td/'test')],check=True)
