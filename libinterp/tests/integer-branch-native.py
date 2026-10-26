#!/usr/bin/env python3
"""Extract integer branch handlers and check their decoded-slot exception contract."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning Inferno source tree.
SOURCE = Path(os.environ.get('INTEGER_BRANCH_SOURCE', ROOT / 'libinterp/xec.c')).read_text()  # Committed handlers or baseline override.
OPS = tuple('b' + condition + width for width in ('b', 'w', 'l') for condition in ('eq', 'ne', 'lt', 'le', 'gt', 'ge')) + ('jmp',)  # All nineteen paths.


# Extract one borrowed C definition, ignoring braces in comments and literals.
def function(signature):
    """Return a production definition by signature; reject unmatched C braces.

    SOURCE is borrowed text. Comments and literals cannot alter brace depth;
    no files are modified and extraction errors raise before compilation.
    """
    start = SOURCE.index(signature)
    brace = SOURCE.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', SOURCE[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return SOURCE[start:brace + token.end()]
    raise RuntimeError(signature)


unit = (ROOT / 'libinterp/tests/integer-branch-native.c').read_text()
parts = [function('OP(' + name + ')') for name in OPS]
unit = unit.replace('IMPLEMENTATION', '\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='integer-branch-native-') as temporary:
    td = Path(temporary)
    source = td / 'fixture.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (32, 64):
        binary = td / ('word' + str(wb))
        subprocess.run(command + ['-I' + str(ROOT / 'include'), '-I' + os.environ.get('INTEGER_BRANCH_EMITTER_DIR', str(ROOT / 'libinterp')), '-D_GNU_SOURCE', '-std=c99', '-DWORD_BITS=' + str(wb), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
