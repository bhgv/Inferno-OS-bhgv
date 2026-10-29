#!/usr/bin/env python3
"""Exercise the real VM fetch loop, native AMD64 JIT and decoder in both WORD models."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]  # Owning interpreter and compiler fixture.
environment = dict(os.environ)  # Keep explicit compiler, parent-source and test-family selections.
environment['BRIDGE_FETCH_TEST'] = '1'
environment['BRIDGE_BACKEND'] = 'amd64'
environment.setdefault('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
subprocess.run([sys.executable, str(ROOT/'libinterp/tests/jit-dispatch-native.py')], cwd=ROOT, env=environment, check=True)
