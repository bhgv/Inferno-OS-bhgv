#!/usr/bin/env python3
"""Run the real stat/path and FD creation matrices across packed frames and callbacks."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
environment = dict(os.environ)
environment.setdefault('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
families = environment.get('SYS_QUERY_FAMILIES', 'query nativefd query-preflight creation creation-preflight').split()
commands = {
    'query': ('sys-stat-slots.py', {'SYS_STAT_CAPTURE': '1'}),
    'nativefd': ('sys-stat-slots.py', {'SYS_STAT_FAMILY': 'nativefd'}),
    'query-preflight': ('sys-stat-slots.py', {'SYS_STAT_FAMILY': 'preflight'}),
    'creation': ('sys-fd-owners.py', {'FD_OWNER_CAPTURE': '1', 'FD_OWNER_CASE': 'files'}),
    'creation-preflight': ('sys-fd-owners.py', {'FD_OWNER_PREFLIGHT': '1'}),
}
if not families or any(family not in commands for family in families):
    raise ValueError('unknown query/creation family')
for family in families:
    driver, selection = commands[family]
    subprocess.run([sys.executable, str(ROOT/'libinterp/tests'/driver)], cwd=ROOT,
                   env=dict(environment, **selection), check=True)
