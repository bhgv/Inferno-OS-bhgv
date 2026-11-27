#!/usr/bin/env python3
"""Compare real bounded native curve geometry with an independent arbitrary-precision legacy-order oracle."""
import os
from pathlib import Path
import random
import re
import resource
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Match signed integer division without floats or host-sized intermediate products.
def divide(value, divisor):
    return -(abs(value)//divisor) if value < 0 else value//divisor


# Reference squared distance uses exact arbitrary-precision integer projection and endpoint clamps.
def distance(p, a, b):
    p = tuple(x-y for x, y in zip(p, a))
    b = tuple(x-y for x, y in zip(b, a))
    num = sum(x*y for x, y in zip(p, b))
    den = sum(x*x for x in b)
    if num <= 0: return sum(x*x for x in p)
    if num >= den: return sum((x-y)**2 for x, y in zip(b, p))
    return sum((divide(x*num, den)-y)**2 for x, y in zip(b, p))


# Preserve established integer subdivision order with Python integers, not native coordinate widths.
def cubic(points):
    result = []
    def visit(p, scale, depth):
        if depth > 60: raise RuntimeError('Reference depth exceeded for ordinary-domain case')
        q = [tuple(divide(v, scale) for v in pt) for pt in p]
        if distance(q[1], q[0], q[3]) <= 1 and distance(q[2], q[0], q[3]) <= 1:
            result.extend(q[:3])
            return
        if scale > 4096: p, scale = q, 1
        add = lambda a, b: tuple(x+y for x, y in zip(a, b))
        mul = lambda a, b: tuple(x*b for x in a)
        a, b, c = add(p[0], p[1]), add(p[1], p[2]), add(p[2], p[3])
        d, e = add(a, b), add(b, c)
        f = add(d, e)
        visit([mul(p[0], 8), mul(a, 4), mul(d, 2), f], scale*8, depth+1)
        visit([f, mul(e, 2), mul(c, 4), mul(p[3], 8)], scale*8, depth+1)
    visit(points, 1, 0)
    result.append(points[3])
    if len(result) > 65536: raise RuntimeError('Reference fixture exceeds its vertex policy')
    return result


# Build spline control points from exact weighted averages before applying the independent curve oracle.
def spline(points):
    if len(points) < 3: return []
    mix = lambda a, x, b, y, n: tuple(divide(u*x+v*y, n) for u, v in zip(a, b))
    periodic = points[0] == points[-1]
    result = []
    if periodic:
        a = mix(points[-2], 1, points[0], 1, 2)
        b = mix(points[-2], 1, points[0], 5, 6)
        c = mix(points[0], 5, points[1], 1, 6)
        d = mix(points[0], 1, points[1], 1, 2)
        result.extend(cubic([a, b, c, d])[:-1])
    for i in range(len(points)-2):
        p, q, r = points[i:i+3]
        a, b = (p, mix(p, 1, q, 2, 3)) if i == 0 and not periodic else (mix(p, 1, q, 1, 2), mix(p, 1, q, 5, 6))
        c, d = (mix(q, 2, r, 1, 3), r) if i == len(points)-3 and not periodic else (mix(q, 5, r, 1, 6), mix(q, 1, r, 1, 2))
        result.extend(cubic([a, b, c, d])[:-1])
    result.append(d)
    return result


# Ordinary/nonzero/transformed domains have exact references; full-span cases have explicit bounded properties.
def cases():
    rng = random.Random(104064)
    rows = []
    base = [[(0, 0), (8, 8), (9, 9), (1, 1)], [(0, 0), (0, 2000), (3000, -2000), (4000, 0)],
            [(0, 0)]*4, [(-20000, -20000), (20000, 20000), (-20000, 20000), (20000, -20000)]]
    for _ in range(15): base.append([(rng.randint(-4096, 4096), rng.randint(-4096, 4096)) for _ in range(4)])
    for points in base:
        for offset in (0, 2147483647-65536, -2147483648+65536):
            data = [(x+offset, y+offset) for x, y in points]
            expected = cubic(data)
            rows.append((2, data, expected))
    for count in (0, 1, 2, 3, 4, 8):
        points = [(rng.randint(-64, 64), rng.randint(-64, 64)) for _ in range(count)]
        for offset in (0, 2147483647-65536, -2147483648+65536):
            data = [(x+offset, y+offset) for x, y in points]
            rows.append((3 if count >= 3 else 1, data, spline(data)))
            if count >= 3:
                data = data[:-1]+[data[0]]
                rows.append((3 if count >= 3 else 1, data, spline(data)))
    lo, hi = -2147483648, 2147483647
    rows.append((0, [(lo, lo), (lo, lo), (hi, hi), (hi, hi)], [(lo, lo), (lo, lo), (hi, hi), (hi, hi)]))
    rows.append((0, [(lo, lo), (-715827882, -715827882), (715827881, 715827881), (hi, hi)], -3))
    rows.append((0, [(lo, lo), (hi, lo), (lo, hi), (hi, hi)], -2))
    return rows


# Compile actual source and fail every weakened guard at semantic10 before unsafe work.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    code = re.sub(r'^#include[^\n]*\n', '', (ROOT/'libdraw/bezier.c').read_text(), flags=re.M)
    eq = support['function']((ROOT/'libdraw/arith.c').read_text(), 'int\neqpt(')
    prefix = '#define malloc(n) curvealloc(n) // Native test allocator.\n#define realloc(p,n) curverealloc(p,n) // Preserve native owner replacement.\n#define free(p) curvefree(p) // Observe every native retirement.\n'
    fixture = (ROOT/'utils/tests/native64-curves.c').read_text()
    with tempfile.TemporaryDirectory(prefix='native64-curves-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        rows = cases()
        data = bytearray(struct.pack('>i', len(rows)))
        for mode, points, expected in rows:
            data += struct.pack('>iii', mode, len(points), len(expected) if isinstance(expected, list) else expected)
            for point in points+(expected if isinstance(expected, list) else []): data += struct.pack('>ii', *point)
        (work/'cases.bin').write_bytes(data)
        includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for mode in (None, 'work', 'depth', 'points', 'origin', 'projection', 'growth'):
            body = code
            if mode == 'work': body = body.replace('l->work == 0 || depth > BezierDepth', 'depth > BezierDepth', 1)
            if mode == 'depth': body = body.replace('l->work == 0 || depth > BezierDepth', 'l->work == 0', 1)
            if mode == 'points': body = body.replace('if(l->np >= BezierPoints)', 'if(0)', 1)
            if mode == 'origin':
                actual = support['function'](body, 'static int\nbeziersource(')
                changed = actual.replace('if(x < (-0x7fffffff-1) || x > 0x7fffffff || y < (-0x7fffffff-1) || y > 0x7fffffff)', 'if(0)', 1)
                body = body.replace(actual, changed, 1)
            if mode == 'growth': body = body.replace('l->capacity*2', 'l->capacity+PINC', 1)
            if mode == 'projection': body = body.replace('if(num >= den)', 'if(0 && num >= den)', 1)
            if mode is not None and body == code: raise RuntimeError('Curve control drift: '+mode)
            source = work/((mode or 'normal')+'.c')
            source.write_text(support['portfixture'](fixture.replace('CURVE_IMPLEMENTATION', eq+'\n'+prefix+body+'\n#undef malloc\n#undef realloc\n#undef free\n'), '#include "lib9.h"'))
            binary = work/(mode or 'normal')
            subprocess.run(compiler+flags+includes+[str(source), '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary), str(work/'cases.bin')], env=env, capture_output=True, text=True, timeout=90)
            if mode is None:
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)
            elif result.returncode != 10 or 'curve semantic mismatch' not in result.stderr:
                raise RuntimeError('Curve control missed semantic predicate: '+str(mode)+' '+result.stdout+result.stderr)
            else: print('PASS curve semantic control:', mode, arch, flush=True)


if __name__ == '__main__':
    main()
