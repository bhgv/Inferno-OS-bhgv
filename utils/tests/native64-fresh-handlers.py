#!/usr/bin/env python3
"""Build real exception messages after native call unwind and scan published roots."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Refuse silent fixture drift when specializing the previous owning call matrix.
def replace(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError('fresh handler fixture marker is not unique: '+old[:80])
    return source.replace(old, new, 1)


# Keep actual handler, string constructors and root tracing; bound only OS/pool services.
def unit(port, control=None):
    support = runpy.run_path(str(ROOT/'utils/tests/native64-local-handlers.py'))
    runtime = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    extract = runtime['function']
    source = support['unit'](port)
    source = replace(source, 'dtype(freeheap, 112, rootmap, sizeof(rootmap))', 'dtype(freeheap, 128, rootmap, sizeof(rootmap))')
    source = replace(source, 'uchar rootmap[] = {0, 0x18}', 'uchar rootmap[] = {0, '+('0x18' if control == 'map' else '0x1a')+'}')
    source = replace(source, 'block = trackedallocate(offsetof(Bhdr, u.data)+bytes, 0);', 'allocroots();\n\tblock = trackedallocate(offsetof(Bhdr, u.data)+bytes, 0);')
    marker = '/* Return an actual pool block header'
    source = replace(source, marker, 'static void allocroots(void); // Observe only enabled message-allocation boundaries.\n\n\n'+marker)
    gc = (ROOT/'libinterp/gc.c').read_text()
    prefix = '''static int marker, sweeper, gce, visit, markdepth; // Actual tracing state, without a sweep driver.
int gccolor = 3; // Stable three-color phase used for the controlled root scan.


/* This fixture has no externally registered pointer roots. */
static void
ptrmark(void)
{
}
'''
    bodies = [extract(gc, signature) for signature in ('static int\ngcintadd(', 'static void\ngcspend(', 'void\nmarkheap(', 'static void\nrootset(')]
    source = replace(source, extract(source, 'void\nmarkheap('), prefix+'\n\n'+'\n\n'.join(bodies))
    strings = (ROOT/'libinterp/string.c').read_text()
    heap = (ROOT/'libinterp/heap.c').read_text()
    handler = (ROOT/port/'port/exception.c').read_text()
    constructors = [extract(heap, 'int\nhmsize(')]
    constructors += [extract(strings, signature) for signature in ('String*\nnewstring(', 'String*\nnewrunes(', 'String*\nc2string(')]
    constructors += [extract(handler, signature) for signature in ('void\nexcinit(', 'static void\nsetstr(', 'static String*\nnewestring(')]
    source = replace(source, extract(source, 'int\nmain('), '')
    fixture = (ROOT/'utils/tests/native64-fresh-handlers.c').read_text().replace('MESSAGE_IMPLEMENTATION', '\n\n'.join(constructors))
    source = replace(source, extract(source, 'static String*\nnewestring('), fixture)
    source = replace(source, 'extent = process.R.EX;', 'extent = process.R.EX;\n\tfreshsetup(module);')
    source = replace(source, 'exception = H2D(void*, heapz(&Tstring));\n\tprocess.exval = exception;\n\trequire(handler(message) == 1);', 'exception = H;\n\tprocess.exval = H;\n\trequire(freshinvoke(failure) == 1);')
    source = replace(source, 'require(published == exception && process.exval == H && oldreleased == 1);', 'require(process.exval == H && oldreleased == 1);\n\tfreshresult(published);')
    source = replace(source, 'free(process.exstr);', 'free(process.exstr);\n\tdestroy(exstr);\n\texstr = H;')
    if control == 'publication':
        source = replace(source, '/* Publish the surviving caller before cleanup or message allocation can scan roots. */\n\tmemmove(&p->R, &R, sizeof(R));', '/* Removed early caller-root publication control. */')
    return source


# Run both handler ports and numeric word models on each owning native backend.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('publication', 'map'))
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
    with tempfile.TemporaryDirectory(prefix='native64-fresh-handlers-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        for port in ('emu', 'os'):
            source = work/(port+'.c')
            source.write_text(unit(port, args.control))
            for bits in (32, 64):
                binary = work/(port+str(bits))
                model = ['-DDIS_WORD32=1'] if bits == 32 else []
                subprocess.run(command+model+[str(source), str(ROOT/'libinterp/dec.c'), str(ROOT/'lib9/rune.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
                if args.control:
                    for jit in (0, 1):
                        code = 12 if args.control == 'publication' else 13
                        result = subprocess.run(runner+[str(binary), str(jit)], env=env, capture_output=True, text=True, timeout=90)
                        marker = 'stale process roots before message' if code == 12 else 'live caller graph was not marked'
                        if result.returncode != code or marker not in result.stderr:
                            raise RuntimeError('fresh message control missed its predicate: '+result.stdout+result.stderr)
                        print('PASS fresh handler control:', arch, port, bits, jit, args.control, flush=True)
                else:
                    subprocess.run(runner+[str(binary)], env=env, check=True, timeout=180)
                print('PASS fresh handler target:', arch, port, 'WORD'+str(bits), flush=True)


if __name__ == '__main__':
    main()
