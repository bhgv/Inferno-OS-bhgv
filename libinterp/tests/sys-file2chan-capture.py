#!/usr/bin/env python3
"""Verify captured file2chan frames, old owners and early rejection under VM errors."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

for family in ('capture', 'preflight'):
    environment = dict(os.environ, FILE2CHAN_CAPTURE='1')
    if family == 'preflight':
        environment['FILE2CHAN_PREFLIGHT'] = '1'
    print('file2chan '+family, flush=True)
    subprocess.run([sys.executable, str(ROOT / 'libinterp/tests/sys-file2chan.py')],
                   cwd=ROOT, env=environment, check=True)
