#!/usr/bin/env python3
"""Check actual RV64 compiler caches across WORD sizes, ISA features and code allocators."""
import os
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Keep the compiler source replaceable for an independently compiled parent regression control.
def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    source = Path(os.environ.get('RISCV_CACHE_SOURCE', ROOT/'libinterp/comp-riscv.c')).resolve()
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    includes = ['-I'+str(ROOT/p) for p in ('Linux/386/include', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='riscv-cache-') as directory:
        work = Path(directory)
        for model, selection in (('WORD64', []), ('WORD32', ['-DDIS_WORD32=1'])):
            for feature, extensions in (('I', []), ('Zmmul', ['-D__riscv_mul=1']), ('M', ['-D__riscv_mul=1', '-D__riscv_div=1'])):
                for allocator, mapping in (('embedded', []), ('pages', ['-DRV_CODE_PAGES=1'])):
                    label = '-'.join((model, feature, allocator))
                    binary = work/label
                    command = compiler+flags+selection+extensions+mapping+includes+[
                        '-DRVXLEN=64', '-DCOMPILER_IMPLEMENTATION="'+str(source)+'"',
                        str(ROOT/'libinterp/tests/riscv-cache.c'), '-o', str(binary)]
                    subprocess.run(command, check=True)
                    subprocess.run([str(binary)], check=True)
                    print('PASS RV compiler cache configuration:', label, flush=True)


if __name__ == '__main__':
    main()
