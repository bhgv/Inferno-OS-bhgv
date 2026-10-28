#!/usr/bin/env python3
"""Check real allocated formatters with moving allocations, failures and Rune spans."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='allocated-format-') as temporary:
    work = Path(temporary)
    for library in os.environ.get('ALLOC_FMT_LIBRARIES', 'lib9 libkern').split():
        if library not in ('lib9', 'libkern'):
            raise ValueError(library)
        parts = []
        for name in ('vsmprint', 'fmtstr', 'smprint', 'runesmprint'):
            default = ROOT/library/(name+'.c')
            if not default.exists():
                default = ROOT/'lib9'/(name+'.c')
            path = Path(os.environ.get('ALLOC_FMT_'+name.upper(), default))
            parts.append(re.sub(r'^#include[^\n]*\n', '', path.read_text(), flags=re.M))
        source = work/(library+'.c')
        binary = work/library
        source.write_text((ROOT/'lib9/tests/allocated-format.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
        names = ('rune', 'dofmt', 'fmt', 'fmtlock', 'errfmt', 'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'runeseprint', 'runevseprint', 'runesnprint')
        support = []
        for name in names:
            path = ROOT/library/(name+'.c')
            if not path.exists():
                path = ROOT/'lib9'/(name+'.c')
            if name == 'runevseprint':
                path = Path(os.environ.get('ALLOC_FMT_RUNEVSEPRINT', path))
            support.append(str(path))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'lib9')]
        subprocess.run(command+[str(source)]+support+['-o', str(binary)], check=True)
        for family in ([os.environ['ALLOC_FMT_CASE']] if os.environ.get('ALLOC_FMT_CASE') else ['growth', 'failures', 'views', 'flush', 'rune', 'runealloc']):
            print(library+' '+family, flush=True)
            subprocess.run([str(binary), family], check=True)
