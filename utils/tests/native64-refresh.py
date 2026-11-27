#!/usr/bin/env python3
"""Execute real Draw stream, output and native queue bodies against independent wire records."""
import os
from pathlib import Path
import resource
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Serialize independent signed coordinates without relying on production decoder/packing macros.
def oracles():
    events = [(7, (-2147483648, -17, 2147483647, 9), 1),
              (8, (-1, 0, 1, 2), 1), (0xffffffff, (1, 2, 3, 4), 0),
              (9, (0, 0, 1, 1), 0), (10, (0, 0, 1, 1), 0),
              (7, (17, -2147483648, 18, 2147483647), 1), (8, (-8, -9, -2, -1), 1)]
    wire = b''.join(struct.pack('<Iiiii', identifier, *rectangle) for identifier, rectangle, _ in events)
    rows = []
    for identifier, rectangle, deliver in events:
        encode = lambda value: '(-2147483647-1)' if value == -2147483648 else str(value)
        rows.append('{'+str(identifier)+'U,{{'+','.join(map(encode, rectangle[:2]))+'},{'+','.join(map(encode, rectangle[2:]))+'}},'+str(deliver)+'}')
    return '''typedef struct RefreshEvent RefreshEvent;

struct RefreshEvent {
    u32int id; // Independent wire image identity.
    Rectangle rectangle; // Independent signed callback coordinates.
    int deliver; // Only actual callback-bearing screen windows receive notification.
};
static const RefreshEvent events[] = {
'''+',\n'.join(rows)+'''\n}; // Independent expected callbacks, including ignored window identities.
static const uchar wire[] = {'''+','.join(str(value) for value in wire)+'''}; // Python little-endian protocol encoding.
'''


# Keep complete owning bodies; replace only bounded native allocation transports in this fixture.
def unit(control):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    body = (ROOT/'libinterp/draw.c').read_text()
    signatures = ('static int\ndrawbuffered(', 'static void\ndrawflushfailed(', 'static int\ndoflush(',
                  'int\nflushimage(', 'uchar*\nbufimage(', 'void\nqueuerefresh(', 'void\nrefreshslave(')
    selected = '\n\n'.join(support['function'](body, name) for name in signatures)
    selected = selected.replace('rq = malloc(sizeof(Refreshq));', 'rq = refreshallocate(sizeof(Refreshq));')
    selected = selected.replace('free(r);', 'refreshfree(r);')
    if control:
        old, new = {
            'queue': ('rq->next = nil;', '/* Deliberately omit the private queue terminator. */'),
            'retire': ('refreshfree(r);\n\t\t\tcallback(nil, rectangle, context);', 'callback(nil, rectangle, context);\n\t\t\trefreshfree(r);'),
            'write': ('written += amount;', 'written = n; /* Deliberately drop a short-write suffix. */'),
            'eof': ('if(n <= 0){', 'if(n < 0){'),
        }[control]
        if selected.count(old) != 1:
            raise RuntimeError('Refresh control drift: '+control)
        selected = selected.replace(old, new, 1)
    fixture = (ROOT/'utils/tests/native64-refresh.c').read_text()
    fixture = fixture.replace('REFRESH_ORACLES', oracles()).replace('REFRESH_IMPLEMENTATIONS', selected)
    return support['portfixture'](fixture, '#include "lib9.h"')


# A source omission must stop at semantic10, never at a timeout, sanitizer or fatal signal.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    includes = ['-I'+str(ROOT/path) for path in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='native64-refresh-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        for mode, control in (('WORD32', None), ('WORD32-file', None), ('queue', 'queue'), ('write', 'write'), ('eof', 'eof'), ('retire', 'retire')):
            source = work/(mode+'.c'); source.write_text(unit(control))
            binary = work/mode
            model = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode == 'WORD32-file' else [])
            subprocess.run(compiler+flags+model+includes+[str(source), '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=60)
            if control:
                if result.returncode != 10 or 'Refresh mismatch' not in result.stderr:
                    raise RuntimeError('Refresh control missed semantic failure: '+control+' '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS refresh semantic control:', control, arch, flush=True)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(mode+': '+result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
