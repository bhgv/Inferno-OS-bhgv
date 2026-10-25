#!/usr/bin/env python3
"""Run stat/fstat/fd2path with the guarded production directory/heap adapters."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract source functions without counting braces in literals and comments."""
    start=source.index(signature)
    brace=source.index('{',start)
    depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise ValueError(signature)


heap=(ROOT/'libinterp/heap.c').read_text()
strings=(ROOT/'libinterp/string.c').read_text()
runt=(ROOT/'libinterp/runt.c').read_text()
parts=[function(heap,result+'\n'+name+'(') for result,name in
       [('void','freeptrs'),('void','initmem'),('void','acheck'),('void','initarray'),('Heap*','heaparray'),('void','freearray')]]
parts += [function(strings,'String*\n'+name+'(') for name in ('newstring','newrunes','c2string')]
parts += [function(runt,'void\nretstr(')]
shared=(ROOT/'libinterp/tests/sys-dirread.c').read_text()
support=shared.split('/* Invoke the builtin under an outer frame')[0]
support+=function(shared,'static void\ncheckstring(')+'\n'
# Add the string-owner observer to the shared real-finalizer adapter.
support=support.replace('static Proc worker;','static Proc worker;\nstatic void watchstring(void*);')
support=support.replace('if(value == H)\n\t\treturn;','if(value == H)\n\t\treturn;\n\twatchstring(value);',1)
template=support+(ROOT/'libinterp/tests/sys-stat-slots.c').read_text()
with tempfile.TemporaryDirectory(prefix='sys-stat-slots-') as directory:
    td=Path(directory)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for platform in ('emu','os'):
        source=Path(os.environ.get('SYS_STAT_SOURCE',ROOT/platform/'port/inferno.c')).read_text()
        guard=(ROOT/platform/'port/dis.c').read_text()
        bodies=[function(guard,'int\nkprotect(')]
        bodies += [function(source,sig) for sig in ('void\nsysinit(','static void\nunpackdir(','void\nSys_stat(','void\nSys_fstat(','void\nSys_fd2path(')]
        target=td/(platform+'.c')
        target.write_text(template.replace('IMPLEMENTATION','\n'.join(parts+bodies)))
        for bits in (64,32):
            model=td/(platform+str(bits))
            model.mkdir()
            header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=td/(platform+str(bits)+'-stat')
            subprocess.run(command+includes+[str(target),str(ROOT/'lib9/rune.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
