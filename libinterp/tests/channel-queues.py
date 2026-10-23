#!/usr/bin/env python3
"""Execute production channel queues/alt preparation with real types/error frames."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
xec = Path(os.environ.get('XEC_SOURCE', root/'libinterp/xec.c')).read_text()
alt = Path(os.environ.get('ALT_SOURCE', root/'libinterp/alt.c')).read_text()


# Extract complete bodies without counting braces in comments or literals.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [function(xec, 'void\n'+name+'(') for name in ('cqadd', 'cqdel', 'cqdelp')]
parts += [function(xec, 'OP('+name+')') for name in ('isend', 'irecv')]
parts += [re.sub(r'^#include[^\n]*\n', '', alt, flags=re.M)]
unit = (root/'libinterp/tests/channel-queues.c').read_text()
for runtime in ('emu/port/dis.c', 'os/port/dis.c'):
    guard = (root/runtime).read_text()
    native = Path(os.environ.get('CCOM_SOURCE', root/runtime.replace('dis.c','inferno.c'))).read_text()
    body = function(native, 'void\nccom(')+'\n'+'\n'.join(function(guard, result+'\n'+name+'(')
                     for name, result in [('kprotect','int'), ('krethrow','void')])
    with tempfile.TemporaryDirectory(prefix='channel-queues-') as directory:
        td = Path(directory)
        source = td/'queues.c'
        source.write_text(unit.replace('GUARD',body).replace('IMPLEMENTATION','\n'.join(parts)))
        cmd = shlex.split(os.environ.get('CC','cc'))
        cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        for bits in (64,32):
            includes = []
            if bits == 32:
                header = (root/'include/interp.h').read_text()
                header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                (td/'interp.h').write_text(header)
                includes = ['-I'+str(td)]
            includes += ['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary = td/('queues'+str(bits))
            subprocess.run(cmd+includes+[str(source),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
