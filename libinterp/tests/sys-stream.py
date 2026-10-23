#!/usr/bin/env python3
"""Run both production Sys.stream implementations with actual VM frame declarations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
bodies = []


def function(source, signature):
    """Extract balanced C bodies while ignoring braces in comments and strings."""
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


with tempfile.TemporaryDirectory(prefix='sys-stream-') as directory:
    td = Path(directory)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for platform in ('emu', 'os'):
        source = Path(os.environ.get('SYS_STREAM_SOURCE', root / platform / 'port/inferno.c')).read_text()
        parts = []
        if 'static int\nsysstreamwrite(' in source:
            parts.append(function(source, 'static int\nsysstreamwrite('))
        parts.append(function(source, 'void\nSys_stream('))
        bodies.append('\n'.join(parts))
        target = td / (platform+'.c')
        target.write_text((root / 'libinterp/tests/sys-stream.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts)))
        for width in (32, 64):
            modeldir = td / (platform+str(width))
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            if width == 32:
                header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (modeldir / 'interp.h').write_text(header)
            includes = ['-I'+str(modeldir)] + ['-I'+str(root / p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
            binary = modeldir / 'stream'
            subprocess.run(cmd+includes+[str(target), '-o', str(binary)], check=True)
            subprocess.run([str(binary)]+os.environ.get('SYS_STREAM_CASE', '').split(), check=True, timeout=20)
    assert bodies[0] == bodies[1], 'emu and kernel stream bodies differ'
