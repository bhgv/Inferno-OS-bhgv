#!/usr/bin/env python3
"""Execute Tk ownership and quote paths with actual records and deterministic unlock interleavings."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(text, signature):
    """Extract complete C functions without counting comment or literal braces."""
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


source = Path(os.environ.get('TK_OWNER_SOURCE', root / 'libinterp/tk.c')).read_text()
strings = (root / 'libinterp/string.c').read_text()
runt = (root / 'libinterp/runt.c').read_text()
parts = [function(strings, 'String*\n'+name+'(') for name in ('newstring','newrunes','c2string')]
parts += [function(strings, 'char*\nstring2c('), function(runt, 'void\nretstr(')]
parts += [function((root / 'emu/port/dis.c').read_text(), 'int\nkprotect(')]
parts += [function(source, 'void\n'+name+'(') for name in ('Tk_toplevel','Tk_namechan','Tk_quote')]
unit = (root / 'libinterp/tests/tk-owners.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='tk-owners-') as directory:
    td = Path(directory)
    target = td / 'owners.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        includes = []
        if bits == 32:
            model = td / 'word32'
            model.mkdir(exist_ok=True)
            header = (root / 'include/interp.h').read_text()
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (model / 'interp.h').write_text(header)
            includes = ['-I'+str(model)]
        includes += ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td / 'owners'
        subprocess.run(cmd+includes+[str(target),str(root / 'lib9/rune.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)]+os.environ.get('TK_OWNER_CASE','').split(),check=True)
        print('WORD'+str(bits), flush=True)
