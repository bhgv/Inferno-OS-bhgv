#!/usr/bin/env python3
"""Compare actual polygon packets and Frame64 Point consumers with an independent wide-integer encoder."""
import argparse
import os
from pathlib import Path
import random
import re
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Encode exact signed7 deltas/signed23 absolute values with unbounded Python arithmetic.
def encoded(points):
    output = bytearray()
    previous = [0, 0]
    for point in points:
        for axis, value in enumerate(point):
            delta = value-previous[axis]
            if -64 <= delta <= 63:
                output.append(delta&127)
            elif -4194304 <= value <= 4194303:
                output.extend((0x80|(value&127), (value>>7)&255, (value>>15)&255))
            else:
                return None
            previous[axis] = value
    return bytes(output)


# Boundary and deterministic randomized inputs retain independent exact compressed bytes.
def declarations():
    rng = random.Random(103064)
    cases = [[], [(0, 0)], [(63, -64), (127, -129)], [(64, -65), (-64, 63)],
             [(-4194304, 4194303), (-4194305, 4194304)], [(4194303, 0), (4194400, 0)],
             [(2147483647, -2147483648)], [(-2147483648, 2147483647)]]
    for _ in range(120):
        points = []
        x = y = 0
        for _ in range(rng.randint(1, 8)):
            x += rng.choice([-65, -64, -1, 0, 1, 63, 64, 8192])
            y += rng.choice([-8192, -65, -64, -1, 0, 63, 64, 65])
            points.append((x, y))
        cases.append(points)
    rows = []
    for points in cases:
        data = encoded(points)
        number = lambda v: '(-2147483647-1)' if v == -2147483648 else str(v)
        coords = ','.join('{'+','.join(map(number, p))+'}' for p in points) or '{0,0}'
        payload = ','.join(str(b) for b in (data or b'\0'))
        rows.append('{'+str(len(points))+','+str(-1 if data is None else len(data))+', {'+coords+'}, {'+payload+'}}')
    return '''typedef struct PolygonCase PolygonCase;

struct PolygonCase {
    int count; // Independent input population.
    int bytes; // Encoded payload length, or minus one for unrepresentable coordinates.
    Point points[8]; // Actual bounded input vertices.
    uchar encoded[48]; // Exact bytes computed independently of native packing code.
};
static PolygonCase cases[] = {
'''+',\n'.join(rows)+'\n}; // Independent signed coordinate expectations.\n'


# Preserve owning VM/graphics bodies and inject only explicit allocator/I/O/lookup test transports.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    draw = (ROOT/'libinterp/draw.c').read_text()
    names = ('drawpointdata', 'drawsplinepoly', 'drawfillsplinepoly', 'Image_poly', 'Image_polyop',
             'Image_bezspline', 'Image_bezsplineop', 'Image_fillpoly', 'Image_fillpolyop', 'Image_fillbezspline', 'Image_fillbezsplineop')
    consumer = '\n\n'.join(support['function'](draw, ('static int\n' if n == 'drawpointdata' else 'static void\n' if n.startswith('draw') else 'void\n')+n+'(') for n in names)
    consumer = consumer.replace('F_Image_', 'F_Draw_Image_').replace('\nImage_', '\nDraw_Image_')
    native = '\n\n'.join(re.sub(r'^#include[^\n]*\n', '', (ROOT/p).read_text(), flags=re.M) for p in ('libdraw/poly.c', 'libdraw/bezier.c', 'libdraw/arith.c'))
    native = '#define malloc(n) pointalloc(n) // Bounded test allocation transport.\n#define realloc(p,n) pointrealloc(p,n) // Preserve native failure semantics.\n#define free(p) pointfree(p) // Observe native retirement.\n'+native+'\n#undef malloc\n#undef realloc\n#undef free\n'
    fixture = (ROOT/'utils/tests/native64-polygons.c').read_text()
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    flags += ['-ffunction-sections', '-fdata-sections']
    arch = env.get('NATIVE64_ARCH', 'amd64')
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='native64-polygons-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', '-M', 'Draw', '-I'+str(ROOT/'module'), str(ROOT/'module/draw-native64.m')], env=env))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp', 'libdraw')]
        for mode in ('WORD32', 'WORD32-file', 'packet', 'count', 'absolute', 'array', 'retire'):
            body, vm = native, consumer
            if mode == 'packet': body = body.replace('BPLONG(a+19, src->id);', 'BPLONG(a+19, src->id-1);', 1)
            if mode == 'count': body = body.replace('np > 65536', '0', 1)
            if mode == 'absolute': body = body.replace('newx < -4194304 || newx > 4194303', '0', 1)
            if mode == 'array': vm = vm.replace('size != sizeof(Point) || pointers != 0', '0', 1)
            if mode == 'retire': body = body.replace('free(l->p);', '/* omitted native list retirement */', 1)
            source = work/(mode+'.c')
            source.write_text(support['portfixture'](fixture.replace('NATIVE_IMPLEMENTATION', body).replace('VM_IMPLEMENTATION', vm).replace('POLYGON_ORACLES', declarations()), '#include "lib9.h"'))
            binary = work/mode
            model = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode == 'WORD32-file' else [])
            subprocess.run(compiler+flags+model+includes+[str(source), '-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=90)
            if mode.startswith('WORD'):
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(mode+': '+result.stdout, end='', flush=True)
            elif result.returncode != 10 or 'polygon semantic mismatch' not in result.stderr:
                raise RuntimeError('Polygon control missed semantic predicate: '+mode+' '+result.stdout+result.stderr)
            else: print('PASS polygon semantic control:', mode, arch, flush=True)


if __name__ == '__main__':
    main()
