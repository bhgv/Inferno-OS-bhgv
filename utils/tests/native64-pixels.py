#!/usr/bin/env python3
"""Exercise actual pixel protocol libraries and VM consumers with independent wide geometry."""
import argparse
import os
from pathlib import Path
import random
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Derive byte extents using unlimited Python integers before any native narrowing.
def extent(r, depth, capacity):
    x, y, u, v = r
    if not 1 <= depth <= 32 or capacity < 0 or u < x or v < y:
        return None
    if u == x or v == y:
        return 0, 0
    stride = -((-u*depth)//8)-x*depth//8
    total = stride*(v-y)
    return (stride, total) if total <= capacity else None


# Build independent clipped geometry, including large coordinates with tiny valid storage.
def declarations():
    rng = random.Random(97064)
    rows = []
    for index in range(80):
        x = [-2147483648, 2147483600, -17, -1, 0, 1, 32767, -32768][index%8]
        y = [-2147483648, 2147483600, -7, 0][index%4]
        width, height = rng.randint(1, 25), rng.randint(1, 9)
        depth = [1, 2, 4, 8, 16, 24, 32][index%7]
        r = [x, y, x+width, y+height]
        image = r[:]
        clip = [x+width//3, y+height//3, x+width, y+height]
        capacity = 32768
        if index%13 == 0:
            capacity = 0
        rows.append((r, image, clip, depth, capacity))
    rows += [([0, 0, 10, 10], [1, 1, 2, 2], [1, 1, 2, 2], 8, 100),
             ([0, 0, 2, 2], [0, 0, 2, 2], [4, 4, 8, 8], 8, 4),
             ([0, 0, 1, 1], [0, 0, 1, 1], [0, 0, 1, 1], 8, -1),
             ([0, 0, 4000, 3], [0, 0, 4000, 3], [0, 0, 4000, 3], 8, 12000),
             ([0, 0, 9000, 1], [0, 0, 9000, 1], [0, 0, 9000, 1], 8, 9000),
             ([-2147483648, -2147483648, 2147483647, 2147483647], [-2147483648]*2+[2147483647]*2, [0, 0, 1, 1], 32, 32768),
             ([0, 0, 0, 1], [0, 0, 1, 1], [0, 0, 1, 1], 8, 0),
             ([0, 0, 1, 0], [0, 0, 1, 1], [0, 0, 1, 1], 8, 0),
             ([1, 1, 0, 0], [0, 0, 1, 1], [0, 0, 1, 1], 8, 16),
             ([0, 0, 1, 1], [0, 0, 1, 1], [0, 0, 1, 1], 0, 16),
             ([0, 0, 1, 1], [0, 0, 1, 1], [0, 0, 1, 1], 33, 16)]
    generated = []
    for r, image, clip, depth, capacity in rows:
        span = extent(r, depth, capacity)
        clipped = [max(r[0], image[0], clip[0]), max(r[1], image[1], clip[1]),
                   min(r[2], image[2], clip[2]), min(r[3], image[3], clip[3])]
        overlap = clipped[0] < clipped[2] and clipped[1] < clipped[3]
        bpl, total = span or (0, 0)
        cspan = extent(clipped, depth, capacity) if overlap else (0, 0)
        cbpl, ctotal = cspan or (0, 0)
        load = -1 if span is None else (0 if total == 0 or not overlap else (-1 if cbpl > 448 else ctotal))
        contained = image[0] <= r[0] and image[1] <= r[1] and r[2] <= image[2] and r[3] <= image[3]
        unload = -1 if span is None or bpl > 8000 or (total and not contained) else total
        offset = ((clipped[1]-r[1])*bpl+clipped[0]*depth//8-r[0]*depth//8) if overlap else 0
        encode = lambda v: str(v) if v != -2147483648 else '(-2147483647-1)'
        rect = lambda v: '{{'+','.join(map(encode, v[:2]))+'},{'+','.join(map(encode, v[2:]))+'}}'
        generated.append('{'+','.join([rect(r), rect(image), rect(clip), str(depth), str(capacity), str(load), str(unload), rect(clipped), str(bpl), str(cbpl), str(offset)+'LL'])+'}')
    return '''typedef struct PixelCase PixelCase;

struct PixelCase {
    Rectangle r; // Requested source/destination pixel rectangle.
    Rectangle image; // Independent image extent.
    Rectangle clip; // Independent write clipping extent.
    int depth; // Native channel depth under test.
    int capacity; // Actual bounded byte capacity, never an allocation request.
    int load; // Independent expected write result.
    int unload; // Independent expected read result.
    Rectangle clipped; // Independent clipped write coordinates.
    int stride; // Independent complete source row stride.
    int linebytes; // Independent clipped row stride.
    vlong offset; // Independent first clipped source-byte offset.
};
static PixelCase cases[] = {
'''+',\n'.join(generated)+'''
}; // Independent arbitrary-precision geometry expectations.


/* Compare every actual message and result against independent geometry. */
static void
exercise(PixelCase *value)
{
    int operation, fault, result, expected;
    Rectangle target;

    for(operation=0; operation<2; operation++)
    for(fault=0; fault<10; fault++){
        setup();
        image.r = value->image;
        image.clipr = value->clip;
        image.depth = value->depth;
        kind = operation ? 'r' : 'y';
        target = operation ? value->r : value->clipped;
        expectedx = target.min.x; expectedy = target.min.y;
        width = (vlong)target.max.x-target.min.x;
        height = (vlong)target.max.y-target.min.y;
        stride = value->stride;
        linebytes = operation ? value->stride : value->linebytes;
        start = operation ? 0 : value->offset;
        limit = value->capacity;
        if(fault == 1) allocfail = 1;
        if(fault == 2) flushfail = 1;
        if(fault == 3) readmode = 1;
        if(fault == 4) readmode = 2;
        if(fault == 5) readmode = 3;
        if(fault == 6) readmode = 4;
        if(fault == 9) image.display = nil;
        expected = operation ? value->unload : value->load;
        if(expected > 0 && (allocfail || flushfail || (operation && readmode && (readmode < 3 || expected > 7))))
            expected = -1;
        if(fault == 7 && value->stride > 0)
            expected = -1;
        if(fault == 8 || fault == 9)
            expected = -1;
        result = operation ? unloadimage(fault == 8 ? nil : &image, value->r, fault == 7 ? nil : storage, value->capacity) :
            loadimage(fault == 8 ? nil : &image, value->r, fault == 7 ? nil : storage, value->capacity);
        check(result == expected);
        check(balance == 0);
        if(operation && result < 0 && reads > 0 && (readmode == 2 || readmode == 4))
            check(diagnostic == 2);
        if((operation ? value->unload : value->load) < 0)
            check(requests == 0 && flushes == 0 && reads == 0);
        if(expected > 0)
            check(row == height);
    }
}
'''


# Link real pixel libraries and exact owning VM bodies across explicit bounded transports.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))+['-ffunction-sections', '-fdata-sections']
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    draw = (ROOT/'libinterp/draw.c').read_text()
    consumer = '\n\n'.join(support['function'](draw, signature) for signature in
                          ('static int\ndrawpixeldata(', 'void\nImage_readpixels(', 'void\nImage_writepixels('))
    consumer = consumer.replace('F_Image_', 'F_Draw_Image_').replace('\nImage_', '\nDraw_Image_')
    fixture = (ROOT/'utils/tests/native64-pixels.c').read_text().replace('PIXEL_CONSUMERS', consumer)
    fixture = fixture.replace('PIXEL_ORACLES', 'static void setup(void); // Shared bounded transport reset.\n'+declarations())
    with tempfile.TemporaryDirectory(prefix='native64-pixels-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', '-M', 'Draw', '-I'+str(ROOT/'module'), str(ROOT/'module/draw-native64.m')], env=env))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp', 'libdraw')]
        (work/'fixture.c').write_text(support['portfixture'](fixture, '#include "lib9.h"'))
        for mode in ('WORD32', 'WORD32-file', 'packet'):
            model = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode == 'WORD32-file' else [])
            objects = []
            for index, path in enumerate(('libdraw/loadimage.c', 'libdraw/unloadimage.c', 'libdraw/rectclip.c', 'libdraw/arith.c')):
                body = (ROOT/path).read_text()
                if mode == 'packet' and index == 0:
                    old = 'BPLONG(a+13, dstr.max.x);'
                    if body.count(old) != 1:
                        raise RuntimeError('Pixel packet control drift')
                    body = body.replace(old, 'BPLONG(a+13, dstr.max.x-1);', 1)
                source = work/(str(index)+'.c'); obj = work/(str(index)+'.o')
                source.write_text(support['portfixture'](body, '#include "lib9.h"'))
                subprocess.run(compiler+flags+model+includes+['-c', str(source), '-o', str(obj)], env=env, check=True)
                objects.append(str(obj))
            binary = work/mode
            subprocess.run(compiler+flags+model+includes+[str(work/'fixture.c')]+objects+['-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=90)
            if mode == 'packet':
                if result.returncode != 10 or 'Pixel transfer mismatch' not in result.stderr:
                    raise RuntimeError('Pixel control missed semantic failure: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS pixel protocol control:', arch, flush=True)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(mode+': '+result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
