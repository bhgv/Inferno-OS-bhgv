#!/usr/bin/env python3
"""Verify native Fmt short-write recovery and bounded Android logging."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
path = Path(os.environ.get('FMT_FILE_IO_SOURCE', root/'lib9/vfprint.c'))
source = path.read_text()
source = source[source.index('int\n_fmtFdFlush('):source.index('int\nvfprint(')]
init = Path(os.environ.get('FMT_FILE_INIT_SOURCE', root/'lib9/fmtfd.c')).read_text()
init = re.sub(r'^#include[^\n]*', '', init, flags=re.M)
unit = (root/'lib9/tests/fmt-file-io.c').read_text().replace('IMPLEMENTATION', source+'\n'+init)
with tempfile.TemporaryDirectory(prefix='fmt-file-io-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    for mode in ([], ['-DANDROID']):
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        cmd += mode+['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
