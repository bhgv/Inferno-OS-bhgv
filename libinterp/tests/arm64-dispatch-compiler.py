#!/usr/bin/env python3
"""Keep ARM64 complete-entry binding/preflight in the default host checkpoint."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]  # Owning interpreter source tree.
ENVIRONMENT = dict(os.environ)  # Preserve explicit compiler and sanitizer choices.
ENVIRONMENT.update(BRIDGE_BACKEND='arm64',BRIDGE_PREFLIGHT_ONLY='1')
subprocess.run([sys.executable,str(ROOT/'libinterp/tests/jit-dispatch-native.py')],
               cwd=ROOT,env=ENVIRONMENT,check=True)
