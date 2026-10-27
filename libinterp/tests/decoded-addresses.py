#!/usr/bin/env python3
"""Exercise the real generated decoder, native pointers and stable unaligned Inst images."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]  # Owning interpreter tree.
env = dict(os.environ, DECODE_TEST_BODY=str(ROOT/'libinterp/tests/decoded-addresses.c'))
subprocess.run([sys.executable,str(ROOT/'libinterp/tests/dis-numeric-literals.py')],env=env,check=True)
