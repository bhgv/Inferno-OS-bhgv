#!/usr/bin/env python3
"""Compare production compiler exception analysis with independent set-valued AST oracles."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('RAISES_SOURCE', root / 'limbo/typecheck.c')).read_text()
start = source.index('typedef struct Elist Elist;')
end = source.index('/* sort all globals', start)
unit = (root / 'limbo/tests/raises.c').read_text().replace('IMPLEMENTATION', source[start:end])
with tempfile.TemporaryDirectory(prefix='compiler-raises-') as directory:
    target = Path(directory) / 'raises.c'
    target.write_text(unit)
    binary = Path(directory) / 'raises'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'limbo')]
    subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
    subprocess.run([str(binary)]+os.environ.get('RAISES_CASE', '').split(), check=True, timeout=30)
