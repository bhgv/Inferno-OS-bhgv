#!/usr/bin/env python3
"""Execute production buffered/native channel transfers with real types/error frames."""
import os
from pathlib import Path
import re
import shlex
import shutil
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


heap = (root/'libinterp/heap.c').read_text()
parts = [function(heap, result+'\n'+name+'(') for name,result in [('nheap','Heap*'),('acheck','void'),('initmem','void'),('initarray','void'),('heaparray','Heap*'),('freeptrs','void')]]
parts += [function(xec, 'void\n'+name+'(') for name in ('cqadd', 'cqdel', 'cqdelp')]
if 'typedef struct ChannelBuffer' in xec:
    parts += [xec[xec.index('typedef struct ChannelBuffer'):xec.index('/* Validate and retain buffered storage')]]
parts += [function(xec, 'static int\n'+name+'(') for name in ('cgetb','cputb')]
if 'typedef struct CsendAlt' in xec:
    parts += [xec[xec.index('typedef struct CsendAlt'):xec.index('/* Restore REG after allocation')]]
parts += [function(xec, 'int\ncsendalt(')]
parts += [function(xec, 'OP('+name+')') for name in ('isend', 'irecv', 'movp', 'movw')]
parts += ['#undef W\n'+re.sub(r'^#include[^\n]*\n', '', alt, flags=re.M)]
unit = (root/'libinterp/tests/channel-transfers.c').read_text()
unit = unit.replace('int\nmain(void)', (root/'libinterp/tests/native-channel-entry.c').read_text()+'\n\nint\nmain(void)')
for runtime in ('emu/port/dis.c', 'os/port/dis.c'):
    guard = (root/runtime).read_text()
    native = Path(os.environ.get('CCOM_SOURCE', root/runtime.replace('dis.c','inferno.c'))).read_text()
    service = (root/runtime.replace('dis.c','devsrv.c')).read_text()
    start = service.index('typedef struct SrvFile SrvFile;')
    service = service[start:service.index('\nenum\n',start)]+'\n'+function(service, 'static void\nsrvunblock(')
    body = '\n'.join(function(native, 'void\n'+name+'(') for name in ('ccom','csend','crecv'))+'\n'+'\n'.join(function(guard, result+'\n'+name+'(')
                     for name, result in [('kprotect','int'), ('krethrow','void')])
    with tempfile.TemporaryDirectory(prefix='channel-transfers-') as directory:
        td = Path(directory)
        source = td/'queues.c'
        source.write_text(unit.replace('GUARD',body).replace('IMPLEMENTATION','\n'.join(parts)+'\n'+service))
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
            # Keep unrelated longjmp stack topologies in separate instrumented processes.
            if os.environ.get('CHANNEL_TRANSFER_CASE'):
                families = [os.environ['CHANNEL_TRANSFER_CASE']]
            elif os.environ.get('CHANNEL_TRANSFER_CASES'):
                families = os.environ['CHANNEL_TRANSFER_CASES'].split()
            else:
                families = [
                    'entry-move', 'entry-allocate', 'entry-destroy', 'entry-block',
                    'entry-invalid', 'entry-refill', 'ring', 'retains', 'fifo',
                    'senders', 'native', 'ready', 'empty', 'service', 'faults', 'wake',
                ]
            if not families:
                raise ValueError('CHANNEL_TRANSFER_CASES must select at least one family')
            for family in families:
                try:
                    subprocess.run([str(binary)],env=dict(os.environ, **{"CHANNEL_TRANSFER_CASE":family}),check=True)
                except subprocess.CalledProcessError:
                    if os.environ.get('CHANNEL_KEEP_ARTIFACT'):
                        shutil.copy2(binary, os.environ['CHANNEL_KEEP_ARTIFACT'])
                        shutil.copy2(source, os.environ['CHANNEL_KEEP_ARTIFACT']+'.c')
                    raise
