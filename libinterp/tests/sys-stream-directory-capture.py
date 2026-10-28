#!/usr/bin/env python3
"""Run stream and directory capture, native count boundaries and preflight matrices."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
environment = dict(os.environ)
environment.setdefault('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
commands = {
    'stream': ('sys-stream.py', {'SYS_STREAM_CASE': 'capture'}),
    'stream-fd': ('sys-stream.py', {'SYS_STREAM_CASE': 'nativefd', 'STREAM_PREFLIGHT': '1'}),
    'directory': ('sys-dirread.py', {'DIRREAD_CAPTURE': '1'}),
    'directory-fd': ('sys-dirread.py', {'DIRREAD_FAMILY': 'nativefd'}),
    'directory-count': ('sys-dirread.py', {'DIRREAD_FAMILY': 'nativecount'}),
    'directory-preflight': ('sys-dirread.py', {'DIRREAD_FAMILY': 'preflight'}),
}
families = environment.get('STREAM_DIRECTORY_FAMILIES', ' '.join(commands)).split()
if not families or any(family not in commands for family in families):
    raise ValueError('unknown stream/directory family')
for family in families:
    driver, selection = commands[family]
    subprocess.run([sys.executable, str(ROOT/'libinterp/tests'/driver)], cwd=ROOT,
                   env=dict(environment, **selection), check=True)
