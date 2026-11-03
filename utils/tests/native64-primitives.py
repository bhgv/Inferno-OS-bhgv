#!/usr/bin/env python3
"""Check real native packets and fifteen Draw Frame64 consumers against wide Python geometry."""
import argparse
import os
from pathlib import Path
import random
import re
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NAMES = ('draw', 'drawop', 'gendraw', 'gendrawop', 'line', 'lineop', 'ellipse', 'ellipseop',
         'arc', 'arcop', 'fillellipse', 'fillellipseop', 'fillarc', 'fillarcop', 'border')


# Serialize the established native protocol independently of BPLONG and C arithmetic.
def packet(command, fields, op):
    return (bytes((ord('O'), op)) if op != 11 else b'')+command.encode()+b''.join(struct.pack('<I', x&0xffffffff) for x in fields)


# Expand mathematical inside/outside borders, refusing any unrepresentable coordinate before effects.
def border(r, sp, width):
    x, y, u, v = r
    sx, sy = sp
    if width < 0:
        x, y, u, v, sx, sy, width = x+width, y+width, u-width, v-width, sx+width, sy+width, -width
    edges = [([x, y, u, y+width], [sx, sy]), ([x, v-width, u, v], [sx, sy+v-y-width]),
             ([x, y+width, x+width, v-width], [sx, sy+width]),
             ([u-width, y+width, u, v-width], [sx+u-x-width, sy+width])]
    if any(not -2**31 <= a < 2**31 for rect, point in edges for a in rect+point): return []
    return [(rect, point) for rect, point in edges if rect[0] <= rect[2] and rect[1] <= rect[3]]


# Compute exact packets from API semantics, without reading native implementation/layout bytes.
def expected(name, mode, row):
    r, p0, p1, sp, a, b, width, e0, e1, alpha, phi, op, sd, md, foreign = row
    if not name.endswith('op'): op = 11
    if not 0 <= op < 12: return []
    basic = name.removesuffix('op')
    source = 13 if basic in ('draw', 'gendraw') and sd else 9
    if foreign == 1 and source == 9: return []
    if basic in ('draw', 'gendraw'):
        if r[0] > r[2] or r[1] > r[3] or (foreign == 2 and not md): return []
        mask = (19 if mode else 17) if md else 11
        origins = p0+p0 if basic == 'draw' else p0+p1
        return [packet('d', [7, source, mask]+r+origins, op)]
    if basic == 'line':
        return [] if width < 0 else [packet('L', [7]+p0+p1+[e0, e1, width, source]+sp, op)]
    if basic == 'border':
        if r[0] > r[2] or r[1] > r[3]: return []
        return [packet('d', [7, source, 17]+rect+point+point, op) for rect, point in border(r, sp, width)]
    filled = basic.startswith('fill')
    if a < 0 or b < 0 or (not filled and width < 0): return []
    arc = 'arc' in basic
    return [packet('E' if filled else 'e', [7, source]+p0+[a, b, 0 if filled else width]+sp+
                   [(alpha|0x80000000) if arc else 0, phi if arc else 0], op)]


# Retain edge values and deterministic full-range points, with bounded independent expected traces.
def declarations():
    rng = random.Random(106064)
    rows = []
    ints = [-2**31, 2**31-1, -1, 0, 1, -17, 2147483600, -2147483600]
    for i in range(100):
        point = lambda: [rng.choice(ints) if i < 50 else rng.randint(-2**31, 2**31-1) for _ in range(2)]
        p0, p1, sp = point(), point(), point()
        x, y = [-20, -2**31, 2**31-11, 0][i%4], [-30, -2**31, 2**31-21, 0][i%4]
        r = [x, y, x+10, y+20]
        if i%17 == 0: r[0], r[2] = r[2], r[0]
        row = (r, p0, p1, sp, [4, -1, 0, 2**31-1][i%4], [7, 0, -1, 2**31-1][i%4],
               [2, -1, -2**31, 2**31-1, 0, -3][i%6], rng.choice(ints), rng.choice(ints),
               rng.choice(ints), rng.choice(ints), [0, 1, 5, 11, -1, 12, 2**31-1][i%7],
               int(i%5 == 0), int(i%3 == 0), i%3 if i%11 == 0 else 0)
        for variant, name in enumerate(NAMES):
            for mode in (0, 1):
                messages = expected(name, mode, row)
                payload = b''.join(messages)
                number = lambda x: '(-2147483647-1)' if x == -2**31 else str(x)
                rectangle = '{{'+','.join(map(number, r[:2]))+'},{'+','.join(map(number, r[2:]))+'}}'
                pointc = lambda p: '{'+','.join(map(number, p))+'}'
                values = [str(variant), str(mode), rectangle, pointc(p0), pointc(p1), pointc(sp)]
                values += list(map(number, row[4:]))
                values += [str(len(messages)), str(len(messages[0]) if messages else 0), '{'+(','.join(map(str, payload)) or '0')+'}']
                rows.append('{'+','.join(values)+'}')
    return 'static PrimitiveCase cases[] = {\n'+',\n'.join(rows)+'\n}; // Independent wide-integer packet expectations.\n'


# Populate compiler-generated frames with unchanged argument bytes; production consumers must leave their arguments intact.
def dispatch():
    arms = []
    for variant, name in enumerate(NAMES):
        basic = name.removesuffix('op')
        assigns = ['f.dst = &objects[0];', 'f.src = basic_source;']
        if basic in ('draw', 'gendraw'):
            assigns += ['memmove(&f.r, &c->r, sizeof(f.r));', 'f.matte = basic_mask;']
            if basic == 'draw': assigns += ['memmove(&f.p, &c->p0, sizeof(f.p));']
            else: assigns += ['memmove(&f.p0, &c->p0, sizeof(f.p0));', 'memmove(&f.p1, &c->p1, sizeof(f.p1));']
        elif basic == 'line':
            assigns += ['memmove(&f.p0, &c->p0, sizeof(f.p0));', 'memmove(&f.p1, &c->p1, sizeof(f.p1));', 'f.end0=c->end0; f.end1=c->end1; f.radius=c->width;', 'memmove(&f.sp, &c->sp, sizeof(f.sp));']
        elif basic == 'border': assigns += ['memmove(&f.r, &c->r, sizeof(f.r));', 'f.i=c->width;', 'memmove(&f.sp, &c->sp, sizeof(f.sp));']
        else:
            assigns += ['memmove(&f.c, &c->p0, sizeof(f.c));', 'f.a=c->a; f.b=c->b;', 'memmove(&f.sp, &c->sp, sizeof(f.sp));']
            if not basic.startswith('fill'): assigns += ['f.thick=c->width;']
            if 'arc' in basic: assigns += ['f.alpha=c->alpha; f.phi=c->phi;']
        if name.endswith('op'): assigns += ['f.op=c->op;']
        if basic not in ('draw', 'gendraw'): assigns[1]='f.src = &objects[1];'
        arms.append('case '+str(variant)+': { F_Draw_Image_'+name+' f, copy; memset(&f, 0xa5, sizeof(f)); '+ ' '.join(assigns)+' memmove(&copy, &f, sizeof(f)); Draw_Image_'+name+'(&f); check(memcmp(&copy, &f, sizeof(f)) == 0); break; }')
    return '\n'.join(arms)


# Compile actual owning packet/VM bodies; only native I/O/lookup/locks remain explicit bounded transports.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    draw = (ROOT/'libinterp/draw.c').read_text()
    names = ('imagedraw', 'imagegendraw', 'drawline', 'drawarcellipse', 'drawfillarcellipse', 'drawborder')
    consumer = '\n\n'.join(support['function'](draw, 'static void\n'+n+'(') for n in names)
    consumer += '\n\n'+'\n\n'.join(support['function'](draw, 'void\nImage_'+n+'(') for n in NAMES)
    consumer = consumer.replace('F_Image_', 'F_Draw_Image_').replace('\nImage_', '\nDraw_Image_')
    native = '\n\n'.join(re.sub(r'^#include[^\n]*\n', '', (ROOT/p).read_text(), flags=re.M) for p in ('libdraw/draw.c', 'libdraw/line.c', 'libdraw/ellipse.c', 'libdraw/border.c', 'libdraw/arith.c'))
    helper = (ROOT/'libdraw/draw-packet.h').read_text()
    fixture = (ROOT/'utils/tests/native64-primitives.c').read_text()
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    command = shlex.split(env.get('CC', 'cc'))+shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    command += ['-ffunction-sections', '-fdata-sections']
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='native64-primitives-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory); env['TMPDIR'] = str(work)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', '-M', 'Draw', '-I'+str(ROOT/'module'), str(ROOT/'module/draw-native64.m')], env=env))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for mode in ('WORD32', 'WORD32-file', 'atomic', 'border', 'arc', 'display', 'radius', 'defaults', 'frame'):
            body, header, vm = native, helper, consumer
            if mode == 'atomic':
                old = support['function'](header, 'static uchar*\ndrawpacket(')
                header = header.replace(old, 'static uchar*\ndrawpacket(Display *d, int n, Drawop op) { if(d == nil || n < 0 || (unsigned int)op >= Ncomp) return nil; _setdrawop(d, op); return bufimage(d, n); }')
            if mode == 'border': body = body.replace('if(x < (-2147483647-1)', 'if(0 && x < (-2147483647-1)', 1).replace('|| x > 2147483647 || y < (-2147483647-1) || y > 2147483647)', ')', 1)
            if mode == 'arc': body = body.replace('(u32int)alpha|0x80000000U', '(u32int)alpha')
            if mode == 'display': body = body.replace('dst->display != src->display || radius < 0', '0 || radius < 0', 1)
            if mode == 'radius': body = body.replace('radius < 0)', '0)', 1)
            if mode == 'defaults': body = body.replace('src = dst->display->black;', 'src = dst->display->opaque;', 1)
            if mode == 'frame': vm = vm.replace('IPOINT(f->p0), m, IPOINT(f->p1)', 'IPOINT(f->p1), m, IPOINT(f->p0)', 1)
            source = work/(mode+'.c')
            text = fixture.replace('PACKET_HELPER', header).replace('NATIVE_IMPLEMENTATION', body).replace('VM_IMPLEMENTATION', vm).replace('FRAME_DISPATCH', dispatch()).replace('PRIMITIVE_ORACLES', declarations())
            source.write_text(support['portfixture'](text, '#include "lib9.h"'))
            binary = work/mode
            flags = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode.endswith('file') else [])
            subprocess.run(command+flags+includes+[str(source), '-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=90)
            if mode.startswith('WORD'):
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(mode+': '+result.stdout, end='', flush=True)
            elif result.returncode != 10 or 'primitive semantic mismatch' not in result.stderr:
                raise RuntimeError('Primitive control missed predicate: '+mode+' '+result.stdout+result.stderr)
            else: print('PASS primitive semantic control:', mode, arch, flush=True)


if __name__ == '__main__':
    main()
