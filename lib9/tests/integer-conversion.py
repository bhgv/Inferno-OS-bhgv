#!/usr/bin/env python3
"""Check production signed/unsigned 64-bit conversion across bases 2 through 36."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parts = []
for name in ('strtoll', 'strtoull'):
    source = (root / ('lib9/' + name + '.c')).read_text()
    source = source.replace('#include "lib9.h"', '')
    source = source.replace(name + '(', 'test_' + name + '(')
    parts.append(source)
unit = (root / 'lib9/tests/integer-conversion.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='integer-conversion-') as directory:
    td = Path(directory)
    (td / 'convert.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    subprocess.run(cmd + [str(td / 'convert.c'), '-o', str(td / 'convert')], check=True)
    subprocess.run([str(td / 'convert')], check=True)
