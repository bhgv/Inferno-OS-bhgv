#!/usr/bin/env python3
"""Reject non-pointer pipe arrays before native calls or data-slot inspection."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
environment = dict(os.environ, FD_OWNER_PIPE_TYPES='1')
environment.setdefault('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
subprocess.run([sys.executable, str(ROOT / 'libinterp/tests/sys-fd-owners.py')],
               cwd=ROOT, env=environment, check=True)
