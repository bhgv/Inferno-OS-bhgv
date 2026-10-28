#!/usr/bin/env python3
"""Exercise packed pctl frames/lists, native callbacks and side-effect-free rejection."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

for family in ('capture', 'preflight'):
    environment = dict(os.environ, PCTL_CAPTURE='1')
    if family == 'preflight':
        environment['PCTL_PREFLIGHT'] = '1'
    print('pctl '+family, flush=True)
    subprocess.run([sys.executable, str(ROOT / 'libinterp/tests/sys-pctl.py')],
                   cwd=ROOT, env=environment, check=True)
