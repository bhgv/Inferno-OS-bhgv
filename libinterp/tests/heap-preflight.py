#!/usr/bin/env python3
"""Run heap construction/clone span checks through production ownership helpers."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]  # Owning interpreter source tree.
ENVIRONMENT = dict(os.environ)  # Preserve explicit compiler and parent-source choices.
ENVIRONMENT['HEAP_CLONE_EXTRA'] = str(ROOT/'libinterp/tests/heap-preflight.c')
ENVIRONMENT.setdefault('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
subprocess.run([sys.executable,str(ROOT/'libinterp/tests/heap-clones.py')],
               cwd=ROOT,env=ENVIRONMENT,check=True)
