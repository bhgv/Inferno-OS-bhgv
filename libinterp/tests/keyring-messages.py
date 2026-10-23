#!/usr/bin/env python3
"""Exercise production Keyring framing and real builtin frames with injected I/O."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Find function boundaries without treating comments or literals as C braces.
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


source = Path(os.environ.get('KEYRING_MESSAGES_SOURCE', root/'libinterp/keyring.c')).read_text()
start = source.index('static int\nkeyring_writeall(')
end = source.index('void\nKeyring_auth(', start)
record = source.index('static int\ngetbuf(')
record_end = source.index('void\nKeyring_dessetup(', record)
parts = [function(source, 'static WORD\nkeyring_bytearray('), source[start:end], source[record:record_end]]
protect = (root/'emu/port/dis.c').read_text()
parts.insert(0, function(protect, 'int\nkprotect(') + '\n' + function(protect, 'void\nkrethrow('))
unit = (root/'libinterp/tests/keyring-messages.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='keyring-messages-') as directory:
    td = Path(directory)
    src = td/'messages.c'
    src.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
    for model in ('native', 'word32'):
        override = []
        if model == 'word32':
            target = td/model
            target.mkdir()
            header = (root/'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            assert n == m == 1
            (target/'interp.h').write_text(header)
            override = ['-I'+str(target)]
        binary = td/('messages-'+model)
        subprocess.run(command+override+includes+[str(src), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
