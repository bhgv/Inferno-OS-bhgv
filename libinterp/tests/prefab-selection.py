#!/usr/bin/env python3
"""Run production Prefab tag traversal and selection setup under allocation faults."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('PREFAB_SELECTION_SOURCE', root/'libinterp/prefab.c')).read_text()
start = source.index('static int\ntagspace(') if 'static int\ntagspace(' in source else source.index('static\nPElement**\ntags(')
source = source[start:source.index('void\nCompound_tagselect(')]
unit = (root/'libinterp/tests/prefab-selection.c').read_text().replace('IMPLEMENTATION', source)
with tempfile.TemporaryDirectory(prefix='prefab-selection-') as directory:
    target = Path(directory)/'test.c'; target.write_text(unit)
    binary = Path(directory)/'test'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
