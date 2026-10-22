#!/usr/bin/env python3
"""Exercise both production Sys I/O implementations with real builtin frames."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = ""


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
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root / p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    for platform in ('emu', 'os'):
        source = Path(os.environ.get('SYS_IO_SOURCE', root / (platform+'/port/inferno.c'))).read_text()
        signatures = ['static int\nsysiocount('] + ['void\nSys_'+name+'(' for name in ('read','readn','write','pread','pwrite')]
        unit = (root / 'libinterp/tests/sys-io.c').read_text().replace('IMPLEMENTATION', '\n'.join(function(source, sig) for sig in signatures))
        target = td / (platform+'.c')
        target.write_text(unit)
        for model in ('native','word32'):
            override = []
            if model == 'word32':
                modeldir = td / (platform+model)
                modeldir.mkdir()
                header = (root / 'include/interp.h').read_text()
                header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                assert n == m == 1
                (modeldir / 'interp.h').write_text(header)
                override = ['-I'+str(modeldir)]
            binary = td / (platform+'-'+model)
            subprocess.run(cmd+override+includes+[str(target),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
