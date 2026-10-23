#!/usr/bin/env python3
"""Exercise production TrueType suffix and cache-key helpers without a display."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('FONT_NAMES_SOURCE', root/'libdraw/getsubfont_ft.c')).read_text()
source = source[source.index('int\nis_subfont_ft'):source.index('Subfont*\nreadsubfont_ft')]
unit = (root/'libdraw/tests/font-names.c').read_text().replace('IMPLEMENTATION', source)
with tempfile.TemporaryDirectory(prefix='font-names-') as directory:
    target = Path(directory)/'test.c'
    target.write_text(unit)
    binary = Path(directory)/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
