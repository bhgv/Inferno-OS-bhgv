#!/usr/bin/env python3
"""Check int-result native I/O bridges in emu/os with full-width service replies."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


# Extract definitions without counting braces inside comments and literals.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


with tempfile.TemporaryDirectory(prefix='native-io-') as temporary:
    work = Path(temporary)
    for platform in os.environ.get('NATIVE_IO_PLATFORM', 'emu os').split():
        if platform not in ('emu', 'os'):
            raise ValueError(platform)
        native = Path(os.environ.get('NATIVE_IO_SOURCE', ROOT/platform/'port/discall.c')).read_text()
        parts = []
        for signature in ('static int\nlibiospan(', 'static int\nlibioresult('):
            if signature in native:
                parts.append(function(native, signature))
        parts += [function(native, 'int\n'+name+'(') for name in ('libread', 'libreadn', 'libwrite')]
        source = work/(platform+'.c')
        binary = work/platform
        source.write_text((ROOT/'emu/port/tests/native-io.c').read_text().replace('IMPLEMENTATION', '\n\n\n'.join(parts)))
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'include')]
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        for family in ([os.environ['NATIVE_IO_CASE']] if os.environ.get('NATIVE_IO_CASE') else ['reply', 'stream', 'span']):
            print(platform+' '+family, flush=True)
            subprocess.run([str(binary), family], check=True)
