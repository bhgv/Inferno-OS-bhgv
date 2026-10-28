#!/usr/bin/env python3
"""Exercise both production Sys I/O implementations with real builtin frames."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
PORTS = os.environ.get('SYS_IO_PORTS', 'emu os').split()
MODELS = os.environ.get('SYS_IO_MODELS', 'native word32').split()
if not PORTS or any(port not in ('emu', 'os') for port in PORTS):
    raise ValueError('SYS_IO_PORTS must select emu and/or os')
if not MODELS or any(model not in ('native', 'word32') for model in MODELS):
    raise ValueError('SYS_IO_MODELS must select native and/or word32')


# Ignore braces in comments and literals when extracting production functions.
def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[start:brace+token.end()]
    raise ValueError(signature)


with tempfile.TemporaryDirectory(prefix='sys-io-') as directory:
    td = Path(directory)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    for platform in PORTS:
        source = Path(os.environ.get('SYS_IO_SOURCE', root / (platform+'/port/inferno.c'))).read_text()
        signatures = ['static void\nsysstatusframe(', 'static int\nsysnativeint(', 'static int\nsysiocount(', 'static void\nsysioframe(', 'static int\nsysiofd('] + ['void\nSys_'+name+'(' for name in ('read','readn','write','pread','pwrite')]
        unit = (root / 'libinterp/tests/sys-io.c').read_text().replace('IMPLEMENTATION', '\n'.join(function(source, sig) for sig in signatures if sig in source))
        target = td / (platform+'.c')
        target.write_text(unit)
        for model in MODELS:
            override = []
            if model == 'word32':
                modeldir = td / (platform+model)
                modeldir.mkdir()
                header = (root / 'include/interp.h').read_text()
                header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                if n != 1 or m != 1:
                    raise RuntimeError("WORD model substitution failed")
                (modeldir / 'interp.h').write_text(header)
                override = ['-I'+str(modeldir)]
            binary = td / (platform+'-'+model)
            subprocess.run(cmd+override+includes+['-pthread',str(target),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
            for key in range(40):
                env = dict(os.environ, SYS_IO_ERROR=str(key))
                subprocess.run([str(binary)], env=env, check=True)
