#!/usr/bin/env python3
"""Check the real x86-64 low-level FP port against the independent libc fenv oracle."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Reuse the existing complete Math oracle while linking the new production register implementation.
def main():
    with tempfile.TemporaryDirectory(prefix='host64-fp-',dir=ROOT) as directory:
        binary=Path(directory)/'run'
        command=shlex.split(os.environ.get('CC','cc'))
        command+=shlex.split(os.environ.get('CFLAGS','-O3 -DNDEBUG -g -fno-strict-aliasing -frounding-math -fno-fast-math -fsanitize=undefined -fno-sanitize-recover=all'))
        command+=['-DLINUX_AMD64','-include',str(ROOT/'include/native64-target.h')]
        command+=['-I'+str(ROOT/path) for path in ('Linux/amd64/include','Linux/386/include','include')]
        command+=[str(ROOT/'lib9/setfcr-Linux-amd64.c'),str(ROOT/'libmath/FPcontrol-Linux.c'),str(ROOT/'libmath/tests/native-fp-control.c'),'-pthread','-lm','-o',str(binary)]
        subprocess.run(command,check=True)
        subprocess.run([str(binary)],check=True)


if __name__=='__main__':
    main()
