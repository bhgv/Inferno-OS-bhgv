#!/usr/bin/env python3
"""Compile the complete Draw owner and exercise its real native64 initialization/rollback."""
import argparse
import os
from pathlib import Path
import re
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    """Keep real module/Type construction, with callback identities rather than simulated GUI calls."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    extract = support['function']
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    with tempfile.TemporaryDirectory(prefix='native64-draw-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = subprocess.check_output([str(args.limbo.resolve()), '-H', '-M', 'Draw', '-I'+str(ROOT/'module'),
                                          str(ROOT/'module/draw-native64.m')], env=env)
        (work/'draw-native64.h').write_bytes(header)
        selected = ['-DDIS_WORD32=1', '-DDIS_NATIVE64_DRAW_REGISTER=1', '-DDIS_NATIVE64_DRAW_HEADER="draw-native64.h"']
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in
                    ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'emu/Android', 'include', 'libinterp')]
        # Compile both owners: geometry methods are implemented outside draw.c.
        for name in ('draw', 'geom'):
            subprocess.run(compiler+flags+selected+includes+['-c', str(ROOT/('libinterp/'+name+'.c')),
                                                            '-o', str(work/(name+'.o'))], env=env, check=True)
        for options, marker in ((selected[1:], 'require DIS_WORD32=1'),
                                (selected[:2], 'requires an explicit generated header')):
            result = subprocess.run(compiler+flags+options+includes+['-c', str(ROOT/'libinterp/draw.c'),
                                                                    '-o', str(work/'invalid.o')],
                                    env=env, capture_output=True, text=True)
            if result.returncode == 0 or marker not in result.stderr:
                raise RuntimeError('Draw declaration guard failed: '+result.stderr)
        body = support['constructor']()
        body = body.replace('static int live,', 'static void drawobserve(void);\nstatic int live,', 1)
        body = body.replace('\tcount = 0;\n', '\tdrawobserve();\n\tcount = 0;\n', 1)
        draw = (ROOT/'libinterp/draw.c').read_text()
        initializer = extract(draw, 'static int\ndrawmaketypes(')+'\n'+extract(draw, 'void\ndrawmodinit(')
        callbacks = re.findall(r'^void (Draw_[A-Za-z0-9_]+)\(void\*\);', header.decode(), re.M)
        if len(callbacks) != 89:
            raise RuntimeError('Unexpected Draw callback set')
        fixture = (ROOT/'utils/tests/native64-draw-register.c').read_text()
        geometry = {'Draw_'+name for name in re.findall(r'^void\n([A-Za-z0-9_]+)\(', (ROOT/'libinterp/geom.c').read_text(), re.M)}
        fixture = fixture.replace('CALLBACKS', '\n'.join('void '+name+'(void *value) { USED(value); abort(); }' for name in callbacks if name not in geometry))
        fixture = fixture.replace('WRAPPERS', draw[draw.index('struct DRef\n{'):draw.index('Cache*\tsfcache')])
        fixture = fixture.replace('GLOBALS', draw[draw.index('uchar fontmap[]'):draw.index('Draw_Image*\tallocdrawimage')])
        fixture = fixture.replace('INITIALIZER', initializer)
        source = work/'fixture.c'; source.write_text(body+'\n'+fixture)
        target = work/'fixture'
        subprocess.run(compiler+flags+selected+includes+[str(source), str(work/'geom.o'), '-o', str(target)], env=env, check=True)
        subprocess.run(runner+[str(target)], env=env, check=True, timeout=60)


if __name__ == '__main__':
    main()
