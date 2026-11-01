#!/usr/bin/env python3
"""Run production segflush with actual native64 emulator declarations and OS mappings."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Neither executable memory nor page sizing is replaced by an adapter in this native host gate.
def main():
    with tempfile.TemporaryDirectory(prefix='host64-cache-',dir=ROOT) as directory:
        binary=Path(directory)/'run'
        command=['cc','-O2','-DNDEBUG','-fsanitize=undefined','-fno-sanitize-recover=all']
        command+=['-I'+str(ROOT/path) for path in ('emu/Linux','emu/port','Linux/amd64/include','Linux/386/include','include')]
        command+=[str(ROOT/'emu/Linux/segflush-amd64.c'),str(ROOT/'utils/tests/host64-cache.c'),'-o',str(binary)]
        subprocess.run(command,check=True)
        subprocess.run([str(binary)],check=True)


if __name__=='__main__':
    main()
