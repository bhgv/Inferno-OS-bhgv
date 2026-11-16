#!/usr/bin/env python3
"""Run a classic Dis program on the actual ELF32 x86 console, with JIT off/on."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import tempfile


PROGRAM = '''implement Classic32;
include "sys.m";
Classic32: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    if(sys == nil)
        raise "classic32: missing Sys";
    value := 2147483647;
    value += 1;
    if(value != (-2147483647-1))
        raise "classic32: int width";
    wide := (big 1) << 40;
    if((wide >> 40) != big 1)
        raise "classic32: big width";
    values := array[3] of int;
    values[2] = 21;
    if(real values[2] / 2.0 != 10.5)
        raise "classic32: real or array";
    text := "classic" + "32";
    if(text != "classic32" || len text != 9)
        raise "classic32: string";
    sys->print("CLASSIC32_OK\\n");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emu', type=Path, required=True)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    args = parser.parse_args()
    root, emu = args.root.resolve(), args.emu.resolve()
    header = emu.read_bytes()[:20]
    if header[:6] != b'\x7fELF\x01\x01' or int.from_bytes(header[18:20], 'little') != 3:
        raise RuntimeError('Expected the actual little-endian ELF32 x86 emulator')
    env = dict(os.environ)
    env.pop('EMU', None)
    with tempfile.TemporaryDirectory(prefix='classic32-boot-', dir=root) as directory:
        work = Path(directory)
        source, image = work/'classic.b', work/'classic.dis'
        source.write_text(PROGRAM)
        subprocess.run([str(args.limbo.resolve()), '-i', '-I', str(root/'module'),
                        '-o', str(image), str(source)], env=env, check=True, timeout=60)
        if not image.is_file() or not image.stat().st_size or image.read_bytes().startswith(b'Dis64'):
            raise RuntimeError('The classic compiler did not produce ordinary Dis')
        for jit in (0, 1):
            # Linux cleanexit kills its process group; keep the test runner outside it.
            process = subprocess.Popen([str(emu), '-v', '-r', str(root), '-c', str(jit),
                                        '-d', '/'+str(image.relative_to(root))], env=env,
                                       start_new_session=True, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True)
            try:
                output, _ = process.communicate(timeout=30)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                output, _ = process.communicate()
                raise RuntimeError('Classic console did not terminate: '+output)
            if (process.returncode not in (0, -signal.SIGKILL) or
                output.count('CLASSIC32_OK') != 1 or
                any(error in output for error in ('panic:', 'Broken:', 'faults:'))):
                raise RuntimeError('Classic32 JIT='+str(jit)+' failed: '+output)
            print('PASS actual ELF32 console: classic Dis, Sys, int/big/real/array/string, JIT='+str(jit))


if __name__ == '__main__':
    main()
