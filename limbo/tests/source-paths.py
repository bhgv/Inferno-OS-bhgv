#!/usr/bin/env python3
"""Run production root/source path helpers with immutable environment fixtures."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Extract complete bodies without treating braces in strings as C syntax.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


unit = (root / 'limbo/tests/source-paths.c').read_text()
clean = (root / 'lib9/cleanname.c').read_text().replace('#include "lib9.h"', '')
bodies = []
with tempfile.TemporaryDirectory(prefix='source-paths-') as directory:
    td = Path(directory)
    for filename in ('limbo.y', 'y.tab.c'):
        source = Path(os.environ.get('SOURCE_PATHS_SOURCE', root / 'limbo' / filename)).read_text()
        body = '\n'.join(function(source, sig) for sig in ('static char*\ngetroot(',
                         'static char *\nwin2inf(', 'static char *\ncleann(', 'char *\nsrcpath('))
        bodies.append(body)
        target = td / 'paths.c'
        target.write_text(unit.replace('IMPLEMENTATION', clean+'\n'+body))
        for mode in ('-fsigned-char', '-funsigned-char'):
            cmd = shlex.split(os.environ.get('CC', 'cc'))
            cmd += shlex.split(os.environ.get('CFLAGS',
                               '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
            subprocess.run(cmd + [mode, str(target), '-o', str(td / 'test')], check=True)
            subprocess.run([str(td / 'test')], check=True, timeout=15)
    assert bodies[0] == bodies[1], 'grammar and generated parser path helpers differ'
