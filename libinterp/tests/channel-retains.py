#!/usr/bin/env python3
"""Check complete descriptor reservations before creating channel resources."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract real C functions without treating comment or literal braces as syntax.
def function(path, signature):
    selected = ROOT/path
    if path == 'libinterp/xec.c':
        selected = Path(os.environ.get('CHANNEL_CREATE_SOURCE', selected))
    source = selected.read_text()
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
    raise ValueError(signature)


parts = [function('emu/port/dis.c', 'int\nkprotect('), function('emu/port/dis.c', 'void\nkrethrow(')]
parts += [function('libinterp/heap.c', result+'\n'+name+'(') for name, result in
          [('dtype','Type*'), ('freetype','void'), ('acheck','void'), ('initmem','void'),
           ('freeptrs','void'), ('nheap','Heap*'), ('heap','Heap*'), ('heaparray','Heap*'),
           ('initarray','void'), ('freechan','void'), ('freearray','void')]]
source = Path(os.environ.get('CHANNEL_CREATE_SOURCE', ROOT/'libinterp/xec.c')).read_text()
parts += [source[source.index('typedef struct ChannelBuild'):source.index('OP(newcl)')],
          source[source.index('typedef struct RawChannel'):source.index('OP(newcmp)')]]
unit = (ROOT/'libinterp/tests/channels.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
unit = unit.replace('int main(void)', 'int channelmain(void)')
unit += (ROOT/'libinterp/tests/channel-retains.c').read_text()
with tempfile.TemporaryDirectory(prefix='channel-retains-') as directory:
    td = Path(directory)
    target = td/'channels.c'
    target.write_text(unit)
    command = shlex.split(os.environ.get('CC','cc'))
    command += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        model = td/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/path) for path in
                    ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('channels'+str(bits))
        subprocess.run(command+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
