#!/usr/bin/env python3
"""Run production legacy JIT entry checks before any encoder or allocator executes."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
unit = (root / 'libinterp/tests/legacy-jit-inputs.c').read_text()
with tempfile.TemporaryDirectory(prefix='legacy-jit-inputs-') as directory:
    td = Path(directory)
    includes = ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
    for architecture in ('mips', 'power', 'sparc'):
        source = Path(os.environ.get('LEGACY_JIT_SOURCE', root / ('libinterp/comp-'+architecture+'.c'))).read_text()
        start = source.index('int\ncompile(Module *m, int size, Modlink *ml)')
        boundary = '\t/* Input is valid; the legacy compiler may now acquire working storage. */'
        if boundary in source[start:]:
            prefix = source[start:source.index(boundary, start)]
        else:
            # Old compilers started mutating globals immediately after declarations.
            marker = '\tpatch = mallocz' if architecture == 'mips' else '\tbase = nil;'
            prefix = source[start:source.index(marker, start)]
        body = prefix + '\treturn 1;\n}\n'
        target = td / (architecture+'.c')
        target.write_text(unit.replace('IMPLEMENTATION', body))
        for model in ('native', 'word32'):
            override = []
            if model == 'word32':
                modeldir = td / (architecture+'-word32')
                modeldir.mkdir()
                header = (root / 'include/interp.h').read_text()
                header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                assert n == m == 1
                (modeldir/'interp.h').write_text(header)
                override = ['-I'+str(modeldir)]
            cmd = shlex.split(os.environ.get('CC', 'cc'))
            cmd += shlex.split(os.environ.get('CFLAGS',
                               '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            binary = td / (architecture+'-'+model+'-test')
            subprocess.run(cmd+override+includes+[str(target), '-o', str(binary)], check=True)
            print(architecture, model, flush=True)
            subprocess.run([str(binary)], check=True, timeout=15)
