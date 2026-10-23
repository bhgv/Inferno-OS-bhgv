#!/usr/bin/env python3
"""Verify production Screen.top/bottom array filtering and allocation bounds."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('DRAW_WINDOWS_SOURCE', root/'libinterp/draw.c')).read_text()
start = source.index('static void\nscreentopbot(') if 'static void\nscreentopbot(' in source else source.index('static\nvoid\nscreentopbot(')
source = source[start:source.index('void\nScreen_top(')]
unit = (root/'libinterp/tests/draw-windows.c').read_text().replace('IMPLEMENTATION', source)
with tempfile.TemporaryDirectory(prefix='draw-windows-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include', 'libinterp')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
