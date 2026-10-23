#!/usr/bin/env python3
"""Exercise POSIX Dir snapshots, basename rules, NSS strings and allocation failure."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
unit = (root/'lib9/tests/dirstat-owned.c').read_text()
with tempfile.TemporaryDirectory(prefix='dirstat-owned-') as directory:
    for library, mode in [('posix', []), ('Nt', ['-DNT_STAT'])]:
        path = Path(os.environ.get('DIRSTAT_'+library.upper()+'_SOURCE', os.environ.get('DIRSTAT_OWNED_SOURCE', root/'lib9'/('dirstat-'+library+'.c'))))
        source = re.sub(r'^#include[^\n]*', '', path.read_text(), flags=re.M)
        target = Path(directory)/'test.c'; target.write_text(unit.replace('IMPLEMENTATION', source))
        binary = Path(directory)/'test'
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
        cmd += mode+['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
        subprocess.run(cmd+[str(target), '-o', str(binary)], check=True)
        print(library, flush=True)
        subprocess.run([str(binary)], check=True)
