#!/usr/bin/env python3
"""Trace process roots and sweep an exception graph through the complete collector."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Compile unchanged collector bodies with a bounded arena and explicit native roots.
def main():
    production = Path(os.environ.get('GC_ROOT_SOURCE', ROOT/'libinterp/gc.c')).read_text()
    production = '\n'.join(line for line in production.splitlines() if not line.startswith('#include'))
    unit = (ROOT/'libinterp/tests/gc-roots.c').read_text().replace('COLLECTOR_IMPLEMENTATION', production)
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'emu/port', 'include')]
    with tempfile.TemporaryDirectory(prefix='gc-roots-') as temporary:
        work = Path(temporary)
        source = work/'roots.c'
        source.write_text(unit)
        for label, selection in (('legacy', []), ('WORD32', ['-DDIS_WORD32=1'])):
            binary = work/label
            subprocess.run(compiler+flags+selection+includes+[str(source), '-o', str(binary)], check=True)
            for family in os.environ.get('GC_ROOT_CASE', 'exception frames').split():
                subprocess.run([str(binary), family], check=True)
            print('PASS GC process roots:', label, flush=True)


if __name__ == '__main__':
    main()
