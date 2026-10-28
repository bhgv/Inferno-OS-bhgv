#!/usr/bin/env python3
"""Check actual emu/os print, fprint and werrstr with controlled native I/O."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]  # Owning VM tree.
environment = dict(os.environ)  # Retain source-replay, compiler and scalar selections.
environment['SYS_PRINT_EXTRA'] = str(ROOT/'libinterp/tests/sys-print-io.c')
environment.setdefault('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all')
ports = environment.get('PRINT_IO_PORTS', 'emu os').split()
families = [environment['SYS_PRINT_CASE']] if environment.get('SYS_PRINT_CASE') else ['io', 'preflight', 'nativefd']
if not ports or any(port not in ('emu', 'os') for port in ports):
    raise ValueError('unknown I/O port')
if any(family not in ('io', 'preflight', 'nativefd') for family in families):
    raise ValueError('unknown I/O family')
for name, choices in (('PRINT_IO_OP', ('0', '1', '2')),
                      ('PRINT_IO_ERROR', tuple(str(value) for value in range(7)))):
    if environment.get(name) is not None and environment[name] not in choices:
        raise ValueError(name)
for port in ports:
    for family in families:
        selected = dict(environment, SYS_PRINT_CASE=family,
                        SYS_PRINT_IO_SOURCE=environment.get('SYS_PRINT_IO_SOURCE', str(ROOT/port/'port/inferno.c')),
                        SYS_PRINT_GUARD_SOURCE=str(ROOT/port/'port/dis.c'))
        subprocess.run([sys.executable, str(ROOT/'libinterp/tests/sys-print.py')], cwd=ROOT,
                       env=selected, check=True)
