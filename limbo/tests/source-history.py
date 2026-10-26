#!/usr/bin/env python3
"""Exercise production source history growth and coordinate lookup under UBSan."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


# Extract complete functions, ignoring comments and string literals.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace + token.end()]
    raise ValueError(signature)


root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('SOURCE_HISTORY_SOURCE', root/'limbo/lex.c')).read_text()
parts = '\n'.join(function(source, s) for s in ('int\naddfile(', 'Fline\nfline('))
case_source = (root/'limbo/typecheck.c').read_text()
parts += '\n' + function(case_source, 'int\nfindlab(')
with tempfile.TemporaryDirectory(prefix='source-history-') as directory:
    target = Path(directory)/'history.c'
    target.write_text((root/'limbo/tests/source-history.c').read_text().replace('IMPLEMENTATION', parts))
    binary = Path(directory)/'history'
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for flags in ([], ['-DHISTORY_ULONG32']):
        subprocess.run(command + flags + [str(target), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
