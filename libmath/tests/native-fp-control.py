#!/usr/bin/env python3
"""Check native Linux compiler floating-point control against actual hardware state."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning native compiler/runtime sources.


# Link complete production functions and retain hardware checks under NDEBUG.
def main():
    with tempfile.TemporaryDirectory(prefix='native-fp-') as temporary:
        binary = Path(temporary)/'fp-control'
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get(
            'CFLAGS', '-O1 -g -DNDEBUG -frounding-math -fno-fast-math -fsanitize=undefined -fno-sanitize-recover=all',
        ))
        command += ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'include')]
        command += [str(ROOT/'libmath/FPcontrol-native.c'), str(Path(__file__).with_suffix('.c'))]
        subprocess.run(command+['-pthread', '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


# Fail the driver if any mask, native register, thread or trap check fails.
if __name__ == '__main__':
    main()
