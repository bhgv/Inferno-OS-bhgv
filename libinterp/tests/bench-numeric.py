#!/usr/bin/env python3
"""Execute production benchmark arithmetic, timer conversion and report readers."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Repository containing the benchmark device.
SOURCE = Path(os.environ.get('BENCH_NUMERIC_SOURCE', ROOT/'os/port/devbench.c')).read_text()  # Exact selected implementation.


# Borrow complete C definitions without counting braces in comments or literals.
def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', SOURCE[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return SOURCE[start:brace+token.end()]
    raise ValueError(signature)


start = SOURCE.index('typedef struct', SOURCE.index('/* DIS operation timing */'))
end = SOURCE.index('enum\n{\n\tHSIZE', start)
stats = SOURCE[start:end]
# Legacy sources have no mean accessor; expose their report expression verbatim.
if 'statmean(' not in stats:
    stats += '\n\n/* Replay the parent report expression without changing its arithmetic. */\nstatic vlong statmean(Stat *s) { return s->n ? s->sum/s->n : 0; }\n'
parts = [function(sig) for sig in ('static long\nrep(', 'static long\ncal(', 'static long\nbase(',
         'static void\ntimeop0(', 'static double\nts2us(', 'static vlong\nbus(', 'static vlong\nsdev(')]
parts.append('static long\n'+function('benchread('))
parts.insert(0, function('char*\nts2str(') if 'char*\nts2str(' in SOURCE else function('char *\nts2str('))
for name in ('tickspan', 'tickint', 'tickmicro'):
    signature = 'static vlong\n'+name+'('
    if signature in SOURCE:
        parts.insert(0, function(signature))
unit = (ROOT/'libinterp/tests/bench-numeric.c').read_text().replace('STATISTICS', stats).replace('IMPLEMENTATION', '\n\n\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='bench-numeric-') as directory:
    work = Path(directory)
    target = work/'fixture.c'
    target.write_text(unit)
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all'))
    command = shlex.split(os.environ.get('CC', 'cc'))+flags
    binary = work/'fixture'
    subprocess.run(command+['-I'+str(ROOT/'libinterp/tests'), '-I'+str(ROOT/'include'), str(target), '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
