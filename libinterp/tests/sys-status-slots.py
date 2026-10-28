#!/usr/bin/env python3
"""Check real Sys status bindings with generated frames and controlled native APIs."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract production functions while ignoring quoted and comment braces."""
    start=source.index(signature)
    brace=source.index('{',start)
    depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise ValueError(signature)


with tempfile.TemporaryDirectory(prefix='sys-status-') as directory:
    td=Path(directory)
    command=shlex.split(os.environ.get('CC','cc'))
    command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for platform in ('emu','os'):
        source=Path(os.environ.get('SYS_STATUS_SOURCE',ROOT/platform/'port/inferno.c')).read_text()
        parts=[function(source,'static int\nsysnativeint(')] if 'static int\nsysnativeint(' in source else []
        parts += [function(source, signature) for signature in ('static void\nsysstatusframe(', 'static int\nsysiofd(') if signature in source]
        parts += [function(source,'void\nSys_'+name+'(') for name in ('dup','remove','seek','unmount','chdir','mount','bind','iounit','wstat','fwstat','millisec','sleep','print','fprint','werrstr','export')]
        target=td/(platform+'.c')
        unit=(ROOT/'libinterp/tests/sys-status-slots.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
        if os.environ.get('SYS_STATUS_ALIGNED'):
            unit=unit.replace('offset<8','offset<1')
        target.write_text(unit)
        for bits in (64,32):
            model=td/(platform+str(bits))
            model.mkdir()
            header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=td/(platform+str(bits)+'-status')
            subprocess.run(command+includes+['-DTEST_OS='+str(int(platform=='os')),str(target),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
