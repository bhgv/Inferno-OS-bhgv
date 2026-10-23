#!/usr/bin/env python3
"""Compare native substring helpers with host libc, including a real wide needle."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='native-substrings-') as directory:
    td = Path(directory)
    for library in ('lib9', 'libkern'):
        parts = []
        specs = [(library, 'cistrstr'), (library, 'cistrncmp')]
        specs += [('libkern', n) for n in ('strstr', 'strlen', 'strchr', 'strncmp')]
        for lib, name in specs:
            path = Path(os.environ.get('SUBSTRING_' + lib.upper() + '_' + name.upper(),
                                      root / lib / (name + '.c')))
            source = path.read_text()
            for inc in ('#include "lib9.h"', '#include <lib9.h>'):
                source = source.replace(inc, '')
            parts.append(source)
        unit = (root / 'lib9/tests/substrings.c').read_text()
        target = td / (library + '.c')
        target.write_text(unit.replace('IMPLEMENTATION', '\n'.join(parts)))
        for sign in ('signed', 'unsigned'):
            binary = td / (library + '-' + sign)
            cmd = shlex.split(os.environ.get('CC', 'cc'))
            cmd += shlex.split(os.environ.get('CFLAGS',
                               '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            cmd += ['-f' + sign + '-char', '-fno-builtin']
            cmd += ['-I' + str(root / p) for p in ('Linux/arm/include', 'include')]
            subprocess.run(cmd + [str(target), '-o', str(binary)], check=True)
            args = shlex.split(os.environ.get('SUBSTRING_CASE', ''))
            subprocess.run([str(binary)] + args, check=True, timeout=120)
            if os.environ.get('SUBSTRING_WIDE') == '1' and not args and sign == 'signed':
                subprocess.run([str(binary), 'wide'], check=True, timeout=120)
