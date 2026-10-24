#!/usr/bin/env python3
"""Compare production geometry with arbitrary-precision Python coordinate oracles."""
import os
from pathlib import Path
import random
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
geom = Path(os.environ.get('GEOMETRY_SOURCE', root/'libinterp/geom.c')).read_text()
draw = Path(os.environ.get('DRAW_ARITH_SOURCE', root/'libdraw/arith.c')).read_text()
unit = (root/'libinterp/tests/geometry-arithmetic.c').read_text()
unit = unit.replace('IMPLEMENTATION', re.sub(r'^#include[^\n]*\n', '', geom+draw, flags=re.M))


vm_operations = ['Point_add', 'Point_sub', 'Point_mul', 'Point_div',
                 'Rect_addpt', 'Rect_subpt', 'Rect_inset', 'Rect_dx', 'Rect_dy', 'Rect_size']
native_operations = ['addpt', 'subpt', 'mulpt', 'divpt', 'rectaddpt', 'rectsubpt', 'insetrect']
case = os.environ.get('GEOMETRY_CASE', '')
operation = os.environ.get('GEOMETRY_OP')
if case not in ('', 'vm', 'native'):
    raise ValueError('unknown GEOMETRY_CASE: '+case)
if operation is not None and operation not in vm_operations+native_operations:
    raise ValueError('unknown GEOMETRY_OP: '+operation)


def vectors(bits):
    """Compute wrapped results with unlimited integers and truncating signed division."""
    mask = (1 << bits)-1
    limit = 1 << (bits-1)
    edges = [0, 1, 2, 3, 7, 255, 256, 65535, 65536, limit-2,
             limit-1, limit, limit+1, mask-2, mask-1, mask]
    rng = random.Random(0x6c696d626f)
    inputs = [[x, y, y, x, y, x, y] for x in edges for y in edges]
    inputs += [[rng.getrandbits(bits) for _ in range(7)] for _ in range(512)]
    rows = []
    for raw in inputs:
        a, b, c, d, n, qx, qy = [v if v < limit else v-(1 << bits) for v in raw]
        def divide(v):
            if not n:
                return 0
            quotient = abs(v)//abs(n)
            return -quotient if (v < 0) != (n < 0) else quotient
        expected = [a+qx, b+qy, a-qx, b-qy, a*n, b*n, divide(a), divide(b),
                    a+qx, b+qy, c+qx, d+qy, a-qx, b-qy, c-qx, d-qy,
                    a+n, b+n, c-n, d-n, c-a, d-b, c-a, d-b]
        values = lambda seq: ','.join('0x%xULL' % (v & mask) for v in seq)
        rows.append('{{'+values(raw)+'},{'+values(expected)+'}}')
    return 'static const Test vectors[] = {\n'+',\n'.join(rows)+'\n};\n'


with tempfile.TemporaryDirectory(prefix='geometry-arithmetic-') as directory:
    td = Path(directory)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64, 32):
        if bits == 64 and not os.environ.get('GEOMETRY_PROBE_PARENT') and (case == 'native' or operation in native_operations):
            continue
        includes = []
        if bits == 32:
            model = td/'word32'
            model.mkdir()
            header = (root/'include/interp.h').read_text()
            header, n = re.subn(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header, m = re.subn(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
            assert n == m == 1
            (model/'interp.h').write_text(header)
            includes = ['-I'+str(model)]
        includes += ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
        source = td/('geometry'+str(bits)+'.c')
        source.write_text(unit.replace('VECTORS', vectors(bits)))
        binary = td/('geometry'+str(bits))
        subprocess.run(cmd+includes+[str(source), '-o', str(binary)], check=True)
        if os.environ.get('GEOMETRY_PROBE_PARENT'):
            operations = list(vm_operations)
            if bits == 32:
                operations += native_operations
            for operation in operations:
                env = dict(os.environ, GEOMETRY_OP=operation)
                probe = subprocess.run([str(binary)], env=env, text=True, capture_output=True)
                assert probe.returncode != 0 and 'runtime error:' in probe.stderr, (operation, probe.stderr)
            print('PASS independent parent failures: %d entry points, WORD%d' % (len(operations), bits))
        else:
            subprocess.run([str(binary)]+shlex.split(os.environ.get('GEOMETRY_CASE', '')), check=True)
