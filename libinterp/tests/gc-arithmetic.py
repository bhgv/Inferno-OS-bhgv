#!/usr/bin/env python3
"""Check actual GC mark callbacks and arithmetic against arbitrary-precision host vectors."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PRODUCTION = Path(os.environ.get('GC_ARITH_SOURCE', ROOT/'libinterp/gc.c'))
SOURCE = PRODUCTION.read_text()


# Ignore braces embedded in C literals and comments when borrowing definitions.
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
    raise RuntimeError(signature)


parts = []
for signature in ('static int\ngcintadd(', 'static void\ngcspend(', 'static int\ngcnextquanta(', 'static int\ngcnextcolor('):
    if signature in SOURCE:
        parts.append(function(signature))
if 'static int\ngcnextquanta(' not in SOURCE:
    start = SOURCE.index('\tquanta = (MaxQuanta+Quanta)/2')
    end = SOURCE.index('\n\n\tif(base != nil)', start)
    parts.append('''/* Isolate the original production feedback statements for before-source replay. */
static int
gcnextquanta(int balance, int inspected)
{
    gce = balance;
    gct = inspected;
'''+SOURCE[start:end]+'''
    return quanta;
}
''')
    parts.append('''/* Supply the historical int arithmetic only for isolated old-source fixtures. */
static int
gcintadd(int value, int delta)
{
    return value+delta;
}
''')
    parts.append('''/* Model the old unbounded work subtraction for otherwise unused helper probes. */
static void
gcspend(uvlong count)
{
    visit -= (WORD)count;
}
''')
    statement = re.search(r'\bgccolor\+\+;', SOURCE).group()
    parts.append('''/* Execute the exact original color-increment statement in a bounded probe. */
static int
gcnextcolor(int color)
{
    gccolor = color;
    '''+statement+'''
    return gccolor;
}
''')
parts += [function(signature) for signature in ('void\nmarkheap(', 'void\nmarkarray(', 'void\nmarklist(', 'static void\nrootset(')]
lo, hi = -(1 << 31), (1 << 31)-1
seeds = [lo, lo+1, -100, -1, 0, 1, 49, 50, hi-1, hi]
counts = [0, 1, 2, 3, 50, hi, hi+1, (1 << 32)-1, 1 << 32, 1 << 63, (1 << 64)-1]
spend = '\n'.join('{%d,%dULL,%d},' % (seed, count, max(lo, seed-count)) for seed in seeds for count in counts)
adds = '\n'.join('{%d,%d,%d},' % (seed, delta, min(hi, max(lo, seed+delta))) for seed in seeds for delta in seeds)
quanta = []
for balance in seeds:
    for scanned in [1, 2, 3, 50, 750, hi-1, hi, 0, -1]:
        ratio = abs(100*balance)//scanned if scanned > 0 else 0
        if balance < 0:
            ratio = -ratio
        expected = min(750, max(50, 400+35*ratio)) if scanned > 0 else 50
        quanta.append('{%d,%d,%d},' % (balance, scanned, expected))
with tempfile.TemporaryDirectory(prefix='gc-arithmetic-') as temporary:
    work = Path(temporary)
    source = work/'arithmetic.c'
    fixture = (ROOT/'libinterp/tests/gc-arithmetic.c').read_text()
    for name, value in [('IMPLEMENTATION', '\n\n\n'.join(parts)), ('SPEND_VECTORS', spend), ('ADD_VECTORS', adds), ('QUANTA_VECTORS', '\n'.join(quanta))]:
        fixture = fixture.replace(name, value)
    source.write_text(fixture)
    widths = tuple(map(int, os.environ.get('GC_ARITH_WORD_BITS', '32 64').split()))
    if not widths:
        raise ValueError('select at least one WORD width')
    for bits in widths:
        if bits not in (32, 64):
            raise ValueError(bits)
        model = work/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        command += ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'emu/port', 'include')]
        binary = model/'arithmetic'
        subprocess.run(command+['-c', str(PRODUCTION), '-o', str(model/'collector.o')], check=True)
        subprocess.run(command+[str(source), '-o', str(binary)], check=True)
        for family in ([os.environ['GC_ARITH_CASE']] if os.environ.get('GC_ARITH_CASE') else ['integers', 'quanta', 'color', 'heap', 'array', 'list', 'balance']):
            subprocess.run([str(binary), family], check=True)
