#!/usr/bin/env python3
"""Run actual Sys descriptor construction with real dtype/freetype and fault ownership checks."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep construction/cleanup bodies real while bounding only allocator/process/payload callbacks.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--emu-source', type=Path, help='Optional parent source for an active failure control')
    args = parser.parse_args()
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    prefix = support['constructor']()
    prefix = prefix.replace('static void*\nallocate(size_t size)', 'static void observeinit(void);\n\n\n/* Observe publication before allocator entry. */\nstatic void*\nallocate(size_t size)')
    prefix = prefix.replace('\tcount = 0;\n', '\tobserveinit();\n\tcount = 0;\n', 1)
    env = dict(os.environ)
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='native64-sys-init-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        (work/'sys-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', str(ROOT/'module/sys.m')], cwd=work, env=env))
        compiler = shlex.split(env.get('CC', 'cc'))
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for platform in ('emu', 'os'):
            sourcepath = args.emu_source if platform == 'emu' and args.emu_source else ROOT/(platform+'/port/inferno.c')
            source = sourcepath.read_text()
            fd = source[source.index('typedef struct FD FD;'):source.index('\n};', source.index('typedef struct FD FD;'))+3]
            unit = (ROOT/'utils/tests/native64-sys-init.c').read_text()
            unit = unit.replace('FD_DECLARATION', fd).replace('SYS_INITIALIZER', support['function'](source, 'void\nsysinit('))
            (work/'test.c').write_text(prefix+unit)
            for label, model in [('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])]:
                for mode, selection in [('normal', []), ('wide', ['-DSYS_INIT_WIDE']), ('smallmap', ['-DSYS_INIT_WIDE', '-DSYS_INIT_SMALL_MAPS'])]:
                    binary = work/(platform+'-'+label+'-'+mode)
                    subprocess.run(compiler+flags+model+selection+includes+[str(work/'test.c'), '-o', str(binary)], env=env, check=True)
                    subprocess.run(runner+[str(binary)], env=env, check=True)
                    print('PASS Sys initializer:', platform, label, mode, flush=True)


if __name__ == '__main__':
    main()
