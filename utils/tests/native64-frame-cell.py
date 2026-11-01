#!/usr/bin/env python3
"""Verify generated scalar VM frame cells with actual collector/heap/Math and bounded OS pools."""
from pathlib import Path
import argparse
import os
import runpy
import shlex
import signal
import resource
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Retain the actual collector graph; generated caller maps are independent of the result resolver.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--live', action='store_true', help='publish live interpreter REG before acquired lease and actual collection')
    parser.add_argument('--lease', action='store_true', help='retain a generation-checked caller across the combined protected call')
    parser.add_argument('--gateway', action='store_true', help='combine the structural fixed call with real protected VM/collector bodies')
    parser.add_argument('--control', choices=('scalar', 'identity', 'boundary', 'map'))
    args = parser.parse_args()
    if args.live and not args.lease:
        parser.error('--live requires --gateway --lease')
    if args.lease and not args.gateway:
        parser.error('--lease requires --gateway')
    if args.gateway and args.control not in (None, 'map'):
        parser.error('The combined gateway supports the GC-map removal control; use standalone mode for cell-validation controls')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    compiler = shlex.split(env.get('CC', 'cc'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    support = runpy.run_path(str(ROOT/'utils/tests/native64-gc.py'))
    with tempfile.TemporaryDirectory(prefix='native64-frame-cell-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory); env['TMPDIR'] = str(work)
        (work/'roots.m').write_text('Roots: module { Node: adt {next: ref Node;}; copy: fn(source: array of ref Node): array of ref Node; caller: fn(padding: big,result: array of ref Node,tail: array of byte); many: fn('+','.join('a'+str(i)+': array of byte' for i in range(150))+'); };')
        (work/'scalar.m').write_text('Scalar: module {caller: fn(padding: big,result: real,keep: array of byte);};')
        for name, source in (('math', ROOT/'module/math.m'), ('roots', work/'roots.m'), ('scalar', work/'scalar.m')):
            (work/(name+'-native64.h')).write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', str(source)]))
        helper = (ROOT/'include/native64-frame-cell.h').read_text()
        if args.control == 'scalar': helper = helper.replace('if(type->map[slot/8] & (0x80>>(slot%8)))', 'if(0)')
        if args.control == 'identity': helper = helper.replace('type != expected || ', '')
        if args.control == 'boundary': helper = helper.replace('return found ? (void*)(target+offset) : nil;', 'return (void*)(target+offset);')
        (work/'native64-frame-cell.h').write_text(helper)
        framebody = (ROOT/'utils/tests/native64-frame-cell.c').read_text()
        if args.gateway:
            result = runpy.run_path(str(ROOT/'utils/tests/native64-result.py'))
            source = result['unit']('emu').replace('int\nmain(void)', 'int\nnative64_previous_result_main(void)', 1)
            function = result['function']
            source = source.replace(function(source, 'void\nunload('), function((ROOT/'libinterp/load.c').read_text(), 'void\nunload('), 1)
            source += framebody.replace('int\nmain(void)', 'int\nnative64_previous_cell_main(void)', 1)
            source += (ROOT/'utils/tests/native64-frame-gateway.c').read_text()
            golden = ''
            for name in ('strings', 'imports'):
                data = bytes.fromhex((ROOT/'libinterp/tests/dis64/fixed-math'/(name+'.hex')).read_text())
                golden += 'static uchar golden_'+name+'[] = {'+','.join(str(x) for x in data)+'}; // Shared independent fixture.\n'
            (work/'fixed-math-golden.h').write_text(golden)
        else:
            source = support['unit']().replace('int\nmain(int argc, char **argv)', 'int\nnative64_previous_main(int argc, char **argv)', 1)+framebody
        (work/'test.c').write_text(source)
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for label, model in (('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])):
            command = compiler+flags+model+includes+(['-DFRAMECELL_LIVE=1'] if args.live else [])+(['-DFRAMECELL_LEASE=1', '-DDIS_NATIVE64_CALLER_GUARD=1', '-DDIS_NATIVE64_FIXED_MATH_CALL=1'] if args.lease else [])+['-ffunction-sections', '-fdata-sections']
            objects = []
            for name in ('heap', 'math'):
                obj = work/(label+'-'+name+'.o')
                options = ['-DDIS_NATIVE64_MATH_HEADER="math-native64.h"'] if name == 'math' else []
                if args.gateway and name == 'math':
                    options += ['-DDIS_NATIVE64_MATH_REGISTER=1', '-DDIS_NATIVE64_FIXED_MATH_CALL=1', '-Dmallocz=native64gatewayalloc', '-Dfree=native64gatewayfree']
                path = ROOT/'libinterp'/(name+'.c')
                if args.gateway and name == 'math':
                    text = path.read_text()
                    body = function(text, 'void\nMath_bits64real(')
                    injected = body.replace('memmove(f->ret, &result, sizeof(result));', 'memmove(f->ret, &result, sizeof(result));\n\tnative64gatewayafter();')
                    path = work/'math-safepoint.c'
                    path.write_text('extern void native64gatewayafter(void); // Test-only post-production safepoint.\n'+text.replace(body, injected, 1))
                subprocess.run(command+options+['-c', str(path), '-o', str(obj)], env=env, check=True)
                objects.append(str(obj))
            binary = work/label
            subprocess.run(command+(['-DFRAMECELL_ROOT_CONTROL=1'] if args.control == 'map' else [])+[str(work/'test.c')]+objects+['-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=90)
            if args.control:
                if result.returncode not in (-signal.SIGABRT, 128+signal.SIGABRT) or 'GC root predicate failed' not in result.stderr:
                    raise RuntimeError('Frame-cell control missed active predicate: '+args.control+'\n'+result.stdout+result.stderr)
                print('PASS VM frame-cell control:', args.control, arch, label, flush=True)
            else:
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)


if __name__ == '__main__':
    main()
