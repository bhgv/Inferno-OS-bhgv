#!/usr/bin/env python3
"""Execute production channel queues/alt preparation with real types/error frames."""
import os
import platform
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


parts = [function(xec, 'void\n'+name+'(') for name in ('cqadd', 'cqdel', 'cqdelp')]
# New SEND/RECV share production span/peer checks; retain the old-source override.
# Extract the real helpers rather than weakening guards with fixture stubs.
if 'static int\nchannelspan(' in xec:
    parts += [function(xec, 'static int\nchannelspan('),
              function(xec, 'static Prog*\nchannelpeer(')]
if 'typedef struct ChannelRefill' in xec:
    start = xec.index('struct ChannelRefill\n{')
    parts += ['typedef struct ChannelRefill ChannelRefill;\n'+xec[start:xec.index('/* The refill callback', start)], function(xec, 'static int\nchannelrefill(')]
parts += [function(xec, 'OP('+name+')') for name in ('isend', 'irecv')]
parts += [function(xec, 'OP(alt)'), function(xec, 'OP(nbalt)')]
parts += [re.sub(r'^#include[^\n]*\n', '', alt, flags=re.M)]
unit = (root/'libinterp/tests/channel-queues.c').read_text()
unit = unit.replace('int\nmain(void)', (root/'libinterp/tests/alt-publication.c').read_text()+'\n\n'+(root/'libinterp/tests/queue-chain.c').read_text()+'\n\nint\nmain(void)')
# Optional integration route uses published native tails and the real C executor.
if os.environ.get('QUEUE_JIT_ROUTE') == '1':
    machine=platform.machine().lower()
    backend='amd64' if machine in ('amd64','x86_64') else 'arm64' if machine in ('arm64','aarch64') else None
    if backend is None:raise RuntimeError('native queue JIT route needs AMD64 or ARM64')
    for header in ('isa.h','raise.h','kernel.h'):
        unit=unit.replace('#include "'+header+'"\n','')
    unit=unit.replace('#include "interp.h"', '#define TEST_LEAF_DISPATCH_RUNTIME // Link the real complete-entry executor.\n#include "comp-'+backend+'.c"\n#include "tests/leaf-cons-stub.h"\n#include "tests/leaf-plain-stubs.h"')
    unit=unit.replace('Type Tarray;', r'''Type Tarray, Tstring;
int mutator,nprop; // Required collector identities for unrelated native entries.
void (*optab[256])(void); // Real C opcode bindings behind the native tails.
char exOp[]="opcode"; // Invalid decoded opcode identity.
static Jit *queuejit; // Owned native table for ALT and NBALT.


/* Keep native publication allocations separate from queue failure injection. */
void*
mallocz(ulong size,int clear)
{
    return clear ? calloc(1,size) : malloc(size);
}
''')
    parts += [function(xec,'int\ncjitexec(')]
    unit=unit.replace('int\nmain(void)\n{', r'''int
main(void)
{
    Module module={0};
    Inst program[2]={{0}};
    program[0].op=IALT;program[1].op=INBALT;
    module.prog=program;module.nprog=2;
    optab[IALT]=alt;optab[INBALT]=nbalt;
    channelcheck(compile(&module,2,nil));queuejit=module.jit;
    channelcheck(queuejit->entry[IALT]==nil && queuejit->entry[INBALT]==nil);
''')
    unit=unit.replace('printf("PASS channel queues:', 'queuejit->freecode(queuejit->code,queuejit->size);free(queuejit);\n\tprintf("PASS channel queues:')
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
            # Keep unrelated longjmp stack topologies in separate instrumented processes.
            if os.environ.get('QUEUE_CASE'):
                families = [os.environ['QUEUE_CASE']]
            elif os.environ.get('QUEUE_CASES'):
                families = os.environ['QUEUE_CASES'].split()
            else:
                families = [
                    'queue-chains', 'queue-slots', 'queue-pop', 'queue-long',
                    'publication-bad-ready', 'publication-bad-wake', 'publication-bad-output',
                    'publication-idle', 'publication-wake', 'publication-wake-tuple',
                    'publication-result', 'publication-payload', 'publication-tuple',
                    'altspans', 'alttransfererrors', 'altslots', 'altgeometry',
                    'queues', 'blocking', 'altregister', 'altconflicts', 'native',
                ]
            if not families:
                raise ValueError('QUEUE_CASES must select at least one family')
            for family in families:
                try:
                    subprocess.run([str(binary)],env=dict(os.environ, **{"QUEUE_CASE":family}),check=True,timeout=60)
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
                    if os.environ.get('QUEUE_KEEP_ARTIFACT'):
                        shutil.copy2(binary, os.environ['QUEUE_KEEP_ARTIFACT'])
                        shutil.copy2(source, os.environ['QUEUE_KEEP_ARTIFACT']+'.c')
                    raise
