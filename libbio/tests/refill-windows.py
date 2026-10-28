#!/usr/bin/env python3
"""Exercise real Bio pushback/line windows and native I/O result widths."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parts = []
for name in ('bgetc', 'bgetrune', 'bread', 'brdline', 'brdstr', 'bwrite'):
    source = Path(os.environ.get('BIO_REFILL_'+name.upper(), ROOT/('libbio/'+name+'.c'))).read_text()
    parts.append(re.sub(r'^#include[^\n]*\n', '', source, flags=re.M))
with tempfile.TemporaryDirectory(prefix='bio-refill-windows-') as temporary:
    work = Path(temporary)
    source = work/'windows.c'
    binary = work/'windows'
    source.write_text((ROOT/'libbio/tests/refill-windows.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include')]
    subprocess.run(command+[str(source), str(ROOT/'lib9/rune.c'), '-o', str(binary)], check=True)
    for family in ([os.environ['BIO_REFILL_CASE']] if os.environ.get('BIO_REFILL_CASE') else ['line', 'string', 'getc', 'read', 'rdline', 'rdstr', 'write']):
        subprocess.run([str(binary), family], check=True)
