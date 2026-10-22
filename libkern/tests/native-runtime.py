#!/usr/bin/env python3
"""Run native-kernel runtime bodies against independent library regression fixtures."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
environment = dict(os.environ)
environment['NATIVE_LIBRARY'] = 'libkern'
for name in ('integer-conversion', 'rune-length', 'utf-search', 'format-bounds'):
    print('libkern: '+name, flush=True)
    subprocess.run(['python3', str(root / 'lib9/tests' / (name+'.py'))],
                   env=environment, check=True, timeout=120)
