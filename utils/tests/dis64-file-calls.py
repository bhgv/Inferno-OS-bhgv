#!/usr/bin/env python3
"""Exercise explicit startup selection, actual file admission/cache and scheduled scalar calls."""
import argparse
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FILES = runpy.run_path(str(ROOT/'libinterp/tests/dis64-fileloader.py'))
INFER = runpy.run_path(str(ROOT/'utils/tests/dis64-call-infer.py'))
CONSTRUCT = runpy.run_path(str(ROOT/'utils/tests/dis64-call-construct.py'))


# Extend the existing actual reader/scheduler closure with production call handlers.
def unit(control=None):
    source = FILES['buildunit']()
    source = source.replace('directory->length = filebytes;', 'directory->length = filebytes;\n\tdirectory->mode = dis64launchmode() ? 0755 : 0;', 1)
    source = source[:source.index('/* Execute a real root process, retain its result owner')]
    extract = FILES['function']
    stack = (ROOT/'libinterp/stack.c').read_text()
    xec = (ROOT/'libinterp/xec.c').read_text()
    parts = [extract(stack, s) for s in ('void\nextend(', 'void\nunextend(')]
    parts += [extract(xec, s) for s in ('static Type*\nmoduletype(', 'static int\nframefits(',
              'OP(frame)', 'OP(call)', 'OP(ret)', 'OP(lea)', 'OP(cvtwf)', 'OP(movl)', 'OP(movf)')]
    if control:
        source = source.replace('static void\nrequire(', 'static void trackedfree(void*); // Retire the bounded ledger at a semantic stop.\n\nstatic void\nrequire(', 1)
        source = source.replace('abort();', 'for(int i=0; i<RuntimeBlocks; i++) if(blocks[i].pointer != NULL) trackedfree(blocks[i].pointer);\n\t\texit(10);', 1)
        signature, old, new = {
            'cache': ('Module*\nreadmod(', 'if(m != nil && !dis64launchcompatible(m)){', 'if(0 && m != nil && !dis64launchcompatible(m)){'),
            'seal': ('int\ndis64selectlaunch(', 'dis64sealed ||', '0 ||'),
            'proof-route': ('static int\ndis64parse(', 'if(dis64launchmode())', 'if(0)'),
        }[control]
        actual = extract(source, signature)
        if actual.count(old) != 1: raise RuntimeError('Launch control drift: '+control)
        source = source.replace(actual, actual.replace(old, new, 1), 1)
    return source+'\n\n'+'\n\n'.join(parts)+'\n\n'+(ROOT/'utils/tests/dis64-file-calls.c').read_text()


# Use fresh processes for immutable startup modes, never reset the production selector.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--control", choices=("cache", "seal", "proof-route"))
    args = parser.parse_args()
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    command = shlex.split(env.get('CC', 'cc'))+flags+['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1']
    if arch == 'arm64': command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
    elif arch == 'riscv64': command += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
    elif arch != 'amd64': raise RuntimeError('Unsupported target '+arch)
    command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    rows = []
    manifest = INFER['BASE']['fixtures']()
    for item in manifest['cases']:
        data = bytes.fromhex((INFER['BASE']['GOLDENS']/(item['name']+'.hex')).read_text())
        rows.append((item['name'], data, item['private_accept'], item['result_kind'] if item['runtime_error'] else 0, 0, item['runtime_error']))
    for kind, name, data, accepted, value in INFER['goldens']():
        rows.append(('derived-'+str(kind)+'-'+name, data, accepted, kind if accepted else 0, value, 0))
    for name, data, accepted, kind, value, _, _, _ in CONSTRUCT['reordered_case']():
        rows.append((name, data, accepted, kind, value, 0))
    with tempfile.TemporaryDirectory(prefix='dis64-file-calls-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        source, binary = work/'calls.c', work/'calls'
        source.write_text(unit(args.control))
        subprocess.run(command+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
        pilot = work/'pilot.dis64'
        pilot.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64/pilot42.hex').read_text()))
        if args.control: rows = rows[:1]
        for name, data, accepted, kind, value, failure in rows:
            path = work/(name+'.dis64')
            path.write_bytes(data)
            result = subprocess.run(runner+[str(binary), str(path), str(int(accepted)), str(kind), str(value), str(pilot), str(failure)],
                                    env=env, capture_output=True, text=True, timeout=90)
            if args.control:
                if result.returncode != 10 or 'Dis64 runtime check failed' not in result.stderr:
                    raise RuntimeError('Launch control missed semantic predicate: '+str(args.control)+'\n'+result.stdout+result.stderr)
                print('PASS file-call semantic control:', args.control, arch, flush=True)
                return
            if result.returncode: raise RuntimeError(result.stdout+result.stderr)
            if name == 'answer42':
                subprocess.run(runner+[str(binary), str(path), '1', '0', '0', str(pilot), '0', 'default'], env=env, check=True)
        print('PASS explicit file calls:', arch, len(rows), 'raw images; fixed startup/cache profiles, copied lowering, C/RX scheduled roots and allocation rollback', flush=True)


if __name__ == '__main__':
    main()
