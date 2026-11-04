#!/usr/bin/env python3
"""Run real built-in font decode/upload/subfont/synthetic ownership at native target widths."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep the actual immutable data, constructors, upload, weak cache and destructors; inject only native services.
def unit(control):
    helpers = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    fn = helpers['function']
    data = (ROOT/'libdraw/defont.c').read_text()
    original = data[data.index('uchar\ndefontdata[]'):data.index('int\tsizeofdefont')]
    original = original.replace('uchar\ndefontdata[]', 'static const uchar original[]', 1)
    parts = [fn((ROOT/'libdraw/drawinfo.h').read_text(), 'static int\ndrawinfointeger('),
             (ROOT/'libdraw/pixelspan.h').read_text()]
    for name in ('getdefont', 'loadimage', 'subfont', 'freesubfont', 'subfontcache', 'mkfont', 'buildfont'):
        body = re.sub(r'^#include[^\n]*\n', '', (ROOT/'libdraw'/(name+'.c')).read_text(), flags=re.M)
        if name == 'mkfont' and control == 'range':
            body = body.replace('min > Runemax || (Rune)(subfont->n-1) > Runemax-min', '0', 1)
        if name == 'mkfont' and control == 'transfer':
            body = body.replace('font->subf[0].f = subfont;', 'subfont->ref++; font->subf[0].f = subfont;', 1)
        if name == 'getdefont':
            if control == 'immutable':
                body = body.replace('if(font != nil)\n\t\treturn font;', 'if(font != nil){ defontdata[0] ^= 1; return font; }', 1)
            if control == 'extent':
                body = body.replace('if(remaining != 6*(count+1))', 'if(0 && remaining != 6*(count+1))', 1)
            if control == 'pixels':
                body = body.replace('if(loadimage(image, rectangle, pixels, bytes) != bytes)\n\t\tgoto Refuse;', 'loadimage(image, rectangle, pixels, bytes);', 1)
            if control == 'rollback':
                body = body.replace('free(info);', 'USED(info);', 1)
        parts.append(body)
    decoded = fn(data, 'void\n_unpackinfo(')
    if control == 'bearing':
        decoded = decoded.replace('fc->left = p[4] < 128 ? p[4] : (int)p[4]-256;', 'fc->left = 0;', 1)
    parts.append(decoded)
    parts.append(re.sub(r'^#include[^\n]*\n', '', (ROOT/'libdraw/chan.c').read_text(), flags=re.M))
    arith = (ROOT/'libdraw/arith.c').read_text()
    parts.append(fn(arith, 'Rectangle\nRect('))
    parts.append(fn((ROOT/'libdraw/rectclip.c').read_text(), 'int\nrectclip('))
    parts.append(arith[arith.index('ulong\ndrawld2chan[]'):arith.index('/* Assemble premultiplied')])
    text = (ROOT/'libdraw/tests/default-font.c').read_text().replace('BUILTIN_BYTES', original)
    text = text.replace('IMPLEMENTATION', '\n\n'.join(parts))
    return helpers['portfixture'](text, '#include "draw.h"')


# A diagnostic omission must reach its exact predicate, never a compile, resource or sanitizer failure.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('immutable', 'extent', 'pixels', 'rollback', 'range', 'transfer', 'bearing'))
    parser.add_argument('--char-model', choices=('native', 'signed', 'unsigned'), default='native')
    args = parser.parse_args()
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    source = unit(args.control)
    guard = {'amd64': '!defined(__x86_64__)', 'arm64': '!defined(__aarch64__)',
             'riscv64': '!defined(__riscv) || __riscv_xlen != 64'}[arch]
    source = '#if '+guard+'\n#error native target compiler mismatch\n#endif\n'+source
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    if args.char_model != 'native':
        flags += ['-f'+args.char_model+'-char']
    flags += ['-ffunction-sections', '-fdata-sections']
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'include', 'emu/port')]
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='default-font-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        path = Path(directory)/'test.c'
        path.write_text(source)
        binary = Path(directory)/'test'
        subprocess.run(compiler+flags+includes+[str(path), '-Wl,--gc-sections', '-o', str(binary)], env=env, check=True)
        result = subprocess.run(runner+[str(binary)], capture_output=True, text=True, env=env, timeout=60)
        if args.control:
            stage = {'immutable': 'blob identity', 'extent': 'preflight', 'pixels': 'load refusal',
                     'rollback': 'owners', 'range': 'synthetic admission', 'transfer': 'transfer', 'bearing': 'signed metric'}[args.control]
            if result.returncode != 10 or ('Default font semantic mismatch: '+stage+' ') not in result.stderr:
                raise RuntimeError('default-font control missed exact stage: '+result.stdout+result.stderr)
            print('PASS default-font semantic omission:', args.control, arch)
        else:
            if result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
