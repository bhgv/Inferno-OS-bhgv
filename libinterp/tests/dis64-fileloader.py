#!/usr/bin/env python3
"""Exercise opt-in production readmod/parsemod routing and the connected scheduler."""
import os
from pathlib import Path
import platform
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SUPPORT = {'__file__': str(ROOT/'libinterp/tests/dis64-scheduler.py'), '__name__': 'dis64_scheduler_support'}
exec(compile((ROOT/'libinterp/tests/dis64-scheduler.py').read_bytes(), SUPPORT['__file__'], 'exec'), SUPPORT)
function = SUPPORT['function']


# Preserve production parser bodies and strict unreachable legacy-helper boundaries.
def buildunit():
    unit = SUPPORT['buildunit']()
    unit = unit.replace('Module*\nprepare64pilot(', 'Module*\nactualprepare64pilot(', 1)
    point = unit.index('typedef struct NativeObservation')
    prefix, scheduler = unit[:point], unit[point:]
    scheduler = scheduler.replace('prepare64pilot(', 'fileprepare(')
    scheduler = scheduler.replace('int\nmain(', 'int\nschedulermain(', 1)
    source = (ROOT/'libinterp/load.c').read_text()
    parts = [function((ROOT/'libinterp/link.c').read_text(), signature) for signature in ('static void\nnewlink(', 'void\nmlink(')]
    parts += [function(source, signature) for signature in ('static int\noperand(', 'static ulong\ndisw(',
             'static WORD\ndissignedword(', 'double\ncanontod(', 'int\nbrpatch(')]
    parts += [source[source.index('/* Check the complete encoded operand'):source.index('/* Allocate an unpublished module')]]
    parts += [function(source, 'Module*\nlookmod(')]
    parts += [function((ROOT/'libinterp/readmod.c').read_text(), 'Module*\nreadmod(')]
    fixture = (ROOT/'libinterp/tests/dis64-fileloader.c').read_text()
    fixture = fixture.replace('FILE_IMPLEMENTATION', '\n\n'.join(parts))
    fixture = fixture.replace('FILE_SCHEDULER', scheduler)
    return prefix+fixture


# Compile default, mismatched and enabled models, then run the real whole-image chain.
def main():
    if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        print('SKIP Dis64 file-loader RX: requires Linux/AMD64')
        return
    with tempfile.TemporaryDirectory(prefix='dis64-fileloader-') as temporary:
        work = Path(temporary)
        source = work/'fileloader.c'
        source.write_text(buildunit())
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        fixtures = []
        scalar = SUPPORT['SUPPORT']
        for name in ('pilot42', 'pilot-init42', 'divide-zero', 'shift-invalid'):
            fixture = work/(name+'.dis64')
            if name in ('divide-zero', 'shift-invalid'):
                operation = '43100000' if name == 'divide-zero' else '4e102000'
                fixture.write_bytes(scalar['image']([bytes.fromhex(operation), bytes.fromhex('0f1b')]))
            else:
                fixture.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64'/(name+'.hex')).read_text()))
            fixtures.append((fixture, '1' if name == 'divide-zero' else '2' if name == 'shift-invalid' else '0'))
        for label, flags in (('default', ['-DDIS_WORD32=1']), ('legacy', ['-DDIS64_PILOT_LOADER=1']),
                             ('enabled', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])):
            binary = work/label
            subprocess.run(command+flags+[str(source), str(ROOT/'libinterp/dec.c'), '-o', str(binary), '-lm', '-Wl,--wrap=free'], check=True)
            selected = fixtures if label == 'enabled' else [(fixtures[0][0], 'gated')]
            for fixture, mode in selected:
                subprocess.run([str(binary), str(fixture), mode], check=True)


if __name__ == '__main__':
    main()
