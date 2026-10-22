#!/usr/bin/env python3
"""Check the real wire macros and both libraries' stat conversion bodies."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='wire-integers-') as directory:
    td = Path(directory)
    for header_name in ('fcall', 'styx'):
        header = Path(os.environ.get('WIRE_'+header_name.upper()+'_SOURCE',
                                     root/'include'/(header_name+'.h')))
        # Each header defines the same protocol interface; compile independently.
        (td/'fcall.h').write_text(header.read_text())
        for library in ('lib9', 'libkern'):
            bodies = '\n'.join((root/library/(name+'.c')).read_text()
                               for name in ('convD2M', 'convM2D'))
            unit = (root/'lib9/tests/wire-integers.c').read_text().replace('IMPLEMENTATION', bodies)
            # fcall.h follows the project's single-include convention.
            unit = unit.replace('#include\t"fcall.h"', '')
            target = td/'test.c'; target.write_text(unit)
            cmd = shlex.split(os.environ.get('CC', 'cc'))
            cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            cmd += ['-I'+str(root/p) for p in ('Linux/arm/include', 'include')]
            subprocess.run(cmd+[str(target), '-o', str(td/'test')], check=True)
            print(header_name, library, flush=True)
            subprocess.run([str(td/'test')], check=True)
