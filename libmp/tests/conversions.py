#!/usr/bin/env python3
"""Production multiprecision scalar, byte and text conversions with width models."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
files = ['mptoi', 'mptoui', 'mptov', 'mptouv', 'betomp', 'letomp', 'mptobe', 'mptole']
parts = []
for name in files:
    source = (root / ('libmp/port/' + name + '.c')).read_text()
    source = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
    parts.append(source)
unit = (root / 'libmp/tests/conversions.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='mp-conversions-') as directory:
    td = Path(directory)
    (td / 'convert.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    for width in (32, 64):
        binary = td / ('convert' + str(width))
        subprocess.run(cmd + ['-DDIGIT_BITS=' + str(width), '-I' + str(root / 'include'),
                              str(td / 'convert.c'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
