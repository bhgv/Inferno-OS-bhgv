#!/usr/bin/env python3
"""Check NT positional reads and both native seek dispatch paths."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
def source(name, default):
    return re.sub(r'^#include[^\n]*', '', Path(os.environ.get(name, root/default)).read_text(), flags=re.M)
unit = (root/'lib9/tests/positional-io.c').read_text()
unit = unit.replace('PREAD_IMPLEMENTATION', source('POSITIONAL_PREAD_SOURCE', 'lib9/pread-Nt.c'))
unit = unit.replace('SEEK_IMPLEMENTATION', source('POSITIONAL_SEEK_SOURCE', 'lib9/seek.c'))
with tempfile.TemporaryDirectory(prefix='positional-io-') as directory:
    target = Path(directory)/'test.c'
    target.write_text(unit)
    for windows in (False, True):
        binary = Path(directory)/'test'
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        if windows: cmd += ['-D_WIN32']
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
