#!/usr/bin/env python3
"""Exercise production Loader descriptor initialization with actual native records and type ownership."""
import argparse
import os
from pathlib import Path
import runpy
import resource
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Retain real dtype/freetype/error protection; inject only registration and Sys initialization.
def unit(control):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    adapter = support['constructor']()
    adapter = adapter.replace('static int live,', 'static int disturb; // Diagnostic mutation at the isolated deallocation boundary.\nstatic void loaderobserve(void);\nstatic int live,', 1)
    adapter = adapter.replace('\tcount = 0;\n', '\tloaderobserve();\n\tcount = 0;\n', 1)
    adapter = adapter.replace('\t\t\tlive--;\n', '\t\t\tlive--;\n\t\t\tif(disturb) strcpy(diagnostic, "cleanup diagnostic");\n', 1)
    loader = (ROOT/'libinterp/loader.c').read_text()
    body = support['function'](loader, 'void\nloadermodinit(')
    if control:
        old, new = {
            'retirement': ('\t\tfreetype(types[i]);', '\t\tUSED(types[i]);'),
            'publication': ('\tif(types[0] == nil)', '\tTinst = types[0];\n\tif(types[0] == nil)'),
            'registration': ('builtinmod("$Loader", Loadermodtab, Loadermodlen) == nil', 'builtinmod("$Loader", Loadermodtab, Loadermodlen) != nil'),
        }[control]
        if body.count(old) != 1:
            raise RuntimeError('Loader initializer control drift: '+control)
        body = body.replace(old, new, 1)
    fixture = (ROOT/'utils/tests/native64-loader-init.c').read_text()
    fixture = fixture.replace('LOADER_GLOBALS', loader[loader.index('static uchar\tInstmap'):loader.index('/* Stage Loader')])
    return adapter+'\n\n'+fixture.replace('LOADER_INITIALIZER', body)


# A strict semantic control must fail before stale descriptors or leaked allocations are consumed.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('retirement', 'publication', 'registration'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='native64-loader-init-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        source = work/'fixture.c'
        source.write_text(unit(args.control))
        for bits in ((32,) if args.control else (32, 64)):
            target = work/('word'+str(bits))
            subprocess.run(compiler+flags+(['-DDIS_WORD32=1'] if bits == 32 else [])+includes+[str(source), '-o', str(target)], env=env, check=True)
            result = subprocess.run(runner+[str(target)], env=env, capture_output=True, text=True, timeout=30)
            if args.control:
                if result.returncode != 10 or 'Loader type initialization mismatch' not in result.stderr:
                    raise RuntimeError('Loader control missed its semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS Loader initialization control:', args.control, arch, flush=True)
            else:
                if result.returncode:
                    raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
