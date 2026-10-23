#!/usr/bin/env python3
"""Check production legacy integer/key text with real libmp export and native Fmt."""
import base64
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract complete production functions, ignoring literal/comment braces."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise ValueError(signature)


source = Path(os.environ.get('KEYRING_SERIALIZE_SOURCE', root / 'libinterp/keyring.c')).read_text()
parts = [function(source, 'int\nbigtobase64('), function(source, 'int\nbig64conv(')]
parts.append(function(source, 'int\nkeyringfields('))
algdir = Path(os.environ.get('KEYRING_ALG_DIR', root / 'libkeyring'))
for alg in ('rsa', 'dsa', 'eg'):
    algsource = (algdir / (alg+'alg.c')).read_text()
    for kind in ('sk', 'pk', 'sig'):
        parts.append(function(algsource, 'static int\n'+alg+'_'+kind+'2str('))
export = (root / 'libmp/port/mptobe.c').read_text()
parts.insert(0, re.sub(r'^#include[^\n]*\n', '', export, flags=re.M))
codec = re.sub(r'^#include[^\n]*\n', '', (root / 'lib9/u64.c').read_text(), flags=re.M)
parts.insert(0, codec)
vectors = []
for n in range(129):
    raw = bytes((i*37+n*11)%256 for i in range(n))
    magnitude = raw.lstrip(b'\0') or b'\0'
    legacy = (b'\0' if magnitude[0]&128 else b'')+magnitude
    vectors.append('{'+str(n)+',"'+base64.b64encode(legacy).decode()+'"}')
unit = (root / 'libinterp/tests/keyring-serialize.c').read_text()
unit = unit.replace('main(int argc, char **argv)', 'integer_main(int argc, char **argv)')
unit += (root / 'libinterp/tests/keyring-fields.c').read_text().replace('main(int argc, char **argv)', 'fields_main(int argc, char **argv)')
unit += (root / 'libinterp/tests/keyring-records.c').read_text()
recordsource = Path(os.environ.get('KEYRING_RECORD_SOURCE', root / 'libinterp/keyring.c')).read_text()
records = []
# Allow older record wrappers to be tested while supplying the new helper separately.
records.append(function(recordsource if 'int\nkeyringrecord(' in recordsource else source, 'static int\nkeyringrecord('))
records.append(function(source, 'static int\nkeyringexpiration('))
for name in ('sktostr', 'pktostr', 'certtostr'):
    records.append(function(recordsource, 'static int\n'+name+'('))
unit = unit.replace('CERTIFICATE_IMPLEMENTATION', function(recordsource, 'static Certificate*\nstrtocert('))
unit = unit.replace('RECORD_IMPLEMENTATION', '\n'.join(records))
unit = unit.replace('IMPLEMENTATION', '\n'.join(parts))
unit = unit.replace('VECTORS', 'static struct {int n; char *expected;} vectors[] = {'+','.join(vectors)+'};')
with tempfile.TemporaryDirectory(prefix='keyring-serialize-') as directory:
    td = Path(directory)
    target = td / 'serialize.c'
    target.write_text(unit)
    binary = td / 'serialize'
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    cmd += ['-I'+str(root / p) for p in ('Linux/arm/include', 'include', 'lib9', 'libinterp', 'libkeyring')]
    fmt = ('rune', 'snprint', 'vsnprint', 'dofmt', 'fmt', 'fmtlock', 'errfmt',
           'utflen', 'utfnlen', 'runestrlen', 'dorfmt', 'fmtprint')
    subprocess.run(cmd+[str(target)]+[str(root / 'lib9' / (name+'.c')) for name in fmt]+['-o', str(binary)], check=True)
    subprocess.run([str(binary)]+os.environ.get('KEYRING_SERIALIZE_CASE', '').split(), check=True, timeout=30)
