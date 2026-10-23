#!/usr/bin/env python3
"""Exercise production translation-unit cleanup with injected stream failures."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Extract production functions without interpreting braces in quoted text.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


unit = (root / 'limbo/tests/translation-io.c').read_text()
with tempfile.TemporaryDirectory(prefix='translation-io-') as directory:
    td = Path(directory)
    bodies = []
    for name in ('limbo.y', 'y.tab.c'):
        source = Path(os.environ.get('TRANSLATE_SOURCE', root / 'limbo' / name)).read_text()
        body = function(source, 'void\ntranslate(')
        body += '\n' + function(source, 'static char *\nwin2inf(')
        bodies.append(body)
        target = td / 'translate.c'
        target.write_text(unit.replace('IMPLEMENTATION', body))
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS',
                           '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        subprocess.run(cmd + [str(target), '-o', str(td / 'test')], check=True)
        subprocess.run([str(td / 'test')], check=True, timeout=15)
    assert bodies[0] == bodies[1], 'grammar and generated parser translation drivers differ'
