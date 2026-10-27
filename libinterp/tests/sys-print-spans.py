#!/usr/bin/env python3
"""Check formatter spans, packed borrowed strings and wide native heap diagnostics."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]  # Owning VM tree.
ENVIRONMENT = dict(os.environ)  # Preserve compiler, source-replay and scalar-model choices.
ENVIRONMENT['SYS_PRINT_EXTRA'] = str(ROOT/'libinterp/tests/sys-print-spans.c')
ENVIRONMENT.setdefault('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
subprocess.run([sys.executable,str(ROOT/'libinterp/tests/sys-print.py')],
               cwd=ROOT,env=ENVIRONMENT,check=True)
