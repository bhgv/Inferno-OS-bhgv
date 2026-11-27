#!/usr/bin/env python3
"""Compile complete fixed Sys owners and check atomic descriptors/module registration."""
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
    """Use real constructor/init/UTF bodies with bounded allocation and error transports."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    extract = support['function']
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(os.environ.get('NATIVE64_RUNNER', ''))
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    with tempfile.TemporaryDirectory(prefix='native64-sys-fixed-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory); env = dict(os.environ, TMPDIR=str(work))
        header = subprocess.check_output([str(args.limbo.resolve()), '-H', str(ROOT/'module/sys.m')], env=env)
        (work/'sys-native64.h').write_bytes(header)
        selected = ['-DDIS_WORD32=1', '-DDIS_NATIVE64_GUI_SYS_FIXED=1']
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in
                    ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'emu/Android', 'include', 'libinterp')]
        for name in ('libinterp/runt.c', 'emu/port/inferno.c'):
            subprocess.run(compiler+flags+selected+includes+['-c', str(ROOT/name), '-o', str(work/(Path(name).stem+'.o'))], env=env, check=True)
        for options, marker in ((selected[1:], 'requires DIS_WORD32=1'),
                                (selected+['-DDIS_NATIVE64_GUI_SYS_FIXED=0'], 'owns its generated declarations'),
                                (selected+['-DDIS_NATIVE64_SYS_HEADER="sys-native64.h"'], 'owns its generated declarations'),
                                (selected+['-DDIS_NATIVE64_SCALAR_BOOT=1', '-DDIS64_PILOT_LOADER=1'], 'owns its generated declarations')):
            result = subprocess.run(compiler+flags+options+includes+['-fsyntax-only', str(ROOT/'libinterp/runt.c')], env=env, capture_output=True, text=True)
            if result.returncode == 0 or marker not in result.stderr:
                raise RuntimeError('Fixed Sys selection guard failed: '+result.stderr)
        runt = (ROOT/'libinterp/runt.c').read_text(); port = (ROOT/'emu/port/inferno.c').read_text()
        callbacks = re.findall(r'static const LimboNative64Entry limbo_native64_(Sys_[A-Za-z0-9_]+)_entry', header.decode())
        if len(callbacks) != 39: raise RuntimeError('Unexpected fixed Sys table')
        real = {'Sys_utfbytes', 'Sys_byte2char', 'Sys_char2byte'}
        fixture = (ROOT/'utils/tests/native64-sys-fixed.c').read_text()
        fixture = fixture.replace('CALLBACKS', '\n'.join('void '+name+'(void *value) { USED(value); abort(); }' for name in callbacks if name not in real))
        fd = port.index('typedef struct FD FD;')
        fixture = fixture.replace('SYS_DESCRIPTORS', port[port.index('uchar\tFDmap[]'):port.index('\n};', fd)+3])
        fixture = fixture.replace('SYS_INITIALIZERS', extract(port, 'void\nsysinit(')+'\n'+extract(port, 'int\nsysnative64typesready('))
        fixture = fixture.replace('SYS_REGISTRATION', '\n'.join(extract(runt, sig) for sig in
                                  ('int\nsysnative64registerfixed(', 'static int\nsysnative64fixedmatches(', 'void\nsysmodinit(')))
        fixture = fixture.replace('SYS_UTF', '\n'.join(extract(runt, sig) for sig in
                                  ('static int\nutfnleng(', 'static int\nsysutfarray(', 'static uchar*\nsysutfresult(',
                                   'void\nSys_utfbytes(', 'void\nSys_byte2char(', 'void\nSys_char2byte(')))
        body = support['constructor']().replace('static int live,', 'static void sysobserve(void);\nstatic int live,', 1)
        body = body.replace('\tcount = 0;\n', '\tsysobserve();\n\tcount = 0;\n', 1)
        source = work/'fixture.c'; source.write_text(body+'\n'+fixture)
        target = work/'fixture'
        subprocess.run(compiler+flags+selected+includes+[str(source), str(ROOT/'lib9/rune.c'), '-o', str(target)], env=env, check=True)
        subprocess.run(runner+[str(target)], env=env, check=True, timeout=60)


if __name__ == '__main__':
    main()
