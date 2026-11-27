#!/usr/bin/env python3
"""Exercise real glyph-cache/text/width bodies and five Frame64 consumers with independent packets."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NAMES = ('text', 'textop', 'textbg', 'textbgop', 'width')


# Derive native packet fields from character widths, never C packing or measured results.
def oracle(chars, p, bgp, bg, op, fault):
    output, point, background, requests = bytearray(), p[:], bgp[:], 0
    for start in range(0, len(chars), 100):
        batch = chars[start:start+100]
        width = sum(r%7+1 for r in batch)
        baseline, end, bend = point[1]+12, point[0]+width, background[0]+width
        if not -2**31 <= baseline < 2**31 or not -2**31 <= end < 2**31 or (bg and not -2**31 <= bend < 2**31): break
        requests += 1
        if requests == fault: break
        output += bytes((ord('O'), op)) if op != 11 else b''
        output += b'x' if bg else b's'
        fields = [7, 9, 13, point[0], baseline, -300, -400, 500, 600, -17, 19]
        output += b''.join(struct.pack('<I', v&0xffffffff) for v in fields)+struct.pack('<H', len(batch))
        if bg: output += struct.pack('<I', 11)+b''.join(struct.pack('<I', v&0xffffffff) for v in background)
        output += b''.join(struct.pack('<H', 17*r&1023) for r in batch)
        point[0] = end
        if bg: background[0] = bend
    return bytes(output), point, requests


# Retain long multi-batch ASCII/Rune inputs and wide-coordinate partial-output cases.
def declarations():
    rows = []
    for wide in (0, 1):
        alphabet = [65, 66, 67, 68] if not wide else [65, 937, 128512, 66]
        for count in (0, 1, 63, 64, 65, 99, 100, 101, 201):
            chars = [alphabet[i%4] for i in range(count)]
            for scenario in range(5):
                p = [[10, 20], [2**31-10, 20], [-2**31, -2**31], [10, 2**31-8], [10, 20]][scenario]
                bgp = [2**31-10 if scenario == 4 else -31, 37]
                for variant in range(5):
                    for mode in (0, 1):
                        for fault in (0, 1, 2, 3):
                            op = 5 if variant in (1, 3) else 11
                            if variant == 4:
                                trace, point, requests = b'', [sum(r%7+1 for r in chars), 0], 0
                            else: trace, point, requests = oracle(chars, p, bgp, variant in (2, 3), op, fault)
                            num = lambda v: '(-2147483647-1)' if v == -2**31 else str(v)
                            pt = lambda v: '{'+','.join(map(num,v))+'}'
                            vals = [str(wide), str(count), str(variant), str(mode), str(fault), pt(p), pt(bgp), pt(point), str(requests), str(len(trace)), '{'+(','.join(map(str, trace)) or '0')+'}']
                            rows.append('{'+','.join(vals)+'}')
    return 'static TextCase cases[] = {\n'+',\n'.join(rows)+'\n}; // Independent character/coordinate/protocol expectations.\n'


# Generated frames preserve their arguments and route to actual consumer bodies.
def dispatch():
    rows = []
    for i, name in enumerate(NAMES):
        prefix = 'Font_' if name == 'width' else 'Image_'
        typ = 'F_Draw_'+prefix+name
        fields = ['f.ret = (void*)result;', 'f.str = input;']
        if name == 'width': fields += ['f.f = &fontobject;']
        else:
            fields += ['f.dst=objects; f.src=objects+1; f.font=&fontobject;', 'memmove(&f.p, &c->p, sizeof(f.p));', 'f.sp.x=-17; f.sp.y=19;']
            if 'bg' in name: fields += ['f.bg=objects+2;', 'memmove(&f.bgp, &c->bgp, sizeof(f.bgp));']
            if name.endswith('op'): fields += ['f.op=5;']
        rows.append('case '+str(i)+': { '+typ+' f, copy; memset(&f, 0xa5, sizeof(f)); '+' '.join(fields)+' memmove(&copy, &f, sizeof(f)); Draw_'+prefix+name+'(&f); check(memcmp(&copy,&f,sizeof(f))==0); break; }')
    return '\n'.join(rows)


# Preserve actual cache algorithms; only cache-miss/native I/O/lookup/VM-error services are bounded transports.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    draw = (ROOT/'libinterp/draw.c').read_text()
    begin = draw.index('typedef struct DrawTextView')
    end = draw.index('/* Return the requested point', begin)
    consumer = draw[begin:end]
    consumer += '\n\n'+'\n\n'.join(support['function'](draw, sig) for sig in ('static void\ndrawtext(', 'static void\ndrawtextbg(', 'void\nImage_text(', 'void\nImage_textop(', 'void\nImage_textbg(', 'void\nImage_textbgop(', 'void\nFont_width('))
    consumer = consumer.replace('F_Image_', 'F_Draw_Image_').replace('F_Font_width','F_Draw_Font_width').replace('\nImage_', '\nDraw_Image_').replace('\nFont_width(', '\nDraw_Font_width(')
    native = '\n\n'.join(re.sub(r'^#include[^\n]*\n', '', (ROOT/p).read_text(), flags=re.M) for p in ('libdraw/string.c', 'libdraw/stringbg.c', 'libdraw/stringwidth.c', 'libdraw/arith.c', 'lib9/rune.c'))
    cache = support['function']((ROOT/'libdraw/font.c').read_text(), 'int\ncachechars(').replace('int\ncachechars(', 'int\nactualcachechars(', 1)
    cache += '\n\n'+support['function']((ROOT/'libdraw/font.c').read_text(), 'void\nagefont(').replace('void\nagefont(', 'void\nactualagefont(', 1)
    fixture = (ROOT/'utils/tests/native64-text.c').read_text()
    helper = (ROOT/'libdraw/draw-packet.h').read_text()
    env = dict(os.environ); arch = env.get('NATIVE64_ARCH', 'amd64')
    command = shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))+['-ffunction-sections','-fdata-sections']
    runner = shlex.split(env.get('NATIVE64_RUNNER',''))
    with tempfile.TemporaryDirectory(prefix='native64-text-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work=Path(directory); env['TMPDIR']=str(work)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()),'-H','-M','Draw','-I'+str(ROOT/'module'),str(ROOT/'module/draw-native64.m')], env=env))
        includes=['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        for mode in ('WORD32','WORD32-file','result','background','progress','geometry','width','view','packet','utf-cache'):
            body, vm = native, consumer
            if mode=='result': vm=vm.replace('pt = IPOINT(f->p);','pt = ZP;',1)
            if mode=='background': vm=vm.replace(' || d->display!=bg->display','',1)
            if mode=='progress': body=body.replace('n == 0 && ++retries > 10','0',1)
            if mode=='geometry': body=body.replace('baseline > 2147483647','0',1)
            if mode=='width': body=body.replace('wid > 2147483647-twid','0',1)
            if mode=='view': vm=vm.replace('if(capacity < view->count)','if(0)',1)
            if mode=='packet': body=body.replace('BPLONG(b+9, f->cacheimage->id);','BPLONG(b+9, f->cacheimage->id-1);',1)
            if mode=='utf-cache': vm=vm.replace('result = text.ascii != nil ?', 'string2c(f->str);\n\t\tresult = text.ascii != nil ?',1)
            text=fixture.replace('PACKET_HELPER',helper).replace('CACHE_IMPLEMENTATION',cache).replace('NATIVE_IMPLEMENTATION',body).replace('VM_IMPLEMENTATION',vm).replace('FRAME_DISPATCH',dispatch()).replace('TEXT_ORACLES',declarations())
            source=work/(mode+'.c');source.write_text(support['portfixture'](text,'#include "lib9.h"'))
            binary=work/mode
            model=['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode.endswith('file') else [])
            subprocess.run(command+model+includes+[str(source),'-Wl,--gc-sections','-o',str(binary)],env=env,check=True)
            result=subprocess.run(runner+[str(binary)],env=env,capture_output=True,text=True,timeout=90)
            if mode.startswith('WORD'):
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(mode+': '+result.stdout,end='',flush=True)
            elif result.returncode!=10 or 'text semantic mismatch' not in result.stderr: raise RuntimeError('Text control missed predicate '+mode+' '+result.stdout+result.stderr)
            else: print('PASS text semantic control:',mode,arch,flush=True)


if __name__=='__main__':
    main()
