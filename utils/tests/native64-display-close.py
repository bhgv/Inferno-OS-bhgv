#!/usr/bin/env python3
"""Execute actual native Display cancellation/teardown with explicit resource ownership transports."""
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep whole production bodies; route only native allocations to the exact test ledger.
def unit(control):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    draw = (ROOT/'libinterp/draw.c').read_text()
    native = (ROOT/'libdraw/init.c').read_text()
    signatures = ('static int\ndrawbuffered(', 'static void\ndrawflushfailed(', 'static int\ndoflush(',
                  'int\nflushimage(', 'uchar*\nbufimage(', 'void\nqueuerefresh(', 'void\ndelrefresh(')
    selected = '\n\n'.join(support['function'](draw, name) for name in signatures)
    selected += '\n\n'+'\n\n'.join(support['function'](native, name) for name in
                                    ('int\nlockdisplay(', 'void\nunlockdisplay(', 'static void\ndiscardrefresh(', 'void\nclosedisplay('))
    selected = selected.replace('rq = malloc(sizeof(Refreshq));', 'rq = ownerallocate(sizeof(Refreshq), 1);')
    selected = selected.replace('free(', 'ownerfree(')
    # Restore the separate native lock destructor spelling after matching allocation calls.
    selected = selected.replace('libqlownerfree(', 'libqlfree(')
    if control == 'admission':
        old = 'if(d->closing)\n\t\treturn;'
        if selected.count(old) != 1:
            raise RuntimeError('Close admission control drift')
        selected = selected.replace(old, 'if(0)\n\t\treturn;', 1)
    if control == 'discard':
        old = '\tdiscardrefresh(disp);'
        if selected.count(old) != 2:
            raise RuntimeError('Private close discard control drift')
        selected = selected.replace(old, '\tUSED(disp);', 1)
    if control == 'flush':
        old = ' || d->closing)'
        if selected.count(old) != 1:
            raise RuntimeError('Closing flush control drift')
        selected = selected.replace(old, ')', 1)
    if control == 'cleanup-write':
        old = support['function'](selected, 'int\nflushimage(')
        new = old.replace('for(;;){', 'for(;;){\n\t\tif(d->closing) return 1;', 1)
        selected = selected.replace(old, new, 1)
    if control == 'cancel-callback':
        old = '\ti->reffn = nil;'
        if selected.count(old) != 1:
            raise RuntimeError('Cancellation callback control drift')
        selected = selected.replace(old, '\tUSED(i);', 1)
    if control == 'cancel-lock':
        old = support['function'](selected, 'void\ndelrefresh(')
        new = old.replace('\tlocked = lockdisplay(d);\n', '', 1)
        new = new.replace('\ti->refptr = nil;', '\ti->refptr = nil;\n\tlocked = lockdisplay(d);', 1)
        selected = selected.replace(old, new, 1)
    fixture = (ROOT/'utils/tests/native64-display-close.c').read_text().replace('CLOSE_IMPLEMENTATIONS', selected)
    return support['portfixture'](fixture, '#include "lib9.h"')


# Refuse compiler/signal/sanitizer failures as substitutes for exact semantic control detection.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    includes = ['-I'+str(ROOT/path) for path in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='native64-display-close-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        for mode, control in (('WORD32', None), ('WORD32-file', None), ('admission', 'admission'),
                              ('discard', 'discard'), ('flush', 'flush'), ('cancel-lock', 'cancel-lock'), ('cancel-callback', 'cancel-callback'), ('cleanup-write', 'cleanup-write')):
            source = work/(mode+'.c'); source.write_text(unit(control))
            binary = work/mode
            model = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode == 'WORD32-file' else [])
            subprocess.run(compiler+flags+model+includes+[str(source), '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=60)
            if control:
                if result.returncode != 10 or 'Display close mismatch' not in result.stderr:
                    raise RuntimeError('Close control missed semantic failure: '+control+' '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS display close semantic control:', control, arch, flush=True)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(mode+': '+result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
