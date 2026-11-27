#!/usr/bin/env python3
"""Compile the actual Draw interface and all geometry bodies against independent integer/alias oracles."""
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


# Interpret raw int32 inputs independently with Python's unlimited integers.
def expected(name, fields, bits):
    values = {}
    cursor = 0
    for kind, field in fields:
        count = {'WORD': 1, 'Draw_Point': 2, 'Draw_Rect': 4}[kind]
        signed = [v if v < 0x80000000 else v-0x100000000 for v in bits[cursor:cursor+count]]
        values[field] = signed[0] if count == 1 else signed
        cursor += count
    p, q = values.get('p'), values.get('q')
    r, s = values.get('r'), values.get('s')
    overlap = r is not None and s is not None and r[0] < s[2] and s[0] < r[2] and r[1] < s[3] and s[1] < r[3]
    if name.startswith('Point_'):
        op = name[6:]
        if op in ('add', 'sub'):
            return [p[i]+(q[i] if op == 'add' else -q[i]) for i in (0, 1)], False
        if op == 'mul':
            return [x*values['i'] for x in p], False
        if op == 'div':
            n = values['i']
            if n == 0:
                return [], True
            return [(abs(x)//abs(n))*(-1 if (x < 0) != (n < 0) else 1) for x in p], False
        if op == 'eq':
            return [int(p == q)], False
        if op == 'in':
            return [int(r[0] <= p[0] < r[2] and r[1] <= p[1] < r[3])], False
    else:
        op = name[5:]
        if op == 'canon':
            return [min(r[0], r[2]), min(r[1], r[3]), max(r[0], r[2]), max(r[1], r[3])], False
        if op == 'combine':
            return [min(r[0], s[0]), min(r[1], s[1]), max(r[2], s[2]), max(r[3], s[3])], False
        if op == 'eq':
            return [int(r == s)], False
        if op == 'Xrect':
            return [int(overlap)], False
        if op == 'clip':
            return ([max(r[0], s[0]), max(r[1], s[1]), min(r[2], s[2]), min(r[3], s[3]), 1] if overlap else r+[0]), False
        if op == 'inrect':
            return [int(s[0] <= r[0] and r[2] <= s[2] and s[1] <= r[1] and r[3] <= s[3])], False
        if op == 'contains':
            return [int(r[0] <= p[0] < r[2] and r[1] <= p[1] < r[3])], False
        if op in ('addpt', 'subpt'):
            return [x+(p[i%2] if op == 'addpt' else -p[i%2]) for i, x in enumerate(r)], False
        if op == 'inset':
            return [x+(values['n'] if i < 2 else -values['n']) for i, x in enumerate(r)], False
        if op == 'dx':
            return [r[2]-r[0]], False
        if op == 'dy':
            return [r[3]-r[1]], False
        if op == 'size':
            return [r[2]-r[0], r[3]-r[1]], False
    raise RuntimeError('Unrecognized owning geometry: '+name)


# Use actual generated field offsets, but independently generate every expected result.
def oracle(header, names):
    output, specs = [], []
    edges = [0, 1, 0xffffffff, 0x7fffffff, 0x80000000, 37, 0xffffffc5, 0x80000001]
    rng = random.Random(0x64726177)
    for name in names:
        frame = 'F_Draw_'+name
        match = re.search(r'struct '+frame+r' \{(.*?)\n\};', header, re.S)
        if match is None:
            raise RuntimeError('Missing real frame: '+frame)
        fields = [(kind, field) for kind, field, offset in re.findall(r'\n\s*(WORD|Draw_Point|Draw_Rect) (\w+); // Offset (\d+),', match.group(1)) if int(offset) >= 64]
        if not 1 <= len(fields) <= 2:
            raise RuntimeError('Unexpected geometry arguments: '+name)
        rows = []
        for row in range(128):
            count = sum({'WORD': 1, 'Draw_Point': 2, 'Draw_Rect': 4}[kind] for kind, _ in fields)
            raw = [(edges[(row+i*3)%len(edges)] if row < 64 else rng.getrandbits(32)) for i in range(count)]
            if name in ('Point_eq', 'Rect_eq', 'Rect_Xrect', 'Rect_clip', 'Rect_combine', 'Rect_inrect') and row%4 == 0:
                half = count//2
                raw[half:] = raw[:half]
            if name == 'Point_div' and row%8 == 0:
                raw[-1] = 0
            result, failure = expected(name, fields, raw)
            encode = lambda words: ','.join('0x%08xU' % (v&0xffffffff) for v in words) or '0'
            rows.append('{{'+encode(raw)+'},{'+encode(result)+'},'+str(int(failure))+'}')
        output.append('static GeometryCase cases_'+name+'[] = {\n'+',\n'.join(rows)+'\n}; // Independent Python coordinate oracle.\n')
        offsets = ','.join('offsetof('+frame+','+field+')' for _, field in fields)
        widths = ','.join('sizeof((('+frame+'*)0)->'+field+')/4' for _, field in fields)
        specs.append('{"'+name+'",Draw_'+name+',sizeof('+frame+'),offsetof('+frame+',ret),sizeof(*(('+frame+'*)0)->ret),'+str(len(fields))+',{'+offsets+'},{'+widths+'},cases_'+name+',128}')
    output.append('static GeometrySpec specifications[] = {\n'+',\n'.join(specs)+'\n}; // Actual generated C geometry, not a duplicate packed ABI.\n')
    return '\n'.join(output)


# Source functions and descriptor/frame macros must remain distinct even across module prefixes.
def collisions(limbo, work, compiler, flags, includes, runner, env):
    source = work/'collision.m'
    source.write_text('''Probe: module { Item: adt { x: int; size: fn(i: self Item): int; map: fn(i: self Item): int; }; };
A: module { f: fn(): int; };
F_A: module { f_size: fn(): int; f_map: fn(): int; };
''')
    header = subprocess.check_output([str(limbo), '-H', str(source)], env=env).decode()
    for symbol in ('Probe_Item_size', 'Probe_Item_map', 'F_A_f_size', 'F_A_f_map'):
        if '#define limbo_native64_descriptor_'+symbol not in header or '#define '+symbol+' ' in header:
            raise RuntimeError('Descriptor shadowed callable: '+symbol)
    (work/'collision.h').write_text(header)
    unit = '''#include "lib9.h"
#include "isa.h"
#include "interp.h"
#define LIMBO_NATIVE64_IMPLEMENTATIONS // Exercise every generated callable in this small real interface.
#include "collision.h"
#include <stdio.h>
void Probe_Item_size(void *p){ F_Probe_Item_size *f=p; *f->ret=f->i.x+1; }
void Probe_Item_map(void *p){ F_Probe_Item_map *f=p; *f->ret=f->i.x+2; }
void A_f(void *p){ F_A_f *f=p; *f->ret=3; }
void F_A_f_size(void *p){ F_F_A_f_size *f=p; *f->ret=4; }
void F_A_f_map(void *p){ F_F_A_f_map *f=p; *f->ret=5; }
int main(void){
 WORD r=0; F_Probe_Item_size a={0}; F_Probe_Item_map b={0}; F_A_f c={0};
 F_F_A_f_size d={0}; F_F_A_f_map e={0};
 if(!limbo_native64_Probe_layout() || limbo_native64_A_specs[0].frame_size!=64) return 10;
 a.ret=&r;a.i.x=10;limbo_native64_Probe_Item_size_entry.function(&a);if(r!=11)return 10;
 b.ret=&r;b.i.x=10;limbo_native64_Probe_Item_map_entry.function(&b);if(r!=12)return 10;
 c.ret=&r;limbo_native64_A_f_entry.function(&c);if(r!=3)return 10;
 d.ret=&r;limbo_native64_F_A_f_size_entry.function(&d);if(r!=4)return 10;
 e.ret=&r;limbo_native64_F_A_f_map_entry.function(&e);if(r!=5)return 10;
 puts("PASS callable/descriptor collisions: type size/map, cross-module frame size/map, actual entry/table bindings");return 0;
}
'''
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    path = work/'collision.c'
    path.write_text(support['portfixture'](unit, '#include "lib9.h"'))
    binary = work/'collision'
    subprocess.run(compiler+flags+['-DDIS_WORD32=1']+includes+[str(path), '-o', str(binary)], env=env, check=True)
    subprocess.run(runner+[str(binary)], env=env, check=True, timeout=30)
    for text in ('Probe: module { A: adt { x: int; }; A_size: con 1; };',
                 'A_B: module { C: adt { x: int; }; }; A: module { B_C: adt { x: int; }; };'):
        source.write_text(text)
        result = subprocess.run([str(limbo), '-H', str(source)], env=env, capture_output=True, text=True)
        if result.returncode == 0 or result.stdout or 'C name collision' not in result.stderr:
            raise RuntimeError('Real C namespace collision was not rejected before output')


# Link complete production geom.c, then require byte-accurate semantic controls before unsafe use.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    with tempfile.TemporaryDirectory(prefix='native64-draw-geometry-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        command = [str(args.limbo.resolve()), '-H', '-M', 'Draw', '-I'+str(ROOT/'module'), str(ROOT/'module/draw-native64.m')]
        header = subprocess.check_output(command, env=env).decode()
        if header != subprocess.check_output(command, env=env).decode():
            raise RuntimeError('Draw header nondeterminism')
        source = (ROOT/'libinterp/geom.c').read_text()
        names = re.findall(r'^void\n((?:Point|Rect)_\w+)\(', source, re.M)
        if len(names) != 19:
            raise RuntimeError('Owning geometry export set changed')
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        collisions(args.limbo.resolve(), work, compiler, flags, includes, runner, env)
        support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
        fixture = (ROOT/'utils/tests/native64-geometry.c').read_text().replace('GEOMETRY_ORACLES', oracle(header, names))
        fixture = support['portfixture'](fixture, '#include "lib9.h"')
        test = work/'test.c'; test.write_text(fixture)
        consumer = work/'geom.c'
        (work/'consumer-draw.h').write_text(header)
        consumer.write_text(support['portfixture'](source, '#include "lib9.h"'))
        for selection in ([], ['-DDIS_WORD32=0']):
            rejected = subprocess.run(compiler+flags+selection+includes+
                ['-DDIS_NATIVE64_GEOMETRY_HEADER="consumer-draw.h"', '-fsyntax-only', str(consumer)],
                env=env, capture_output=True, text=True)
            if rejected.returncode == 0 or 'native64 geometry declarations require DIS_WORD32=1' not in rejected.stderr:
                raise RuntimeError('Geometry consumer accepted an incompatible model')
        print('PASS explicit geometry model guards: missing/zero WORD32', flush=True)
        for mode in ('WORD32', 'WORD32-file', 'input', 'result'):
            selected, body = header, source
            if mode == 'input':
                old = 'struct F_Draw_Point_add {'
                if selected.count(old) != 1:
                    raise RuntimeError('Input control frame drift')
                # Both views use a 256-byte backed frame. The old input offset reaches
                # a semantic mismatch without reading beyond allocated storage.
                start = selected.index(old)
                end = selected.index('\n};', start)+3
                frame = selected[start:end]
                field = 'Draw_Point p; // Offset 64, 8 bytes.'
                if frame.count(field) != 1:
                    raise RuntimeError('Input control argument drift')
                selected = selected[:start]+frame.replace(field, 'uchar changed_input[8]; // Deliberate native input-offset control.\n\tDraw_Point p;', 1)+selected[end:]
                if selected == header:
                    raise RuntimeError('Input control field drift')
            if mode == 'result':
                function = support['function'](body, 'void\nPoint_add(')
                if function.count('sizeof(result)') != 1:
                    raise RuntimeError('Result control drift')
                body = body.replace(function, function.replace('sizeof(result)', 'sizeof(result)-4'), 1)
            (work/'draw-native64.h').write_text(header)
            # Consumer-only header mutation keeps the oracle's frame declaration independent.
            (work/'consumer-draw.h').write_text(selected)
            consumer.write_text(support['portfixture'](body, '#include "lib9.h"'))
            model = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode == 'WORD32-file' else [])
            obj = work/(mode+'.o'); binary = work/mode
            subprocess.run(compiler+flags+model+includes+['-DDIS_NATIVE64_GEOMETRY_HEADER="consumer-draw.h"', '-c', str(consumer), '-o', str(obj)], env=env, check=True)
            subprocess.run(compiler+flags+model+includes+[str(test), str(obj), '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=60)
            if mode in ('input', 'result'):
                if result.returncode != 10 or 'Native64 geometry mismatch' not in result.stderr:
                    raise RuntimeError('Geometry control missed semantic predicate: '+mode+' '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS native64 geometry control:', mode, arch, flush=True)
            else:
                if result.returncode:
                    raise RuntimeError(result.stdout+result.stderr)
                print(mode+': '+result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
