#!/usr/bin/env python3
"""Check actual emu/os error reads and bounded copies, with optional parent-body replay."""
import os
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SUPPORT = {'__file__': str(ROOT/'libinterp/tests/dis64-runtime.py'), '__name__': 'error_source_support'}
exec(compile((ROOT/'libinterp/tests/dis64-runtime.py').read_bytes(), SUPPORT['__file__'], 'exec'), SUPPORT)
function = SUPPORT['function']


# Keep complete selected owning bodies and exact production public declarations.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/386/include', 'emu/port', 'include')]
    with tempfile.TemporaryDirectory(prefix='error-read-') as temporary:
        work = Path(temporary)
        for port in os.environ.get('ERROR_READ_PORTS', 'emu os').split():
            readpath = ROOT/port/'port'/('errstr.c' if port == 'emu' else 'proc.c')
            copy = Path(os.environ.get('ERROR_COPY_SOURCE', ROOT/port/'port/chan.c')).read_text()
            read = Path(os.environ.get('ERROR_READ_SOURCE', readpath)).read_text()
            bodies = function(copy, 'void\nkstrcpy(')+'\n\n'+function(read, 'void\nkgerrstr(')
            source = work/(port+'.c')
            source.write_text((ROOT/'libinterp/tests/error-read.c').read_text().replace('ERROR_IMPLEMENTATION', bodies))
            binary = work/port
            subprocess.run(compiler+flags+includes+[str(source), '-o', str(binary)], check=True)
            for family in os.environ.get('ERROR_READ_CASES', 'read copy').split():
                subprocess.run([str(binary), family], check=True, cwd=work)
            print('PASS actual error read/copy:', port, flush=True)


if __name__ == '__main__':
    main()
