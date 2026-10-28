#!/usr/bin/env python3
"""Run complete pipe/connection capture, owner preflight and array-only callbacks."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
environment = dict(os.environ)
environment.setdefault('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
commands = {
    'pipe': {'FD_OWNER_MULTI': '1', 'FD_OWNER_CASE': 'pipe'},
    'connection': {'FD_OWNER_MULTI': '1', 'FD_OWNER_CASE': 'connection'},
    'preflight': {'FD_OWNER_MULTI_PREFLIGHT': '1'},
    'array': {'FD_OWNER_MULTI': '1', 'FD_OWNER_CASE': 'pipe', 'FD_OWNER_KEEP_FRAME': '1',
              'FD_OWNER_PHASE': '3', 'FD_OWNER_FRAME': '0', 'FD_OWNER_ARRAY_SHIFT': '0'},
}
families = environment.get('SYS_MULTI_FAMILIES', ' '.join(commands)).split()
if not families or any(family not in commands for family in families):
    raise ValueError('unknown multi-owner capture family')
for family in families:
    subprocess.run([sys.executable, str(ROOT/'libinterp/tests/sys-fd-owners.py')], cwd=ROOT,
                   env=dict(environment, **commands[family]), check=True)
