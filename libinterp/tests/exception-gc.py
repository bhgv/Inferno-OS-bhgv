#!/usr/bin/env python3
"""Run actual exception selection/cleanup with complete collector epochs at callbacks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SUPPORT = {'__file__': str(ROOT/'libinterp/tests/dis64-runtime.py'), '__name__': 'runtime_support'}
exec(compile((ROOT/'libinterp/tests/dis64-runtime.py').read_bytes(), SUPPORT['__file__'], 'exec'), SUPPORT)
function = SUPPORT['function']


# Keep real collector/heap/handler bodies and use only the shared bounded arena boundary.
def buildunit(port):
    collector = Path(os.environ.get('EXCEPTION_GC_COLLECTOR', ROOT/'libinterp/gc.c')).read_text()
    collector = '\n'.join(line for line in collector.splitlines() if not line.startswith('#include'))
    prefix = (ROOT/'libinterp/tests/gc-roots.c').read_text().split('/* Retain exception cycles')[0]
    prefix = prefix.replace('/* Swept objects release descriptor', 'static void duringdestroy(Heap*, int); // Observe blocked collection and process-root publication.\n\n\n/* Swept objects release descriptor', 1)
    prefix = prefix.replace(function(prefix, 'static void\ndispose('),
                            'static void\ndispose(Heap *heap, int swept) { duringdestroy(heap, swept); }', 1)
    prefix = prefix.replace(function(prefix, 'void\nerror('),
                            'static void raiseerror(char*); // Capture only the expected actual RAISE transfer.\n\nvoid\nerror(char *text) { raiseerror(text); }', 1)
    prefix = prefix.replace('COLLECTOR_IMPLEMENTATION', collector)
    source = Path(os.environ.get('EXCEPTION_GC_HANDLER', ROOT/port/'port/exception.c')).read_text()
    parts = ['#define NOPC ((ulong)-1) // Native absent-handler coordinate.']
    parts += [function(source, signature) for signature in (
        'static int\nematch(', 'static Type*\nframetype(', 'static void\nfreeframe(',
        'static int\nexceptionframe(', 'static ulong\nexceptionpc(',
        'static int\nexceptiontarget(', 'int\nhandler(')]
    heap = (ROOT/'libinterp/heap.c').read_text()
    parts += [function(heap, signature) for signature in ('void\nfreeptrs(', 'void\nfreeheap(', 'void\ndestroy(', 'void\ninitmem(')]
    parts += [function((ROOT/'libinterp/string.c').read_text(), 'char*\nstring2c(')]
    parts += [function((ROOT/'libinterp/xec.c').read_text(), 'OP(iraise)')]
    fixture = (ROOT/'libinterp/tests/exception-gc.c').read_text()
    return prefix+fixture.replace('EXCEPTION_IMPLEMENTATION', '\n\n'.join(parts))


# Check both real handler ports and scalar selections without changing the public wire profile.
def main():
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'emu/port', 'include')]
    with tempfile.TemporaryDirectory(prefix='exception-gc-') as temporary:
        work = Path(temporary)
        for port in ('emu', 'os'):
            source = work/(port+'.c')
            source.write_text(buildunit(port))
            for label, selection in (('legacy', []), ('WORD32', ['-DDIS_WORD32=1'])):
                binary = work/(port+'-'+label)
                subprocess.run(compiler+flags+selection+includes+[str(source), '-o', str(binary)], check=True)
                subprocess.run([str(binary)], check=True)
                print('PASS exception/GC', port, label, flush=True)


if __name__ == '__main__':
    main()
