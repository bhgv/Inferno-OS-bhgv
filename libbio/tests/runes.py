#!/usr/bin/env python3
"""Run production buffered rune input/output and the UTF codec across refills."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parts = []
for path in ('lib9/rune.c', 'libbio/bgetc.c', 'libbio/bgetrune.c', 'libbio/bputrune.c'):
    source = (root / path).read_text()
    source = source.replace('#include\t"lib9.h"', '')
    source = source.replace('#include\t<bio.h>', '')
    parts.append(source)
unit = (root / 'libbio/tests/runes.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='bio-runes-') as directory:
    td = Path(directory)
    (td / 'runes.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    subprocess.run(cmd + ['-I' + str(root / 'include'), str(td / 'runes.c'),
                          '-o', str(td / 'runes')], check=True)
    subprocess.run([str(td / 'runes')], check=True)
