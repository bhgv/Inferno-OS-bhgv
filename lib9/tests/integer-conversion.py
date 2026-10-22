#!/usr/bin/env python3
"""Check production signed/unsigned 64-bit conversion across bases 2 through 36."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
library = os.environ.get('NATIVE_LIBRARY', 'lib9')
parts = []
for name in ('strtoll', 'strtoull'):
    source = Path(os.environ.get('INTEGER_'+name.upper()+'_SOURCE', root / (library+'/' + name + '.c'))).read_text()
    source = re.sub(r'#include\s*[<"]lib9.h[>"]', '', source)
    source = source.replace(name + '(', 'test_' + name + '(')
    parts.append(source)
unit = (root / 'lib9/tests/integer-conversion.c').read_text()
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))

with tempfile.TemporaryDirectory(prefix='integer-conversion-') as directory:
    td = Path(directory)
    (td / 'convert.c').write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    flags = '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'
    cmd += shlex.split(os.environ.get('CFLAGS', flags))
    subprocess.run(cmd + [str(td / 'convert.c'), '-o', str(td / 'convert')], check=True)
    subprocess.run([str(td / 'convert')], check=True)
