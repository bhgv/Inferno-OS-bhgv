#!/usr/bin/env python3
"""Execute optimizer storage boundaries and actual graph ownership under sanitizers."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Repository containing production definitions.
SOURCE = Path(os.environ.get('OPTIM_STORAGE_SOURCE', ROOT/'limbo/optim.c')).read_text()
CURRENT = (ROOT/'limbo/optim.c').read_text()


# Extract complete definitions while excluding braces inside comments and strings.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


parts = [SOURCE[SOURCE.index('typedef struct Array'):SOURCE.index('Block\tzblock;')]]
parts.append('Block zblock; // Zero-valued production graph template.')
for signature in ('static Bits\nbnew(', 'static Bits*\nallocbits(', 'static int\nbeq(',
                  'static Block*\nmkblock(', 'static Blist*\nmkblist(', 'static void\nleader(',
                  'static Block*\nfindb(', 'static int\nmemb(', 'static int\ncanfallthrough(',
                  'static void\npredsucc(', 'static Block*\nmkblocks(', 'static void\nblfree(',
                  'static void\nbfree(', 'static void\nfreeblks(', 'static int\ninspc(', 'static int\nlen(',
                  'static int\noptpositions('):
    parts.append(function(SOURCE if signature in SOURCE else CURRENT, signature))
unit = (ROOT/'limbo/tests/optimizer-storage.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='optimizer-storage-') as temporary:
    td = Path(temporary)
    source = td/'storage.c'
    source.write_text(unit)
    for width in (4, 8):
        model = td/str(width)
        model.mkdir()
        (model/'isa.h').write_text(re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(width), (ROOT/'include/isa.h').read_text(), count=1))
        binary = model/'storage'
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include', 'limbo')]
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        families = [os.environ['OPTIM_STORAGE_CASE']] if os.environ.get('OPTIM_STORAGE_CASE') else ['ordinary', 'extents', 'bitmap', 'rows', 'positions', 'leaders', 'lookup', 'compare', 'ownership']
        for family in families:
            subprocess.run([str(binary), family], check=True, timeout=60)
