#!/usr/bin/env python3
"""Check real stack destruction, diagnostic scanning and mapped owner cleanup."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract production functions without interpreting braces in comments/strings."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:return source[start:brace+token.end()]
    raise ValueError(signature)


xec = Path(os.environ.get('UNWIND_XEC_SOURCE', ROOT/'libinterp/xec.c')).read_text()
valid = Path(os.environ.get('UNWIND_VALID_SOURCE', ROOT/'libinterp/validstk.c')).read_text()
check = Path(os.environ.get('UNWIND_CHECK_SOURCE', ROOT/'libinterp/validstk.c')).read_text()
heap = (ROOT/'libinterp/heap.c').read_text()
parts = []
if 'static void\nunwindstate(' in xec:
    parts += [function(xec, 'static void\nunwindstate(')]
parts += [function(heap, 'void\nfreeptrs('), function(check, 'void\nmemchk('),
         function(valid, 'void\nvalidstk('), function(xec, 'void\ndestroystack(')]
unit = (ROOT/'libinterp/tests/stack-unwind.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='stack-unwind-') as directory:
    td = Path(directory)
    source = td/'unwind.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    widths=tuple(map(int,os.environ.get('UNWIND_WORD_BITS','64 32').split()))
    if not widths or any(bits not in (32,64) for bits in widths):
        raise ValueError('UNWIND_WORD_BITS must select 32 and/or 64')
    for bits in widths:
        model = td/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header = '#define DIS_WORD32 1 // Select the production experimental scalar model.\n'+header
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('unwind-'+str(bits))
        subprocess.run(command+includes+[str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
