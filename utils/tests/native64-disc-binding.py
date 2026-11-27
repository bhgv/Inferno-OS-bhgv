#!/usr/bin/env python3
"""Compile the actual static-image registry and test non-destructive mode binding."""
import argparse
import os
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Run each target's real C implementation; control failures require the semantic predicate.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('mode', 'jit', 'image'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    cc = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    with tempfile.TemporaryDirectory(prefix='disc-binding-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        source = work/'fixture.c'
        source.write_text((ROOT/'utils/tests/native64-disc-binding.c').read_text())
        if args.control:
            body = (ROOT/'libinterp/disc.c').read_text()
            old, new = {
                'mode': ('if(module->cstep != nil && module->cstep != p->step)', 'if(0)'),
                'jit': ('module->jit != nil || module->compiled', 'module->compiled'),
                'image': ('memcmp(p->image, image, length) == 0', '1'),
            }[args.control]
            if body.count(old) != 1:
                raise RuntimeError('attachment control marker drift')
            (work/'disc.c').write_text(body.replace(old, new))
        includes = ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'include', 'libinterp')]
        for bits in ((32,) if args.control else (32, 64)):
            binary = work/('word'+str(bits))
            subprocess.run(cc+flags+includes+(['-DDIS_WORD32=1'] if bits == 32 else [])+[str(source), '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=30)
            if args.control:
                if result.returncode != 10 or 'static translation binding mismatch' not in result.stderr:
                    raise RuntimeError('attachment control missed semantic predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
                print('PASS static binding control:', args.control, arch, flush=True)
            else:
                if result.returncode:
                    raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
