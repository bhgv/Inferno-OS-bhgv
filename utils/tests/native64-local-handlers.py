#!/usr/bin/env python3
"""Resume trusted native calls through the actual emu/os exception handlers."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Reuse the owning VM closure and constructor, replacing only the test entry point.
def unit(port, control=None):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-local-calls.py'))
    source = support['unit']().split('/* Run real FRAME/CALL/RET')[0]
    source = source.replace('dtype(freeheap, 96, rootmap, sizeof(rootmap))', 'dtype(freeheap, 112, rootmap, sizeof(rootmap))')
    source = source.replace('uchar rootmap[] = {0, 0x10}', 'uchar rootmap[] = {0, 0x18}')
    runtime = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    extract = runtime['function']
    handler = (ROOT/port/'port/exception.c').read_text()
    parts = [extract(handler, signature) for signature in (
        'static int\nematch(', 'static Type*\nframetype(', 'static void\nfreeframe(',
        'static int\nexceptionframe(', 'static ulong\nexceptionpc(',
        'static int\nexceptiontarget(', 'int\nhandler(')]
    if control == 'pending':
        # Retain the caller's new traversal, but restore the previous single-frame release.
        parts[2] = """/* Control: restore the old single-frame cleanup body. */
static void
freeframe(uchar *fp, int setsp)
{
    Frame *f;
    Type *type;

    f = (Frame*)fp;
    memmove(&type, fp+O(Frame, t), sizeof(type));
    if(type == nil)
        unextend(f);
    else if(type->np)
        freeptrs(f, type);
    if(setsp)
        R.SP = fp;
}"""
    elif control == 'registers':
        parts[-1] = parts[-1].replace('destroy(old);\n\tmemmove(&R, &resumed, sizeof(R));',
                                     'destroy(old); /* Removed old-owner REG restoration. */', 1)
    fixture = (ROOT/'utils/tests/native64-local-handlers.c').read_text()
    return source+'\n\n'+fixture.replace('HANDLER_IMPLEMENTATION', '\n\n'.join(parts))


# Execute both active word models with the owning backend and target runner.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('pending', 'registers'))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    command = shlex.split(env.get('CC', 'cc'))+shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    if arch == 'arm64':
        command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
    elif arch == 'riscv64':
        command += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
    elif arch != 'amd64':
        parser.error('unsupported target: '+arch)
    command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    with tempfile.TemporaryDirectory(prefix='native64-local-handlers-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        for port in ('emu', 'os'):
            source = work/(port+'.c')
            source.write_text(unit(port, args.control))
            for bits in (32, 64):
                binary = work/(port+str(bits))
                model = ['-DDIS_WORD32=1'] if bits == 32 else []
                subprocess.run(command+model+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
                if args.control:
                    for jit in (0, 1):
                        code = 1 if args.control == 'pending' else 2
                        result = subprocess.run(runner+[str(binary), str(jit), str(code)], env=env, capture_output=True, text=True, timeout=90)
                        marker = 'prepared handler owner leaked' if code == 1 else 'handler register image lost'
                        if result.returncode != 9+code or marker not in result.stderr:
                            raise RuntimeError('handler removal control missed its predicate: '+result.stdout+result.stderr)
                        print('PASS handler removal control:', arch, port, bits, jit, args.control, flush=True)
                else:
                    subprocess.run(runner+[str(binary)], env=env, check=True, timeout=90)
                print('PASS trusted handler target:', arch, port, 'WORD'+str(bits), flush=True)


if __name__ == '__main__':
    main()
