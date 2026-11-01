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
    parser.add_argument('--caller-control', choices=('epoch', 'hold', 'type', 'clear', 'self', 'live', 'publish', 'rebind'), help='require a compiled lease-contract removal to fail its active oracle')
    parser.add_argument('--caller-port', choices=('emu', 'os'), default='emu', help='owning lifecycle-guard source for lease checks')
    parser.add_argument('--caller-guard', action='store_true', help='check generation/lifetime leases and guarded producer cleanup')
    parser.add_argument('--inject-callee', action='store_true', help='inject a post-production VM error in a copied owning Math unit')
    parser.add_argument('--control', choices=('signature', 'module', 'type', 'frame'), help='require a compiled old-contract mutation to fail an active predicate')
    args = parser.parse_args()
    if args.caller_control and not args.caller_guard:
        parser.error('--caller-control requires --caller-guard')
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
        if args.caller_control:
            helper = (ROOT/'include/native64-caller.h').read_text()
            old, replacement = {
                'live': ('prog->native64live == caller->live && native64callerregisters(prog) &&', ''),
                'publish': ('prog->R = candidate.R;', '/* Removed live publication control. */'),
                'rebind': ('\n\t\tR.FP = candidate.R.FP;', '\n\t\tUSED(caller); /* Removed live transfer control. */'),
                'epoch': ('prog->native64epoch == caller->epoch && ', ''),
                'hold': ('prog->native64holds = 1;', '/* Removed hold control. */'),
                'type': ('type->ref++;', '/* Removed Type retention control. */'),
                'clear': ('freetype(type);', '/* Removed Type retirement control. */'),
                'self': ('if(caller == nil || caller->self != caller || caller->prog == nil)', 'if(caller == nil || caller->self == nil || caller->prog == nil)'),
            }[args.caller_control]
            if helper.count(old) != 1: raise RuntimeError('Lease mutation boundary changed')
            (work/'native64-caller.h').write_text(helper.replace(old, replacement, 1))
        old = (ROOT/'utils/tests/native64-math-register.c').read_text().replace('int\nmain(void)', 'int\nnative64_full_math_main(void)', 1)
        fixed = (ROOT/'utils/tests/native64-fixed-math.c').read_text()
        if args.caller_guard:
            fixed = fixed.replace('int\nmain(void)', 'int\nnative64_fixed_call_main(void)', 1)
            fixed = 'extern void native64callerobserve(void); // Test-only producer observation.\n'+fixed
            fixed = fixed.replace('memory = zeroallocate(bytes, clear);', 'native64callerobserve();\n\tmemory = zeroallocate(bytes, clear);')
            body = (ROOT/'utils/tests/native64-caller.c').read_text()
            guards = ''
            scheduler = (ROOT/args.caller_port/'port/dis.c').read_text()
            for name, variable, result in (('delprog', 'p', 'return;'), ('progexit', 'r', 'return;'), ('killprog', 'p', 'return -1;'), ('release', 'unused', 'return;')):
                function = support['function'](scheduler, ('int' if name == 'killprog' else 'void')+'\n'+name+'(')
                start = function.index('#ifdef DIS_NATIVE64_CALLER_GUARD')
                block = function[start:function.index('#endif', start)+len('#endif')]
                guards += ('int' if name == 'killprog' else 'void')+' test_'+name+'(Prog *'+variable+'){\n'+block+'\n'+result+'\n}\n'
            fixed += body.replace('LIFECYCLE_GUARDS', guards)
        (work/'test.c').write_text(constructor+old+fixed)
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
            command = compiler+flags+model+includes+(['-DDIS_NATIVE64_CALLER_GUARD=1', '-DDIS_NATIVE64_FIXED_MATH_CALL=1'] if args.caller_guard else [])
            obj = work/(label+'.o')
            subprocess.run(command+['-DDIS_NATIVE64_FIXED_MATH_CALL=1', '-DDIS_NATIVE64_MATH_REGISTER=1', '-DDIS_NATIVE64_MATH_HEADER="math-native64.h"', '-Dmallocz=native64testalloc', '-Dfree=native64testfree', '-c', str(mathsource), '-o', str(obj)], env=env, check=True)
            binary = work/label
            subprocess.run(command+[str(work/'test.c'), str(obj), '-Wl,--wrap=fmtinstall', '-Wl,--start-group', str(library/'libmath.a'), str(library/'lib9.a'), '-Wl,--end-group', '-lm', '-pthread', '-o', str(binary)], env=env, check=True)
            if args.control or args.caller_control:
                result = subprocess.run(runner+[str(binary)], env=env, capture_output=True, text=True, timeout=90)
                if result.returncode not in (-signal.SIGABRT, 128+signal.SIGABRT) or 'check failed' not in result.stderr:
                    raise RuntimeError('Control missed its active predicate: '+str(args.control or args.caller_control)+'\n'+result.stdout+result.stderr)
                print('PASS fixed scalar/lease control:', args.control or args.caller_control, arch, label, flush=True)
                continue
            subprocess.run(runner+[str(binary)], env=env, check=True)
            print('PASS fixed scalar import owning Math:', arch, label, flush=True)
        if not args.control and not args.caller_control:
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
            unit.write_text('#include "lib9.h"\n#include "interp.h"\n#include "native64-math-call.h"\nint main(void){return Math_bits64real64((const Native64MathBitsCall*)1)!=0 || Math_bits64realframe64((const Native64MathFrameCall*)1)!=0 || Math_bits64realguard64((const Native64MathFrameCall*)1)!=0 || Math_bits64realvm64((const Native64MathFrameCall*)1)!=0;}\n')
            binary = work/'disabled'
            subprocess.run(compiler+flags+includes+[str(unit), str(obj), '-Wl,--gc-sections', '-lm', '-o', str(binary)], env=env, check=True)
            subprocess.run(runner+[str(binary)], env=env, check=True)
            print('PASS fixed call: four incoherent selections and disabled pre-read refusal')
            if args.caller_guard:
                for selected, message in ((['-DDIS_NATIVE64_FIXED_MATH_CALL=1', '-DDIS_NATIVE64_CALLER_GUARD=0'], 'must be 1'),
                                          (['-DDIS_NATIVE64_FIXED_MATH_CALL=1', '-DDIS_NATIVE64_CALLER_GUARD=2'], 'must be 1'),
                                          (['-DDIS_NATIVE64_CALLER_GUARD=1'], 'requires the explicit fixed Math')):
                    result = subprocess.run(compiler+flags+explicit+selected+includes+['-fsyntax-only', str(ROOT/'libinterp/math.c')], env=env, capture_output=True, text=True)
                    if result.returncode == 0 or message not in result.stderr:
                        raise RuntimeError('Caller guard accepted an incoherent selection: '+result.stderr)
                print('PASS caller guard: three incoherent feature selections refused')


if __name__ == '__main__':
    main()
