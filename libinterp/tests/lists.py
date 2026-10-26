#!/usr/bin/env python3
"""Exercise real list instructions with exact allocations and reference maps."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('LIST_SOURCE', root / 'libinterp/xec.c')).read_text()


# Ignore braces in comments and literals when extracting production functions.
def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


parts = [function(source, 'List*\ncons(')]
parts += [function(source, 'static Type*\nmoduletype(')]
if 'static List*\nlisthead(' in source:parts += [function(source,'static List*\nlisthead(')]
parts += [function(source, 'OP('+name+')') for name in ('consb','consw','consl','consp','consf','consm','consmp','movp','movmp','headmp','headb','headw','headl','headf','headm','lenl','headp','tail')]
heap = (root / 'libinterp/heap.c').read_text()
parts = [function(heap, 'void\nincmem('), function(heap, 'void\nfreeptrs('), function(heap, 'void\ninitmem(')] + parts
parts.insert(0, function((root / 'emu/port/dis.c').read_text(), 'int\nkprotect('))
# Parent snapshots retain their original constructor without the new transaction helpers.
if 'Type*\ncconsmappedtype(' in source:
    parts += [function(source, 'Type*\ncconsmappedtype(')]
    parts += [source[source.index('typedef struct ConsMapped'):source.index('/* Preserve the destination, tail')]]
    parts += [function((root/'emu/port/dis.c').read_text(), 'void\nkrethrow(')]
parts = [part for part in parts if not part.startswith('OP(consmp)')] + [function(source, 'OP(consmp)')]
unit = (root / 'libinterp/tests/lists.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='lists-') as directory:
    td = Path(directory)
    target = td / 'codecs.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    selected=os.environ.get('LIST_HEAD_CASE','')
    if selected not in ('','nil','raw','length','construct','null-tail','null-source','retain'):raise ValueError(selected)
    if selected:cmd += ['-DLIST_HEAD_CASE='+{'nil':'1','raw':'2','length':'3','construct':'4','null-tail':'5','null-source':'6','retain':'7'}[selected]]
    includes = ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    for model in ('native','word32'):
        override = []
        # Match the declaration when testing a parent with the typed cons API.
        legacy_cons = 'cons(ulong size, List **lp)' in source
        if model == 'word32' or legacy_cons:
            modeldir = td / model
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            if legacy_cons:header = header.replace('cons(ulong, void*);','cons(ulong, List**);')
            if model == 'word32':
                header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header, m = re.subn(r'typedef unsigned long\s+UWORD;',
                                   'typedef unsigned int UWORD;', header, count=1)
                if n != 1 or m != 1:
                    raise RuntimeError("WORD32 fixture header mismatch")
            (modeldir / 'interp.h').write_text(header)
            override = ['-I'+str(modeldir)]
        binary = td / ('codecs-'+model)
        subprocess.run(cmd+override+includes+[str(target),'-o',str(binary)],check=True)
        # Separate invalid constructor calls: this sandbox cannot reliably unwind
        # ASan stack shadow state across mixed siglongjmp call paths.
        if selected == 'null-tail':
            for operation in range(8):subprocess.run([str(binary),str(operation)],check=True)
        elif selected == 'null-source':subprocess.run([str(binary),'8'],check=True)
        else:
            subprocess.run([str(binary)],check=True)
            if not selected:
                for operation in range(9):subprocess.run([str(binary),str(operation)],check=True)
