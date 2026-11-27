#!/usr/bin/env python3
"""Fault actual numeric Module/Jit allocation, RW mapping and RX publication ownership."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Retain the actual constructor/compiler closure; inject only allocator and host OS boundaries.
def unit(control):
    source = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-exec.py'))['unit']()
    source = source[:source.index('/* Raw bytes are poisoned after owned construction')]
    source = 'static int numericthrowat; // One active native allocation error boundary.\nstatic void numericthrow(void); // Defined after the real VM error ABI.\n'+source
    marker = '\tallocations++;'
    if source.count(marker) != 1:
        raise RuntimeError('native allocator marker drift')
    source = source.replace(marker, marker+'\n\tif(allocations == numericthrowat)\n\t\tnumericthrow();', 1)
    if control == 'lease':
        marker = '\tfree64storage(load.lease);'
        fn = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))['function']
        target = fn(source, 'static Module*\ndis64numericbuild(')
        if target.count(marker) != 1:
            raise RuntimeError('partial lease marker drift')
        source = source.replace(target, target.replace(marker, '\t/* Deliberately omit partial lease retirement. */', 1), 1)
    fixture = (ROOT/'utils/tests/dis64-numeric-fault.c').read_text()
    if control == 'mapping':
        # Omit only the OS release; the next baseline must detect the retained mapping.
        marker = '\tresult = __real_munmap(address, bytes);'
        if fixture.count(marker) != 1:
            raise RuntimeError('mapping retirement marker drift')
        fixture = fixture.replace(marker, '\treturn 0; /* Deliberately retain the real mapping. */', 1)
    return source+fixture


# Every control must fail at its exact ownership predicate, never compiler/crash/sanitizer failure.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('lease', 'mapping'))
    args = parser.parse_args()
    fixtures = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'))
    rows = [row[2] for row in fixtures['cases']() if row[1]]
    word = fixtures['W']
    stream = word(len(rows))+b''.join(word(len(raw))+raw for raw in rows)
    arch = os.environ.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    command = shlex.split(os.environ.get('CC', 'cc'))+flags+['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1']
    if arch == 'arm64':
        command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
    elif arch == 'riscv64':
        command += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
    elif arch != 'amd64':
        raise RuntimeError('unsupported native target')
    command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='numeric-fault-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        (work/'test.c').write_text(unit(args.control))
        (work/'fixtures').write_bytes(stream)
        binary = work/'test'
        subprocess.run(command+[str(work/'test.c'), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-Wl,--wrap=mmap', '-Wl,--wrap=munmap', '-Wl,--wrap=mprotect', '-lm', '-o', str(binary)], check=True)
        result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER', ''))+[str(binary), str(work/'fixtures')], capture_output=True, text=True, timeout=60)
        if args.control:
            stage = 'root baseline' if args.control == 'lease' else 'mapping baseline'
            if result.returncode != 10 or result.stderr.strip() != 'numeric fault mismatch: '+stage:
                raise RuntimeError('numeric rollback control missed predicate: '+str(result.returncode)+' '+result.stdout+result.stderr)
            print('PASS numeric Jit rollback control:', args.control, arch)
        elif result.returncode:
            raise RuntimeError(result.stdout+result.stderr)
        else:
            print(result.stdout, end='')


if __name__ == '__main__':
    main()
