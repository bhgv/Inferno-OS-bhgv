#!/usr/bin/env python3
"""Execute the first proposed structural scalar import with actual published Math and owned Frame64 metadata."""
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


# Link production Math and constructors; bound only native allocation/error/result transport.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--libdir', type=Path, required=True)
    parser.add_argument('--inject-callee', action='store_true', help='inject a post-production VM error in a copied owning Math unit')
    parser.add_argument('--control', choices=('signature', 'module', 'type', 'frame'), help='require a compiled old-contract mutation to fail an active predicate')
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    support = runpy.run_path(str(ROOT/'utils/tests/native64-builtins.py'))
    constructor = support['constructor']()
    with tempfile.TemporaryDirectory(prefix='native64-fixed-math-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        (work/'math-native64.h').write_bytes(subprocess.check_output([str(args.limbo.resolve()), '-H', str(ROOT/'module/math.m')]))
        golden = ''
        for name in ('strings', 'imports', 'signature'):
            data = bytes.fromhex((ROOT/'libinterp/tests/dis64/fixed-math'/(name+'.hex')).read_text())
            golden += 'static uchar golden_'+name+'[] = {'+','.join(str(x) for x in data)+'}; // Independent shared fixture bytes.\n'
        (work/'fixed-math-golden.h').write_text(golden)
        old = (ROOT/'utils/tests/native64-math-register.c').read_text().replace('int\nmain(void)', 'int\nnative64_full_math_main(void)', 1)
        (work/'test.c').write_text(constructor+old+(ROOT/'utils/tests/native64-fixed-math.c').read_text())
        includes = ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        mathsource = ROOT/'libinterp/math.c'
        if args.inject_callee:
            source = mathsource.read_text()
            body = support['function'](source, 'void\nMath_bits64real(')
            injected = body.replace('memmove(f->ret, &result, sizeof(result));', 'memmove(f->ret, &result, sizeof(result));\n\tnative64testcallee();')
            mathsource = work/'math-injected.c'
            mathsource.write_text('extern void native64testcallee(void); // Test-only post-production error boundary.\n'+source.replace(body, injected, 1))
            flags += ['-DFIXED_CALLEE_INJECTED=1']
        if args.control:
            mutations = {
                'signature': ('return equal;', 'return 1;'),
                'module': ('module->ref++;', '/* Removed module owner control. */'),
                'type': ('type->ref++;', '/* Removed Type owner control. */'),
                'frame': ('free(state.frame);', '/* Removed frame retirement control. */'),
            }
            helper = (ROOT/'libinterp/math-fixed-call.h').read_text()
            old, new = mutations[args.control]
            if helper.count(old) != 1:
                raise RuntimeError('Mutation boundary changed: '+args.control)
            (work/'math-fixed-call.h').write_text(helper.replace(old, new, 1))
            source = mathsource.read_text()
            mathsource = work/'math-control.c'; mathsource.write_text(source)
        library = args.libdir.resolve()
        for label, model in (('WORD32', ['-DDIS_WORD32=1']), ('WORD32-file', ['-DDIS_WORD32=1', '-DDIS64_PILOT_LOADER=1'])):
            command = compiler+flags+model+includes
            obj = work/(label+'.o')
            subprocess.run(command+['-DDIS_NATIVE64_FIXED_MATH_CALL=1', '-DDIS_NATIVE64_MATH_REGISTER=1', '-DDIS_NATIVE64_MATH_HEADER="math-native64.h"', '-Dmallocz=native64testalloc', '-Dfree=native64testfree', '-c', str(mathsource), '-o', str(obj)], env=env, check=True)
            binary = work/label
            subprocess.run(command+[str(work/'test.c'), str(obj), '-Wl,--wrap=fmtinstall', '-Wl,--start-group', str(library/'libmath.a'), str(library/'lib9.a'), '-Wl,--end-group', '-lm', '-pthread', '-o', str(binary)], env=env, check=True)
            if args.control:
                result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=90)
                if result.returncode not in (-signal.SIGABRT, 128+signal.SIGABRT) or 'check failed' not in result.stderr:
                    raise RuntimeError('Control missed its active predicate: '+args.control+'\n'+result.stdout+result.stderr)
                print('PASS fixed scalar control:', args.control, arch, label, flush=True)
                continue
            subprocess.run(runner+[str(binary)], env=env, check=True)
            print('PASS fixed scalar import owning Math:', arch, label, flush=True)
        if not args.control:
            explicit = ['-DDIS_WORD32=1', '-DDIS_NATIVE64_MATH_REGISTER=1', '-DDIS_NATIVE64_MATH_HEADER="math-native64.h"']
            for selection, message in ((explicit+['-DDIS_NATIVE64_FIXED_MATH_CALL=0'], 'must be 1'),
                                       (explicit+['-DDIS_NATIVE64_FIXED_MATH_CALL=2'], 'must be 1'),
                                       (['-DDIS_WORD32=1', '-DDIS_NATIVE64_FIXED_MATH_CALL=1'], 'requires explicit native64 Math'),
                                       (explicit[1:]+['-DDIS_NATIVE64_FIXED_MATH_CALL=1'], 'requires DIS_WORD32=1')):
                result = subprocess.run(compiler+flags+selection+includes+['-fsyntax-only', str(ROOT/'libinterp/math.c')], env=env, capture_output=True, text=True)
                if result.returncode == 0 or message not in result.stderr:
                    raise RuntimeError('Fixed call accepted an incoherent selection: '+result.stderr)
            obj = work/'disabled.o'
            subprocess.run(compiler+flags+explicit+includes+['-ffunction-sections', '-fdata-sections', '-c', str(ROOT/'libinterp/math.c'), '-o', str(obj)], env=env, check=True)
            unit = work/'disabled.c'
            unit.write_text('#include "lib9.h"\n#include "interp.h"\n#include "native64-math-call.h"\nint main(void){return Math_bits64real64((const Native64MathBitsCall*)1)!=0 || Math_bits64realframe64((const Native64MathFrameCall*)1)!=0;}\n')
            binary = work/'disabled'
            subprocess.run(compiler+flags+includes+[str(unit), str(obj), '-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
            subprocess.run(runner+[str(binary)], env=env, check=True)
            print('PASS fixed call: four incoherent selections and disabled pre-read refusal')


if __name__ == '__main__':
    main()
