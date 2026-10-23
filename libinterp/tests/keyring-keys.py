#!/usr/bin/env python3
"""Check real base64/libmp and RSA/DSA/ElGamal parsers and cleanup."""
import base64
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


# Extract functions without matching braces inside C comments or quoted text.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


source = Path(os.environ.get('KEYRING_PARSE_SOURCE', root/'libinterp/keyring.c')).read_text()
parser = function(source, 'mpint*\nbase64tobig(')
parts = []
# Use the real digit allocator/import/export and cryptographic key destructors.
for name in ('libmp/port/mpaux.c', 'libmp/port/betomp.c', 'libmp/port/mptobe.c',
             'lib9/u64.c', 'libsec/port/rsaalloc.c', 'libsec/port/dsaalloc.c', 'libsec/port/egalloc.c'):
    text = re.sub(r'^#include[^\n]*\n', '', (root/name).read_text(), flags=re.M)
    if name == 'lib9/u64.c':
        text = '#define INVAL base64INVAL\n'+text+'\n#undef INVAL\n'
    parts.append(text)
algdir = Path(os.environ.get('KEYRING_ALG_DIR', root/'libkeyring'))
for alg,file in [('rsa','rsaalg.c'),('dsa','dsaalg.c'),('eg','egalg.c')]:
    source = (algdir/file).read_text()
    for kind in ('sk','pk','sig'):
        parts.append(function(source, 'static void*\n'+alg+'_str2'+kind+'('))
vectors = []
for size in (1,2,3,4,31,32,33,767,768,769,800):
    data = bytes((i*37+1)%256 for i in range(size))
    vectors.append('{'+str(size)+',"'+base64.b64encode(data).decode()+'"}')
unit = (root/'libinterp/tests/keyring-keys.c').read_text()
unit = unit.replace('NATIVE_IMPLEMENTATION', '\n'.join(parts[:7]))
unit = unit.replace('PARSER_IMPLEMENTATION', parser)
unit = unit.replace('ALGORITHM_IMPLEMENTATION', '\n'.join(parts[7:]))
unit = unit.replace('VECTORS', 'static struct {int size;char *text;} vectors[]={'+','.join(vectors)+'};')
with tempfile.TemporaryDirectory(prefix='keyring-keys-') as directory:
    td = Path(directory)
    target = td/'keys.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    cmd += ['-ffunction-sections','-fdata-sections','-Wl,--gc-sections']
    cmd += ['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libmp/port')]
    binary = td/'keys'
    subprocess.run(cmd+[str(target),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
