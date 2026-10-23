#!/usr/bin/env python3
"""Exercise complete portable memory/search bodies without replacing host libc."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='portable-memory-') as directory:
    td = Path(directory)
    for variant in ('strchr', 'strchr-spim'):
        parts = []
        for name in ('memmove', 'memset', 'strncpy', variant, 'strrchr'):
            path = Path(os.environ.get('PORTABLE_' + name.upper().replace('-', '_') + '_SOURCE',
                                      root / 'libkern' / (name + '.c')))
            source = path.read_text()
            for inc in ('#include\t<lib9.h>', '#include <lib9.h>'):
                source = source.replace(inc, '')
            parts.append(source)
        unit = (root / 'libkern/tests/portable-memory.c').read_text()
        target = td / (variant + '.c')
        target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
        for sign in ('signed', 'unsigned'):
            binary = td / (variant + '-' + sign)
            cmd = shlex.split(os.environ.get('CC', 'cc'))
            cmd += shlex.split(os.environ.get('CFLAGS',
                               '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            cmd += ['-f' + sign + '-char', '-fno-builtin']
            cmd += ['-I' + str(root / p) for p in ('Linux/arm/include', 'include')]
            subprocess.run(cmd + [str(target), '-o', str(binary)], check=True)
            args = shlex.split(os.environ.get('PORTABLE_CASE', ''))
            subprocess.run([str(binary)] + args, check=True, timeout=120)
            if os.environ.get('PORTABLE_WIDE') == '1' and sign == 'signed' and variant == 'strchr':
                subprocess.run([str(binary), 'wide'], check=True, timeout=120)
