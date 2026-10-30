#!/usr/bin/env python3
"""Connect the production Dis64 constructor to real root scheduling and terminal cleanup."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SUPPORT = {'__file__': str(ROOT/'libinterp/tests/dis64-runtime.py'), '__name__': 'dis64_runtime_support'}
exec(compile((ROOT/'libinterp/tests/dis64-runtime.py').read_bytes(), SUPPORT['__file__'], 'exec'), SUPPORT)
function = SUPPORT['function']


# Assemble unchanged production bodies around the existing bounded host/pool fixture.
def buildunit():
    heap = (ROOT/'libinterp/heap.c').read_text()
    load = (ROOT/'libinterp/load.c').read_text()
    link = (ROOT/'libinterp/link.c').read_text()
    stack = (ROOT/'libinterp/stack.c').read_text()
    xec = (ROOT/'libinterp/xec.c').read_text()
    scheduler = Path(os.environ.get('DIS64_SCHED_SOURCE', ROOT/'emu/port/dis.c')).read_text()
    prefix = (ROOT/'libinterp/tests/dis64-runtime.c').read_text().split('IMPLEMENTATION')[0]
    prefix = prefix.replace('static struct RuntimeBlock blocks', 'static void schedulerfree(void*); // Observe live process terminal metadata.\n\nstatic struct RuntimeBlock blocks', 1)
    prefix = prefix.replace('if(pointer == NULL)\n\t\treturn;', 'if(pointer == NULL)\n\t\treturn;\n\tschedulerfree(pointer);', 1)
    prefix = prefix.replace(function(prefix, 'void\nerror('), function((ROOT/'emu/port/main.c').read_text(), 'void\nerror('))
    state = scheduler[scheduler.index('struct\n{'):scheduler.index('} isched;')+len('} isched;')]
    state += '\nstatic Prog *proghash[64];\nstatic Prog **pidlook(int);\nstatic Progs *delgrp(Prog*);\nstatic void addgrp(Prog*, Prog*);\n'
    prefix = prefix.replace('#define OP(name) void name(void)', '#define OP(name) void actual_##name(void)', 1)
    prefix += '\n'+state+'\n#include "vmstartup.h"\n'
    parts = [function(scheduler, signature) for signature in ('int\nkprotect(', 'void\nkrethrow(')]
    parts += [function(heap, signature) for signature in (
        'void\nfreeptrs(', 'void\nfreeheap(', 'void\ndestroy(', 'Type*\ndtype(',
        'void\nfreetype(', 'void\ninitmem(', 'Heap*\nheapz(', 'Heap*\nnheap(',
        'void\nfreemodlink(', 'void\nnewmp(')]
    parts += [function(load, signature) for signature in ('void\nfreemod(', 'void\nunload(')]
    parts += [function(link, signature) for signature in ('static int\nlinkm(', 'Modlink*\nmklinkmod(')]
    parts += [link[link.index('typedef struct LinkModule'):link.index('/* Release partial or complete export')]]
    parts += [function(stack, signature) for signature in ('static int\nstacksize(', 'static ulong\nstackhint(',
              'static Stkext*\nstackextent(', 'static Type*\nstackframe(', 'void\nnewstack(')]
    parts += [function(xec, signature) for signature in ('static void\nunwindstate(', 'void\ndestroystack(',
              'void\nxec(', 'Prog*\nisave(', 'void\nirestore(', 'static WORD\nshiftcount(')]
    parts += [function(xec, 'int\ncjitexec(').replace('int\ncjitexec(', 'int\nactualcjitexec(', 1)]
    names = ('movw', 'addw', 'divw', 'shlw', 'iexit')
    parts += [function(xec, 'OP('+name+')') for name in names]
    parts += [re.sub(r'^#include[^\n]*\n', '', (ROOT/'libinterp/load64.c').read_text(), flags=re.M)]
    parts += [function(scheduler, signature) for signature in ('static Prog**\npidlook(', 'Prog*\ncurrun(', 'int\nnprog(',
              'void\naddrun(', 'Prog*\ndelrun(', 'void\npushrun(', 'static Progs*\ndelgrp(',
              'void\nnewgrp(', 'static void\naddgrp(', 'static int\nnewrootgrp(')]
    parts += [scheduler[scheduler.index('typedef struct NewProgEnv'):scheduler.index('Prog*\nnewprog(')]]
    parts += [function(scheduler, 'Prog*\nnewprog(')]
    parts += [scheduler[scheduler.index('typedef struct SchedModule'):scheduler.index('Prog*\nschedmod(')]]
    parts += [function(scheduler, signature) for signature in ('Prog*\nschedmod(', 'void\ntellsomeone(', 'void\ndelprog(', 'void\nprogexit(')]
    unit = prefix+(ROOT/'libinterp/tests/dis64-scheduler.c').read_text()
    unit = unit.replace('SCHEDULER_IMPLEMENTATION', '\n\n'.join(parts))
    unit = unit.replace('HANDLERS', 'optab[IMOVW]=countmovw; optab[IADDW]=countaddw; optab[IDIVW]=countdivw; optab[ISHLW]=countshlw; optab[IEXIT]=countexit;')
    return unit


# Execute the connected scheduler fixture without enabling filesystem routing.
def main():
    if platform.system() != 'Linux' or platform.machine().lower() not in ('amd64', 'x86_64'):
        print('SKIP Dis64 scheduler RX: requires Linux/AMD64')
        return
    unit = buildunit()
    with tempfile.TemporaryDirectory(prefix='dis64-scheduler-') as temporary:
        work = Path(temporary)
        source = work/'scheduler.c'
        source.write_text(unit)
        binary = work/'scheduler'
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
        command += ['-DDIS_WORD32=1']
        command += ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        subprocess.run(command+[str(source), str(ROOT/'libinterp/dec.c'), '-o', str(binary), '-lm', '-Wl,--wrap=free'], check=True)
        for name in ('pilot42', 'pilot-init42', 'divide-zero', 'shift-invalid'):
            image = work/(name+'.dis64')
            if name in ('divide-zero', 'shift-invalid'):
                operation = '43100000' if name == 'divide-zero' else '4e102000'
                image.write_bytes(SUPPORT['image']([bytes.fromhex(operation), bytes.fromhex('0f1b')]))
            else:
                image.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64'/ (name+'.hex')).read_text()))
            subprocess.run([str(binary), str(image), '1' if name == 'divide-zero' else '2' if name == 'shift-invalid' else '0'], check=True)


if __name__ == '__main__':
    main()
