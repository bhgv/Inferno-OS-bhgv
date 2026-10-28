#!/usr/bin/env python3
"""Execute real markheap with packed descriptors, records and recursive graphs."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning interpreter tree.
SOURCE = Path(os.environ.get('GC_MAP_SOURCE', ROOT/'libinterp/gc.c')).read_text()


# Borrow the actual collector without counting braces in literals or comments.
def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, SOURCE[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return SOURCE[start:brace+token.end()]
    raise RuntimeError(signature)


# Keep native pointers while independently checking both VM scalar widths.
def main():
    unit = (ROOT/'libinterp/tests/gc-maps.c').read_text()
    parts = []
    for signature in ('static int\ngcintadd(', 'static void\ngcspend('):
        if signature in SOURCE:
            parts.append(function(signature))
    parts.append(function('void\nmarkheap('))
    unit = unit.replace('IMPLEMENTATION', '\n\n\n'.join(parts))
    with tempfile.TemporaryDirectory(prefix='gc-maps-') as directory:
        td = Path(directory)
        target = td/'maps.c'
        target.write_text(unit)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        widths = tuple(map(int, os.environ.get('GC_MAP_WORD_BITS', '32 64').split()))
        if not widths or any(bits not in (32, 64) for bits in widths):
            raise ValueError('GC_MAP_WORD_BITS must select 32 and/or 64')
        for bits in widths:
            model = td/str(bits)
            model.mkdir()
            header = (ROOT/'include/interp.h').read_text()
            if bits == 32:
                header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (model/'interp.h').write_text(header)
            includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
            binary = model/'maps'
            subprocess.run(command+includes+[str(target), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


# Explicit predicates remain enabled in release-style sanitizer runs.
if __name__ == '__main__':
    main()
