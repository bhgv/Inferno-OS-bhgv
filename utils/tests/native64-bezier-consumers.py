#!/usr/bin/env python3
"""Check four actual generated Frame64 cubic consumers at their native C dispatch boundary."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Preserve actual consumer bodies and fresh public frames; native lookup/curve/lock services remain explicit.
def unit(control):
    helper = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    draw = (ROOT/'libinterp/draw.c').read_text()
    names = ('drawbezier','Image_bezier','Image_bezierop','drawfillbezier','Image_fillbezier','Image_fillbezierop')
    source = '\n\n'.join(helper['function'](draw, ('static void\n' if n.startswith('draw') else 'void\n')+n+'(') for n in names)
    source = source.replace('F_Image_', 'F_Draw_Image_').replace('\nImage_', '\nDraw_Image_')
    if control == 'capture':
        source = source.replace('bezierop(d, a, b, c, end,', 'bezierop(d, IPOINT(f->a), b, c, end,', 1)
    elif control == 'source':
        source = source.replace('source = f->src;', 'source = f->dst;', 1)
    elif control == 'flush':
        source = source.replace('checkflush(destination);', 'checkflush(f->dst);', 1)
    elif control == 'operator':
        source = source.replace(' || (unsigned int)op >= Ncomp', '')
    elif control == 'radius':
        source = source.replace('radius < 0', '0', 1)
    elif control == 'foreign':
        source = source.replace('d->display != s->display', '0', 1)
    elif control == 'unlock':
        source = source.replace('unlockdisplay(d->display);', '/* omitted acquired exclusion release */;', 1)
    fixture = (ROOT/'utils/tests/native64-bezier-consumers.c').read_text()
    return helper['portfixture'](fixture.replace('IMPLEMENTATION', source), '#include "lib9.h"')


# Require exact semantic controls, never accepting compiler/resource/sanitizer/signal/timeout failures.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--control', choices=('capture','source','flush','operator','radius','foreign','unlock'))
    args = parser.parse_args()
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    guard = {'amd64':'!defined(__x86_64__)', 'arm64':'!defined(__aarch64__)', 'riscv64':'!defined(__riscv) || __riscv_xlen != 64'}[arch]
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='native64-bezier-consumers-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        (work/'draw-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()),'-H','-M','Draw','-I'+str(ROOT/'module'),str(ROOT/'module/draw-native64.m')], env=env))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        source = work/'test.c'
        source.write_text('#if '+guard+'\n#error native compiler mismatch\n#endif\n'+unit(args.control))
        for mode in (('control',) if args.control else ('WORD32','WORD32-file')):
            binary = work/mode
            model = ['-DDIS_WORD32=1']+(['-DDIS64_PILOT_LOADER=1'] if mode.endswith('file') else [])
            subprocess.run(compiler+flags+model+includes+[str(source),'-o',str(binary)],env=env,check=True)
            result = subprocess.run(runner+[str(binary)],env=env,capture_output=True,text=True,timeout=60)
            if args.control:
                stage = {'capture':'curve arguments','source':'source identity','flush':'flush identity',
                         'operator':'dispatch admission','radius':'dispatch admission','foreign':'dispatch admission','unlock':'balanced exclusion'}[args.control]
                if result.returncode != 10 or result.stderr.strip() != 'Bezier consumer mismatch: '+stage:
                    raise RuntimeError('control missed exact semantic stage: '+result.stdout+result.stderr)
                print('PASS Bezier consumer semantic control:',args.control,arch,flush=True)
            elif result.returncode:
                raise RuntimeError(result.stdout+result.stderr)
            else:
                print(mode+': '+result.stdout,end='',flush=True)


if __name__ == '__main__':
    main()
