#!/usr/bin/env python3
"""Run both production Sys.stream implementations with actual VM frame declarations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]

PORTS = os.environ.get('SYS_STREAM_PORTS', 'emu os').split()
MODELS = os.environ.get('SYS_STREAM_MODELS', '32 64').split()
if not PORTS or any(port not in ('emu', 'os') for port in PORTS):
    raise ValueError('unknown native Sys port')
if not MODELS or any(model not in ('32', '64') for model in MODELS):
    raise ValueError('unknown native Sys WORD model')

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
    cmd += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    for platform in PORTS:
        source = Path(os.environ.get('SYS_STREAM_SOURCE', root / platform / 'port/inferno.c')).read_text()
        parts = []
        parts += [function(source, sig) for sig in ('static int\nsysnativeint(', 'static int\nsysiofd(', 'static void\nsysstatusframe(') if sig in source]
        if 'static int\nsysstreamwrite(' in source:
            parts.append(function(source, 'static int\nsysstreamwrite('))
        parts.append(function(source, 'void\nSys_stream('))
        bodies.append('\n'.join(parts))
        target = td / (platform+'.c')
        target.write_text((root / 'libinterp/tests/sys-stream.c').read_text().replace('IMPLEMENTATION', '\n'.join(parts)))
        for width in map(int, MODELS):
            modeldir = td / (platform+str(width))
            modeldir.mkdir()
            header = (root / 'include/interp.h').read_text()
            if width == 32:
                header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            (modeldir / 'interp.h').write_text(header)
            includes = ['-I'+str(modeldir)] + ['-I'+str(root / p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
            binary = modeldir / 'stream'
            subprocess.run(cmd+includes+['-pthread',str(target), '-o', str(binary)], check=True)
            subprocess.run([str(binary)]+os.environ.get('SYS_STREAM_CASE', '').split(), check=True, timeout=60)
            if os.environ.get('STREAM_PREFLIGHT'):
                for mode in range(10):
                    subprocess.run([str(binary), 'preflight', str(mode)], check=True, timeout=20)
    if len(bodies) == 2 and bodies[0] != bodies[1]:
        raise RuntimeError('emu and kernel stream bodies differ')
