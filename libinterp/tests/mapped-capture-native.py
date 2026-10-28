#!/usr/bin/env python3
"""Check mapped-copy operands across callbacks through C and complete AMD64 JIT."""
import os
import platform
import sys
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

if platform.machine().lower() not in ('x86_64','amd64'):
    print('SKIP mapped native preflight: requires an AMD64 host')
    sys.exit(0)

ROOT = Path(__file__).resolve().parents[2]  # Production source root.
SOURCE = Path(os.environ.get('MAPPED_CAPTURE_SOURCE',ROOT/'libinterp/xec.c')).read_text()  # Baseline override is read-only.


# Borrow a complete C definition, excluding braces in comments and literals.
def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{',start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',SOURCE[brace:]):
        if token.group()=='{': depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0: return SOURCE[start:brace+token.end()]
    raise RuntimeError(signature)


signatures = ['static Type*\nmoduletype(','static List*\nlisthead(','OP(movmp)','OP(headmp)','void\nmovtmp(']
if 'static int\nmappedcopytype(' in SOURCE: signatures.insert(0,'static int\nmappedcopytype(')
signatures += ['int\ncjitexec(']
unit = (ROOT/'libinterp/tests/mapped-capture-native.c').read_text()
heap = (ROOT/'libinterp/heap.c').read_text()
saved = SOURCE
SOURCE = heap
maps = '\n\n'.join(function(signature) for signature in ('void\nincmem(', 'void\nfreeptrs('))
SOURCE = saved
unit = unit.replace('IMPLEMENTATION', maps+'\n\n'+'\n\n'.join(function(signature) for signature in signatures))
with tempfile.TemporaryDirectory(prefix='mapped-capture-native-') as temporary:
    td = Path(temporary)
    source = td/'fixture.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for bits in map(int, os.environ.get('MAPPED_CAPTURE_WORDS', '32 64').split()):
        if bits not in (32, 64):
            raise ValueError(bits)
        model = td/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits==32:
            header = re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
        (model/'interp.h').write_text(header)
        binary = td/('word'+str(bits))
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        subprocess.run(command+includes+[str(source),'-lm','-pthread','-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
