#!/usr/bin/env python3
"""Execute real emu/os/benchmark GC waiters across unsigned epoch and signed color limits."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Borrow actual waiter definitions while ignoring comment/literal braces.
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


if os.environ.get('GC_IDLE_PLATFORM') not in (None, 'emu', 'os', 'bench'):
    raise ValueError('GC_IDLE_PLATFORM must select emu, os or bench')

with tempfile.TemporaryDirectory(prefix='gc-idle-') as temporary:
    work = Path(temporary)
    for name, path in [('emu', 'emu/port/dis.c'), ('os', 'os/port/dis.c'), ('bench', 'os/port/devbench.c')]:
        if os.environ.get('GC_IDLE_PLATFORM') and name != os.environ['GC_IDLE_PLATFORM']:
            continue
        code = Path(os.environ.get('GC_IDLE_SOURCE', ROOT/path)).read_text()
        definition = function(code, 'int\nidlegc(' if name == 'bench' else 'static void\nexecatidle(')
        fixture = (ROOT/'libinterp/tests/gc-idle.c').read_text().replace('IMPLEMENTATION', definition)
        if name == 'bench':
            fixture = '#define TEST_BENCH 1 // Execute the actual benchmark waiter.\n'+fixture
        source = work/(name+'.c')
        binary = work/name
        source.write_text(fixture)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'emu/port', 'include')]
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        print('waiter '+name, flush=True)
        subprocess.run([str(binary)], check=True)
