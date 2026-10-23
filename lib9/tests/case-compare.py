#!/usr/bin/env python3
"""Check both native ASCII comparison implementations with both char signs."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='case-compare-') as directory:
    td = Path(directory)
    for library in ('lib9', 'libkern'):
        sources = []
        for name in ('cistrcmp', 'cistrncmp'):
            path = Path(os.environ.get('CASE_' + library.upper() + '_' + name.upper(),
                                      root / library / (name + '.c')))
            source = path.read_text()
            source = source.replace('#include "kern.h"', '#include "lib9.h"')
            target = td / (name + '.c')
            target.write_text(source)
            sources.append(str(target))
        for sign in ('signed', 'unsigned'):
            binary = td / ('compare-' + library + '-' + sign)
            cmd = shlex.split(os.environ.get('CC', 'cc'))
            cmd += shlex.split(os.environ.get('CFLAGS',
                               '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            cmd += ['-f' + sign + '-char']
            cmd += ['-I' + str(root / p) for p in ('Linux/arm/include', 'include')]
            subprocess.run(cmd + [str(root / 'lib9/tests/case-compare.c')] +
                           sources + ['-o', str(binary)], check=True)
            subprocess.run([str(binary)] + shlex.split(os.environ.get('CASE_COMPARE_CASE', '')),
                           check=True, timeout=15)
