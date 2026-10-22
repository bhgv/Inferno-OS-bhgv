#!/usr/bin/env python3
"""Run the production integer constant folder under host sanitizers."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'limbo/nodes.c').read_text()

# Include the real folder, with only AST allocation and diagnostics stubbed.
start = source.index('Node*\nfoldc(')
end = source.index('\n/*', start)
body = source[start:end]
ops = list(dict.fromkeys(re.findall(r'case (O\w+):', body))) + ['Oconst']
unit = (root / 'limbo/tests/fold-integers.c').read_text()
unit = unit.replace('OPS', 'enum {' + ','.join(ops) + '};')
unit = unit.replace('BODY', body)

with tempfile.TemporaryDirectory(prefix='fold-integers-') as directory:
    td = Path(directory)
    (td / 'fold.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    subprocess.run(cmd + [str(td / 'fold.c'), '-o', str(td / 'fold')], check=True)
    subprocess.run([str(td / 'fold')], check=True)
