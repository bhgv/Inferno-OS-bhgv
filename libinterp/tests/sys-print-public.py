#!/usr/bin/env python3
"""Check public print frame snapshots, owner spans and allocation callbacks."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]  # Owning VM tree.
environment = dict(os.environ)  # Preserve compiler and previous-source replay choices.
environment['SYS_PRINT_EXTRA'] = str(ROOT/'libinterp/tests/sys-print-public.c')
environment.setdefault('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
families = [environment['SYS_PRINT_CASE']] if environment.get('SYS_PRINT_CASE') else ['public', 'preflight']
if any(value not in ('public', 'preflight') for value in families):
    raise ValueError('unknown public formatter family')
for name, choices in (('PRINT_PUBLIC_OP', ('0', '1')), ('PRINT_PUBLIC_BORROW', ('0', '1')),
                      ('PRINT_PUBLIC_ERROR', tuple(str(value) for value in range(6)))):
    if environment.get(name) is not None and environment[name] not in choices:
        raise ValueError(name)
for family in families:
    subprocess.run([sys.executable, str(ROOT/'libinterp/tests/sys-print.py')], cwd=ROOT,
                   env=dict(environment, SYS_PRINT_CASE=family), check=True)
