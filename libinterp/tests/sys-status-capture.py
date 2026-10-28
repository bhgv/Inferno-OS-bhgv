#!/usr/bin/env python3
"""Exercise real fixed Sys wrappers and packdir against callback and native-width hazards."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract whole functions without interpreting braces in strings or comments.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


# Keep native API prototypes exact while inspecting all captured arguments.
def mocks():
    cases = [
        ('int', 'kdup', 'int old, int new', 0, 'old == wantold && new == wantnew', 'nativeint'),
        ('int', 'kremove', 'char *s', 1, '!strcmp(s, "first")', 'nativeint'),
        ('vlong', 'kseek', 'int fd, vlong off, int start', 2, 'fd == wantfd && off == wantoff && start == wantstart', 'nativebig'),
        ('int', 'kunmount', 'char *s, char *on', 3, '!strcmp(s, "first") && !strcmp(on, "second")', 'nativeint'),
        ('int', 'kchdir', 'char *s', 4, '!strcmp(s, "first")', 'nativeint'),
        ('int', 'kmount', 'int fd, int afd, char *on, int flags, char *spec', 5, 'fd == wantfd && afd == wantafd && flags == wantflags && !strcmp(on, "second") && !strcmp(spec, "spec")', 'nativeint'),
        ('int', 'kbind', 'char *s, char *on, int flags', 6, '!strcmp(s, "first") && !strcmp(on, "second") && flags == wantflags', 'nativeint'),
        ('int', 'kiounit', 'int fd', 7, 'fd == wantfd', 'nativeint'),
        ('int', 'kdirwstat', 'char *s, Dir *d', 8, '!strcmp(s, "first")', 'nativeint'),
        ('int', 'kdirfwstat', 'int fd, Dir *d', 9, 'fd == wantfd', 'nativeint'),
        ('long', 'osmillisec', 'void', 10, '!released', 'nativeint'),
        ('int', 'limbosleep', 'ulong period', 11, 'sleepentered && period == (ulong)wantperiod', 'nativeint'),
        ('int', 'export', 'int fd, char *path, int flags', 12, 'fd == wantfd && !strcmp(path, "first") && flags == (3 & Sys_EXPASYNC)', 'nativeint'),
    ]
    result = []
    for kind, name, arguments, op, test, value in cases:
        lines = [f'/* Observe captured arguments at the {name} boundary. */', kind,
                 f'{name}({arguments})', '{', f'\tnativecall({op});', f'\tcheck({test});']
        if op in (8, 9):
            lines += ['\tcheckdirectory(d);']
        lines += [f'\treturn {value};', '}']
        result.append('\n'.join(lines))
    return '\n\n\n'.join(result)


ports = os.environ.get('SYS_CAPTURE_PORTS', 'emu os').split()
models = os.environ.get('SYS_CAPTURE_MODELS', 'native word32').split()
families = os.environ.get('SYS_CAPTURE_FAMILIES', 'ordinary nativefd nativeint preflight').split()
if not ports or any(p not in ('emu', 'os') for p in ports):
    raise ValueError('unknown Sys port')
if not models or any(m not in ('native', 'word32') for m in models):
    raise ValueError('unknown WORD model')
if not families or any(f not in ('ordinary', 'nativefd', 'nativeint', 'preflight') for f in families):
    raise ValueError('unknown capture family')
with tempfile.TemporaryDirectory(prefix='sys-capture-') as directory:
    td = Path(directory)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for port in ports:
        source = Path(os.environ.get('SYS_CAPTURE_SOURCE', ROOT/port/'port/inferno.c')).read_text()
        signatures = ['static int\nsysnativeint(', 'static void\nsysstatusframe(', 'static int\nsysiofd(', 'static Dir*\npackdir(']
        signatures += ['void\nSys_'+name+'(' for name in ('dup', 'remove', 'seek', 'unmount', 'chdir', 'mount', 'bind', 'iounit', 'wstat', 'fwstat', 'millisec', 'sleep', 'export')]
        parts = [function(source, sig) for sig in signatures if sig in source]
        unit = (ROOT/'libinterp/tests/sys-status-capture.c').read_text()
        unit = unit.replace('NATIVE_MOCKS', mocks()).replace('IMPLEMENTATION', '\n\n\n'.join(parts))
        target = td/(port+'.c')
        target.write_text(unit)
        for model in models:
            modeldir = td/(port+model)
            modeldir.mkdir()
            header = (ROOT/'include/interp.h').read_text()
            if model == 'word32':
                header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                if n != 1 or m != 1:
                    raise ValueError('WORD substitution failed')
            (modeldir/'interp.h').write_text(header)
            includes = ['-I'+str(modeldir)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
            binary = td/(port+model+'-capture')
            subprocess.run(command+includes+['-DTEST_OS='+str(int(port=='os')), '-pthread', str(target), '-o', str(binary)], check=True)
            for family in families:
                if family != 'preflight':
                    subprocess.run([str(binary)], env=dict(os.environ, SYS_CAPTURE_FAMILY=family), check=True)
                else:
                    keys = [op*8+mode for op in range(13) for mode in range(8) if mode < 6 or op in (2, 5, 7, 9, 12)]
                    keys += [104, 105, 106, 107]
                    if os.environ.get('SYS_CAPTURE_PREFLIGHT') is not None:
                        key = int(os.environ['SYS_CAPTURE_PREFLIGHT'])
                        if key not in keys:
                            raise ValueError('unknown fixed Sys preflight case')
                        keys = [key]
                    for key in keys:
                        subprocess.run([str(binary)], env=dict(os.environ, SYS_CAPTURE_ERROR=str(key)), check=True)
