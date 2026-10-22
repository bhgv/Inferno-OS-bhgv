#!/usr/bin/env python3
"""Compare production UTF byte counts with the actual encoder output."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'lib9/rune.c').read_text()
source = source.replace('#include\t"lib9.h"', '')
unit = (root / 'lib9/tests/rune-length.c').read_text()
unit = unit.replace('IMPLEMENTATION', source)

with tempfile.TemporaryDirectory(prefix='rune-length-') as directory:
    td = Path(directory)
    (td / 'runes.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    subprocess.run(cmd + [str(td / 'runes.c'), '-o', str(td / 'runes')], check=True)
    subprocess.run([str(td / 'runes')], check=True)
