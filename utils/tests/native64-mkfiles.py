#!/usr/bin/env python3
"""Parse every new mk profile and compile/run matching native-host model probes."""
import os
import platform
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Use actual mk expansion rather than duplicating its variable parser in Python.
def main():
    with tempfile.TemporaryDirectory(prefix='native64-mkfiles-', dir=ROOT) as directory:
        work = Path(directory)
        mk = Path(os.environ.get('NATIVE_MK', ROOT/'native64-tools/mk')).resolve()
        if not mk.is_file():
            env = dict(os.environ, INFERNO_ROOT=str(ROOT), MKBOOTSTRAP_BINDIR=str(work/'tools'))
            subprocess.run(['sh', str(ROOT/'makemk.sh')], cwd=work, env=env, check=True)
            mk = work/'tools/mk'
        for system, arch in (('Linux', 'amd64'), ('Linux', 'arm64'), ('Linux', 'riscv64'), ('Android', 'amd64'), ('Android', 'arm64')):
            profile = ROOT/('mkfiles/mkfile-'+system+'-'+arch)
            source = work/'show.mk'
            source.write_text('ROOT='+str(ROOT)+'\nCROSS=\nANDROID_NDK_HOME=/ndk\nDISMODELFLAGS=\n<'+str(profile)+'\nshow:V:\n\techo PROFILE $CPUS $CC $CFLAGS $LD $LDFLAGS\n')
            result = subprocess.check_output([str(mk), '-s', '-f', str(source), 'show'], cwd=work, text=True)
            if 'PROFILE '+arch not in result or '-m32' in result:
                raise RuntimeError('Incorrect profile expansion: '+result)
            if system == 'Android' and ('21-clang' not in result or '/ndk/' not in result):
                raise RuntimeError('NDK target/API did not expand: '+result)
            print('PASS actual mk profile:', system, arch, flush=True)
        native = {'x86_64': 'amd64', 'amd64': 'amd64', 'aarch64': 'arm64', 'arm64': 'arm64', 'riscv64': 'riscv64'}.get(platform.machine().lower())
        if native is None:
            print('SKIP native execution probes: unsupported host architecture')
            return
        for width, model in ((8, ''), (4, '-DDIS_WORD32=1')):
            code = work/'probe.c'
            code.write_text('#include "lib9.h"\n#include "interp.h"\nint main(void){if(sizeof(void*)!=8 || sizeof(WORD)!='+str(width)+')return 1;return 0;}\n')
            source = work/'probe.mk'
            source.write_text('ROOT='+str(ROOT)+'\nCROSS=\nDISMODELFLAGS='+repr(model)+'\n<'+str(ROOT/('mkfiles/mkfile-Linux-'+native))+'\nprobe:V:\n\t$CC $CFLAGS -o probe.o '+str(code)+'\n\t$LD $LDFLAGS -o probe probe.o\n\t./probe\n')
            subprocess.run([str(mk), '-s', '-f', str(source), 'probe'], cwd=work, check=True)
            print('PASS '+native+' mk compile/link/run: WORD'+str(width*8), flush=True)

        wrong = 'ARM64' if native == 'amd64' else 'AMD64'
        rejected = subprocess.run(['gcc', '-c', '-DLINUX_'+wrong, '-include', str(ROOT/'include/native64-target.h'), str(code),
                                   '-I'+str(ROOT/'Linux/386/include'), '-I'+str(ROOT/'include'), '-o', str(work/'wrong.o')], capture_output=True, text=True)
        if rejected.returncode == 0 or 'profile requires' not in rejected.stderr:
            raise RuntimeError('Wrong-target compiler was not rejected by its profile guard')
        print('PASS mismatched compiler rejection')


if __name__ == '__main__':
    main()
