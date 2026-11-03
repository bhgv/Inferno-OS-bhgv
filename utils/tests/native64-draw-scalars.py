#!/usr/bin/env python3
"""Link the complete owning Draw consumer and real scalar libraries against independent oracles."""
import argparse
import math
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


# Compute the documented 256-colour palette independently of the C implementation.
def palette(c):
    r, v = c//64, (c//16)%4
    j = (c-v+r)%16
    g, b = j//4, j%4
    den = max(r, g, b)
    if den == 0:
        return [17*v]*3
    return [x*17*(4*den+v)//den for x in (r, g, b)]


# Reconstruct the documented arctangent table mathematically, without reading its C bytes.
def direction(x, y):
    if x == 0:
        return [0, 1024 if y >= 0 else -1024]
    a, b = abs(x), abs(y)
    swap = b > a
    t = 1000*min(a, b)//max(a, b)
    index, remainder = divmod(t, 10)
    # Calculate each table endpoint from its documented trigonometric definition.
    def sample(i):
        q = i/100.0
        return [int(1024*q/math.sqrt(1+q*q)+0.5), int(1024/math.sqrt(1+q*q)+0.5)]
    low, high = sample(index), sample(index+1)
    pair = [low[i]+int((high[i]-low[i])*remainder/10) for i in (0, 1)]
    sine, cosine = pair
    if swap:
        sine, cosine = cosine, sine
    return [cosine*(-1 if x < 0 else 1), sine*(-1 if y < 0 else 1)]


# Use independent integers for palette distances, packed colour, channel and rectangle arithmetic.
def cases(name, row, rng):
    edges = [-2147483648, 2147483647, -1, 0, 1, -17, 255, 256]
    x = edges[row%len(edges)] if row < 64 else rng.randint(-2147483648, 2147483647)
    y = edges[(row//8)%len(edges)] if row < 64 else rng.randint(-2147483648, 2147483647)
    if name == 'Font_bbox':
        return [0, 0], [0]*4
    if name == 'Chans_eq':
        return [x, x if row%3 == 0 else y], [int(x == (x if row%3 == 0 else y))]
    if name == 'Chans_depth':
        c = [0, 0x38, 0x082818, 0x08182848, 0x31, 0x32, 0x34, 0x33, 0x39, 0x78][row%10] if row < 64 else rng.getrandbits(32)
        n, value, valid = 0, c, True
        while value:
            kind, width = (value>>4)&15, value&15
            valid &= kind < 7 and 1 <= width <= 8
            n += width
            value >>= 8
        valid &= n != 0 and (n%8 == 0 if n > 8 else 8%n == 0)
        return [c], [n if valid else 0]
    if name in ('Display_cmap2rgb', 'Display_cmap2rgba'):
        c = row%256
        rgb = palette(c)
        return [0, 0, c], rgb if name.endswith('2rgb') else [(rgb[0]<<24)|(rgb[1]<<16)|(rgb[2]<<8)|255]
    if name == 'Display_rgb2cmap':
        rgb = [x, y, edges[(row+3)%8]] if row < 64 else [rng.randint(-2147483648, 2147483647) for _ in range(3)]
        best = min(range(256), key=lambda c: sum((a-b)**2 for a, b in zip(palette(c), rgb)))
        return [0, 0]+rgb, [best]
    if name == 'setalpha':
        color = rng.getrandbits(32)
        alpha = row%256
        result = alpha
        for shift in (8, 16, 24):
            result |= (((color>>shift)&255)*alpha//255)<<shift
        return [color, alpha], [result]
    if name == 'icossin':
        deg = x
        angle = deg%360
        q, r = divmod(angle, 90)
        sine = int(1024*math.sin(math.pi*r/180)+0.5)
        cosine = int(1024*math.sin(math.pi*(90-r)/180)+0.5)
        result = [(cosine, sine), (-sine, cosine), (-cosine, -sine), (sine, -cosine)][q]
        return [deg], list(result)
    if name == 'icossin2':
        return [x, y], direction(x, y)
    if name == 'bytesperline':
        left, right = min(x, y), max(x, y)
        depth = [1, 2, 4, 8, 16, 24, 32][row%7]
        return [left, -1, right, 1, depth], [-((-right*depth)//8)-(left*depth//8)]
    raise RuntimeError('Unknown scalar oracle: '+name)


# Populate actual generated fields; do not duplicate the C compiler's frame padding.
def oracle(header):
    rng = random.Random(96064)
    output, specs = [], []
    names = ('Font_bbox', 'Chans_eq', 'Chans_depth', 'Display_cmap2rgb', 'Display_cmap2rgba',
             'Display_rgb2cmap', 'setalpha', 'icossin', 'icossin2', 'bytesperline')
    for name in names:
        frame = 'F_Draw_'+name
        body = re.search(r'struct '+frame+r' \{(.*?)\n\};', header, re.S).group(1)
        fields = [(kind, field) for kind, field, offset in re.findall(
            r'\n\s*(WORD|Draw_\w+\*?|Draw_Rect) (\w+); // Offset (\d+),', body) if int(offset) >= 64]
        rows, units = [], []
        for row in range(256):
            inputs, results = cases(name, row, rng)
            encode = lambda values: ','.join('0x%08xU' % (v&0xffffffff) for v in values)
            rows.append('{{'+encode(inputs)+'},{'+encode(results)+'},0}')
            if name == 'bytesperline':
                left, _, right, _, depth = inputs
                units.append('{'+encode([-((-right*depth)//bits)-(left*depth//bits) for bits in (32, 64)])+'}')
        output.append('static GeometryCase cases_'+name+'[] = {\n'+',\n'.join(rows)+'\n}; // Independent scalar oracle.\n')
        if name == 'bytesperline':
            output.append('static const u32int nativewordunits[][2] = {\n'+',\n'.join(units)+'\n}; // Independent native32/native64 word-stride oracle.\n')
        offsets = ','.join('offsetof('+frame+','+field+')' for _, field in fields)
        widths = ','.join('sizeof((('+frame+'*)0)->'+field+')/4' for _, field in fields)
        specs.append('{"'+name+'",Draw_'+name+',sizeof('+frame+'),offsetof('+frame+',ret),sizeof(*(('+frame+'*)0)->ret),'+str(len(fields))+',{'+offsets+'},{'+widths+'},cases_'+name+',256}')
    output.append('static GeometrySpec specifications[] = {\n'+',\n'.join(specs)+'\n}; // Actual generated scalar entries.\n')
    return '\n'.join(output)


# Compile every owning Draw body; section collection drops only uninvoked device/resource paths.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    flags += ['-ffunction-sections', '-fdata-sections']
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    with tempfile.TemporaryDirectory(prefix='native64-draw-scalars-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        header = subprocess.check_output([str(args.limbo.resolve()), '-H', '-M', 'Draw', '-I'+str(ROOT/'module'), str(ROOT/'module/draw-native64.m')], env=env).decode()
        (work/'draw-native64.h').write_text(header)
        fixture = (ROOT/'utils/tests/native64-geometry.c').read_text()
        fixture = fixture.replace('int offset[2]', 'int offset[4]').replace('int words[2]', 'int words[4]').replace('spec->arguments <= 2', 'spec->arguments <= 4')
        fixture = fixture.replace('GEOMETRY_ORACLES', oracle(header)).replace('Draw geometry', 'Draw scalars')
        fixture = fixture.replace('check(expecting && message == exZdiv);',
            'check(expecting == 2 ? strcmp(message, "experimental native64 Draw registration is not supported") == 0 : expecting && message == exZdiv);')
        fixture = fixture.replace('/* Full Draw scalars uses',
            '/* Exercise the actual early refusal without activating devices or legacy tables. */\n'
            'static void\ndrawinitialize(void *unused)\n{\n\tUSED(unused);\n\tdrawmodinit();\n}\n\n\n/* Full Draw geometry uses')
        fixture = fixture.replace('unsigned int i;\n',
            'unsigned int i;\n\texpecting = 2;\n\tcheck(!invoke(drawinitialize, nil));\n')
        fixture = fixture.replace('#include "draw-native64.h"', '#include "draw-native64.h"\n#include "draw.h"\nvoid drawmodinit(void); // Actual complete consumer entry, expected to refuse header-only registration.')
        fixture = fixture.replace('unsigned int i;\n',
            'unsigned int i;\n\tRectangle rectangle;\n'
            '\tfor(i=0; i<nelem(cases_bytesperline); i++){\n'
            '\t\tmemmove(&rectangle, cases_bytesperline[i].input, sizeof(rectangle));\n'
            '\t\tcheck((u32int)wordsperline(rectangle, cases_bytesperline[i].input[4]) == nativewordunits[i][sizeof(ulong) == 8]);\n'
            '\t}\n')
        (work/'test.c').write_text(support['portfixture'](fixture, '#include "lib9.h"'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        sources = ['libinterp/draw.c', 'libdraw/rgb.c', 'libdraw/arith.c', 'libdraw/chan.c',
                   'libdraw/bytesperline.c', 'libmemdraw/icossin.c', 'libmemdraw/icossin2.c']
        for mode in ('WORD32', 'WORD32-file', 'result'):
            model = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode == 'WORD32-file' else [])
            objects = []
            for index, path in enumerate(sources):
                body = (ROOT/path).read_text()
                if mode == 'result' and index == 0:
                    function = support['function'](body, 'void\nFont_bbox(')
                    if function.count('sizeof(result)') != 2:
                        raise RuntimeError('Scalar result control drift')
                    changed = function.replace('memmove(f->ret, &result, sizeof(result));', 'memmove(f->ret, &result, sizeof(result)-4);')
                    body = body.replace(function, changed, 1)
                boundary = '#include <lib9.h>' if index == 0 else re.search(r'^#include\s+"lib9.h"', body, re.M).group(0)
                source = work/(str(index)+'.c')
                source.write_text(support['portfixture'](body, boundary))
                obj = work/(str(index)+'.o'); objects.append(str(obj))
                if index == 0 and mode == 'WORD32':
                    for refused in ([], ['-DDIS_WORD32=0']):
                        rejected = subprocess.run(compiler+flags+refused+includes+
                            ['-DDIS_NATIVE64_DRAW_HEADER="draw-native64.h"', '-fsyntax-only', str(source)],
                            env=env, capture_output=True, text=True)
                        if rejected.returncode == 0 or 'native64 Draw declarations require DIS_WORD32=1' not in rejected.stderr:
                            raise RuntimeError('Draw model selection did not refuse')
                    print('PASS complete Draw missing/zero WORD32 guards', flush=True)
                subprocess.run(compiler+flags+model+includes+['-DDIS_NATIVE64_DRAW_HEADER="draw-native64.h"', '-c', str(source), '-o', str(obj)], env=env, check=True)
            binary = work/mode
            subprocess.run(compiler+flags+model+includes+[str(work/'test.c')]+objects+['-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=60)
            if mode == 'result':
                if result.returncode != 10 or 'Native64 geometry mismatch' not in result.stderr:
                    raise RuntimeError('Scalar control missed semantic failure: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS full Draw scalar result control:', arch, flush=True)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(mode+': '+result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
