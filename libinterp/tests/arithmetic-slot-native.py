#!/usr/bin/env python3
"""Extract real division/branch handlers and check their decoded-slot exception contract."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning Inferno source tree.
SOURCE = Path(os.environ.get('ARITH_SLOT_SOURCE', ROOT / 'libinterp/xec.c')).read_text()  # Committed handlers or baseline override.
OPS = ('divb', 'modb', 'divw', 'modw', 'divl', 'modl', 'beqf', 'bnef', 'bltf', 'blef', 'bgtf', 'bgef')  # Checked paths.


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


unit = (ROOT / 'libinterp/tests/arithmetic-slot-native.c').read_text()
parts = [function('static void\nbranchpc('), function('static void\nbranchreal(')] + [function('OP(' + name + ')') for name in OPS]
unit = unit.replace('IMPLEMENTATION', '\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='arithmetic-slot-native-') as temporary:
    td = Path(temporary)
    source = td / 'fixture.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for wb in (32, 64):
        binary = td / ('word' + str(wb))
        subprocess.run(command + ['-std=c99', '-DWORD_BITS=' + str(wb), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
