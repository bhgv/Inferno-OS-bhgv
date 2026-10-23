#!/usr/bin/env python3
"""Exercise the production Rune-string counter on short and genuine LP64 spans."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='runestrlen-width-') as directory:
    for library in ('lib9', 'libkern'):
        binary = Path(directory) / library
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include')]
        source = Path(os.environ.get('RUNESTRLEN_SOURCE', root / library / 'runestrlen.c'))
        subprocess.run(cmd+[str(root / 'lib9/tests/runestrlen-width.c'), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+(['wide'] if os.environ.get('RUNESTRLEN_WIDE') == '1' else []), check=True, timeout=120)
