#!/usr/bin/env python3
"""Run the actual Android POSIX path resolver without depending on an APK or storage grants."""
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


# Exercise real filesystem paths, bounded publication, fallback and atomic configuration.
def main():
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='android-storage-', dir=ROOT) as directory:
        work = Path(directory)
        library = work/'storage.so'
        subprocess.run(compiler+flags+['-shared', '-fPIC', str(ROOT/'emu/Android/storage.c'), '-o', str(library)], check=True)
        api = C.CDLL(str(library))
        api.inferno_android_set_storage.argtypes = [C.c_char_p, C.c_char_p]
        api.inferno_android_root.argtypes = [C.c_void_p, C.c_size_t]
        checks = 0

        def check(condition):
            nonlocal checks
            if not condition:
                raise RuntimeError('Android storage predicate failed at '+str(checks))
            checks += 1

        def configure(inside=None, outside=None):
            return api.inferno_android_set_storage(None if inside is None else os.fsencode(inside),
                                                  None if outside is None else os.fsencode(outside))

        def resolve(expected=None, size=4096):
            buffer = C.create_string_buffer(b'X'*4096)
            before = bytes(buffer)
            result = api.inferno_android_root(buffer, size)
            if expected is None:
                check(result == -1 and bytes(buffer) == before)
            else:
                check(result == 0 and os.fsdecode(buffer.value) == str(expected))
                end = len(os.fsencode(expected))+1
                check(bytes(buffer)[end:] == before[end:])

        keys = ('INFERNO_ROOT', 'INFERNO_FILES_DIR', 'EXTERNAL_STORAGE')
        previous = {key: os.environ.get(key) for key in keys}
        try:
            for key in keys:
                os.environ.pop(key, None)
            check(configure() == 0)
            resolve()
            inside, outside, legacy = (work/name for name in ('private files', 'external', 'legacy'))
            for path in (inside, outside, legacy):
                path.mkdir()
            check(configure(inside, outside) == 0)
            resolve(inside/'Inferno')
            check((inside/'Inferno').is_dir())
            check(configure('relative', outside) == -1)
            resolve(inside/'Inferno')
            check(configure('/'+'x'*4096, outside) == -1)
            resolve(inside/'Inferno')
            check(configure(work/'missing', outside) == 0)
            resolve(outside/'Inferno')
            check(configure() == 0)
            os.environ['EXTERNAL_STORAGE'] = str(legacy)
            resolve()
            check(not (legacy/'Inferno').exists())
            (legacy/'Inferno').mkdir()
            resolve(legacy/'Inferno')
            os.environ['INFERNO_FILES_DIR'] = str(inside)
            resolve(inside/'Inferno')
            override = work/'explicit root'
            os.environ['INFERNO_ROOT'] = str(override)
            resolve(override)
            check(override.is_dir())
            os.environ['INFERNO_ROOT'] = 'content://not-a-posix-directory'
            resolve()
            os.environ['INFERNO_ROOT'] = str(work/'plain-file')
            (work/'plain-file').write_text('data')
            resolve()
            os.environ.pop('INFERNO_ROOT')
            os.environ.pop('EXTERNAL_STORAGE')
            for length in (0, 1, 2, len(os.fsencode(inside/'Inferno'))):
                resolve(size=length)
            check(api.inferno_android_root(None, 4096) == -1)
        finally:
            for key, value in previous.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value
        print('PASS Android storage:', checks, 'actual filesystem/publication predicates; no Android device claim')


if __name__ == '__main__':
    main()
