#!/usr/bin/env python3
"""Check real stack copying, extent release and transactional invalid-owner handling."""
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


stack = Path(os.environ.get('LIFECYCLE_SOURCE', ROOT/'libinterp/stack.c')).read_text()
heap = (ROOT/'libinterp/heap.c').read_text()
signatures = ('static Stkext*\nstackextent(', 'static Type*\nstackframe(', 'static int\nstacksize(', 'static ulong\nstackhint(', 'void\nnewstack(',
              'void\nextend(', 'void\nunextend(', 'void\nunframe(')
parts = [function(heap, name) for name in ('void\ninitmem(', 'void\nfreeptrs(')]
parts += [function(stack, name) for name in signatures if name in stack]
unit = Path(os.environ.get('LIFECYCLE_FIXTURE',ROOT/'libinterp/tests/stack-lifecycle.c')).read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='stack-lifecycle-') as directory:
    td = Path(directory)
    source = td/'lifecycle.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    widths=tuple(map(int,os.environ.get("LIFECYCLE_WORD_BITS","64 32").split()))
    if not widths or any(bits not in (32,64) for bits in widths):
        raise ValueError("LIFECYCLE_WORD_BITS must select 32 and/or 64")
    for bits in widths:
        model = td/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('lifecycle-'+str(bits))
        subprocess.run(command+includes+[str(source), '-o', str(binary)], check=True)
        families = [os.environ['LIFECYCLE_CASE']] if os.environ.get('LIFECYCLE_CASE') else ['copy','pop','extent','borrow','invalid']
        for family in families:
            subprocess.run([str(binary), family], check=True)
