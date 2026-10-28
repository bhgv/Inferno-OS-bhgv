#!/usr/bin/env python3
"""Check actual SBL serialization, deep Type traversal and metadata rejection."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning compiler and formatter tree.
SOURCE = Path(os.environ.get('SBL_SOURCE', ROOT/'limbo/sbl.c')).read_text()
with tempfile.TemporaryDirectory(prefix='symbol-output-') as temporary:
    work = Path(temporary)
    (work/'borrowed.c').write_text(SOURCE.replace('#include "limbo.h"', '', 1))
    source = work/'output.c'
    source.write_text((ROOT/'limbo/tests/symbol-output.c').read_text().replace('REFERENCE', (ROOT/'limbo/tests/symbol-output-reference.c').read_text()))
    widths = tuple(map(int, os.environ.get("SBL_WORD_BYTES", "4 8").split()))
    if not widths or any(width not in (4, 8) for width in widths):
        raise ValueError("SBL_WORD_BYTES must select 4 and/or 8")
    for width in widths:
        model = work/str(width)
        model.mkdir()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), (ROOT/'include/isa.h').read_text(), count=1))
        binary = model/'output'
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fno-optimize-sibling-calls -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model), '-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo', 'lib9')]
        support = [str(ROOT/'lib9'/(name+'.c')) for name in ('rune', 'dofmt', 'fmt', 'fmtlock', 'errfmt', 'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'vseprint')]
        subprocess.run(command+[str(source)]+support+['-o', str(binary)], check=True)
        families = [os.environ['SBL_CASE']] if os.environ.get('SBL_CASE') else ['shallow', 'sections', 'wrappers', 'functions', 'tuples', 'tags', 'visibility', 'lifetime', 'blocks', 'source', 'vectors', 'counts', 'cardinality', 'blockids', 'instructions']
        for family in families:
            subprocess.run([str(binary), family], check=True, timeout=60)
