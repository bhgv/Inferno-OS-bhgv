#!/usr/bin/env python3
"""Run complete legacy compile drivers with a guarded terminal-PC lookup encoder."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
unit = (root / 'libinterp/tests/legacy-jit-fallthrough.c').read_text()
with tempfile.TemporaryDirectory(prefix='legacy-jit-fallthrough-') as directory:
    td = Path(directory)
    for architecture in ('mips', 'power', 'sparc'):
        source = Path(os.environ.get('LEGACY_JIT_SOURCE', root / ('libinterp/comp-'+architecture+'.c'))).read_text()
        start = source.index('int\ncompile(Module *m, int size, Modlink *ml)')
        target = td / (architecture+'.c')
        target.write_text(unit.replace('IMPLEMENTATION', source[start:]))
        binary = td / (architecture+'-test')
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS',
                           '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        print(architecture, flush=True)
        subprocess.run([str(binary)], check=True, timeout=15)
