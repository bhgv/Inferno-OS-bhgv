#!/usr/bin/env python3
"""Compare C and actual AMD64 SLICEA view reuse with real heap lifetimes."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]


# Extract complete production bodies without counting comment or literal braces.
def function(path,signature):
    source=Path(os.environ.get('HEAP_XEC_SOURCE', root/path) if path=='libinterp/xec.c' else root/path).read_text();start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)
parts=[function('emu/port/dis.c','int\nkprotect('), function('emu/port/dis.c','void\nkrethrow(')]
parts += [function('libinterp/heap.c',r+'\n'+n+'(') for n,r in [('incmem','void'),('freeptrs','void'),('acheck','void'),('nheap','Heap*'),('heap','Heap*'),('initmem','void'),('heapz','Heap*'),('initarray','void'),('heaparray','Heap*')]]
heap_source=(root/'libinterp/heap.c').read_text()
parts += [heap_source[heap_source.index('typedef struct ArrayCopy'):heap_source.index('void*\narraycpy(')]]
parts += [function('libinterp/heap.c', 'void*\narraycpy('), function('libinterp/heap.c', 'void\nnewmp(')]
parts += [function('libinterp/link.c','Modlink*\nmklinkmod('),function('libinterp/xec.c','List*\ncons(')]
parts += [function('libinterp/xec.c','static Type*\nmoduletype(')]
if 'static uchar*\narrayspan(' in Path(os.environ.get('HEAP_XEC_SOURCE',root/'libinterp/xec.c')).read_text():parts += [function('libinterp/xec.c','static uchar*\narrayspan(')]
parts += [function('libinterp/xec.c','OP('+n+')') for n in ['newa','newaz','slicea','slicela']]
unit=(root/'libinterp/tests/heap-bounds.c').read_text().replace('IMPLEMENTATION','\n'.join(parts)).replace('main(int argc, char **argv)','boundsmain(int argc, char **argv)')
unit=re.sub(r'^#include \"[^\"]+\"\n','',unit,flags=re.M)
unit='#include \"dat.h\"\n#include \"fns.h\"\n#include \"comp-amd64.c\"\n#include \"pool.h\"\n#include \"raise.h\"\n#include \"kernel.h\"\n'+unit
unit+=(root/'libinterp/tests/array-view-native.c').read_text()
with tempfile.TemporaryDirectory(prefix='heap-bounds-') as directory:
    td=Path(directory);p=td/'heap.c';p.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64, 32):
        includes = []
        legacy_cons = 'cons(ulong size, List **lp)' in Path(os.environ.get('HEAP_XEC_SOURCE',root/'libinterp/xec.c')).read_text()
        if bits == 32 or legacy_cons:
            model = td / ('word'+str(bits))
            model.mkdir()
            header = (root / 'include/interp.h').read_text()
            if legacy_cons:header = header.replace('cons(ulong, void*);','cons(ulong, List**);')
            if bits == 32:
                header, words = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header, unsigned = re.subn(r'typedef unsigned long\s+UWORD;',
                                          'typedef unsigned int UWORD;', header, count=1)
                if words != 1 or unsigned != 1:raise RuntimeError('WORD32 header mismatch')
            (model / 'interp.h').write_text(header)
            includes = ['-I'+str(model)]
        includes += ['-I'+str(root / path) for path in
                     ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        binary = td / ('heap'+str(bits))
        subprocess.run(command+includes+[str(p), '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+shlex.split(os.environ.get('HEAP_CASE', '')), check=True)
        print('WORD'+str(bits), flush=True)
