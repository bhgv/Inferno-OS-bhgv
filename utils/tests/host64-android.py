#!/usr/bin/env python3
"""Compile the real Android startup/compatibility boundary with an explicit modern NDK."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Include ARM32 as a regression target while checking both new LP64 launch boundaries.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True)
    args = parser.parse_args()
    toolchain = args.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin'
    targets = [('arm', 'armv7a-linux-androideabi21', ''),
               ('arm64', 'aarch64-linux-android21', 'ARM64'),
               ('amd64', 'x86_64-linux-android21', 'AMD64')]
    with tempfile.TemporaryDirectory(prefix='host64-android-', dir=ROOT) as directory:
        work = Path(directory)
        env = dict(os.environ, TMPDIR=str(work))
        for arch, target, guard in targets:
            for source in ('amain.c', 'calltrace.c', 'getdtablesize.c', 'devfs.c'):
                command = [str(toolchain/(target+'-clang')), '-c', '-O2', '-fPIC',
                           '-fno-strict-aliasing', '-DANDROID', '-DEMU', '-DROOT="/sdcard/Inferno"']
                command += ['-I'+str(ROOT/path) for path in
                            ('emu/Android', 'emu/port', 'Android/'+arch+'/include',
                             'Android/arm/include', 'include', 'libinterp')]
                if guard:
                    command += ['-DLINUX_'+guard, '-include', str(ROOT/'include/native64-target.h')]
                command += [str(ROOT/'emu/Android'/source), '-o', str(work/(arch+'-'+source+'.o'))]
                subprocess.run(command, env=env, check=True)
            print('PASS modern NDK startup/compatibility objects:', arch, flush=True)


if __name__ == '__main__':
    main()
