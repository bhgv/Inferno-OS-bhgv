#!/usr/bin/env python3
"""Compile owning scalar-field witnesses and execute real Math production for host or ARM64."""
from pathlib import Path
import argparse
import os
import resource
import shlex
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Generate metadata with the actual compiler and keep every contract-removal control semantic.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo',type=Path,required=True)
    parser.add_argument('--control',choices=('kind','owner','generation','field'))
    args=parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
    compiler=shlex.split(env.get('CC','cc'))
    flags=shlex.split(env.get('NATIVE64_CFLAGS','-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner=shlex.split(env.get('NATIVE64_RUNNER',''))
    with tempfile.TemporaryDirectory(prefix='native64-field-',dir=env.get('NATIVE64_TMPDIR',ROOT)) as directory:
        work=Path(directory);env['TMPDIR']=str(work)
        (work/'probe.m').write_text('Probe: module { caller: fn(tag: byte, pad: int, integer: big, value: real, keep: array of byte, aggregate: (int, real), tail: real, trailing: byte); other: fn(value: real); };')
        for name,path in [('probe',work/'probe.m'),('math',ROOT/'module/math.m')]:
            (work/(name+'-native64.h')).write_bytes(subprocess.check_output([str(args.limbo.resolve()),'-H',str(path)]))
        # Bounded metadata and empty/variadic declarations are compiler contracts, not runtime guesses.
        empty=work/'empty.m'
        empty.write_text('Empty: module {zero: fn(); variadic: fn(format: string, *): int;};')
        generated=subprocess.check_output([str(args.limbo.resolve()),'-H',str(empty)],text=True)
        if 'limbo_native64_Empty_zero_field_table' not in generated or 'limbo_native64_Empty_variadic_field_table' in generated:
            raise RuntimeError('Empty/variadic compiler witness contract changed')
        excessive=work/'excessive.m'
        excessive.write_text('Excessive: module {f: fn('+','.join('p'+str(i)+': byte' for i in range(4097))+');};')
        rejected=subprocess.run([str(args.limbo.resolve()),'-H',str(excessive)],capture_output=True,text=True)
        if rejected.returncode==0 or rejected.stdout or 'too many native64 fixed arguments' not in rejected.stderr:
            raise RuntimeError('Compiler field limit was not refused before output: '+rejected.stderr)
        if args.control:
            helper=(ROOT/'include/native64-field.h').read_text()
            changes={
                'kind':('table->fields[ordinal].kind != kind','0'),
                'owner':('registers.M->m != module','0'),
                'generation':('!native64callerready(&witness->caller)','witness->caller.self != &witness->caller'),
                'field':('field->kind != witness->field.kind','0'),
            }
            if args.control == 'generation':
                (work/'native64-field.h').write_text(helper)
                helper=(ROOT/'include/native64-caller.h').read_text()
                old,new='prog->native64epoch == caller->epoch && ',''
                destination=work/'native64-caller.h'
            else:
                old,new=changes[args.control]
                destination=work/'native64-field.h'
            if helper.count(old)!=1:raise RuntimeError('Witness control boundary changed')
            destination.write_text(helper.replace(old,new,1))
        includes=['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        for label,model in [('WORD32',['-DDIS_WORD32=1']),('WORD32-file',['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1'])]:
            command=compiler+flags+model+includes+['-ffunction-sections','-fdata-sections','-DDIS_NATIVE64_FIXED_MATH_CALL=1','-DDIS_NATIVE64_CALLER_GUARD=1']
            # Keep the owning Math implementation; unrelated entries are discarded at linkage.
            obj=work/'math.o'
            subprocess.run(compiler+flags+model+includes+['-ffunction-sections','-fdata-sections','-DDIS_NATIVE64_MATH_HEADER="math-native64.h"','-c',str(ROOT/'libinterp/math.c'),'-o',str(obj)],env=env,check=True)
            binary=work/label
            subprocess.run(command+[str(ROOT/'utils/tests/native64-field.c'),str(obj),'-Wl,--gc-sections','-lm','-o',str(binary)],env=env,check=True)
            result=subprocess.run(runner+[str(binary)],env=env,capture_output=True,text=True,timeout=30)
            if args.control:
                if result.returncode!=1 or 'field witness predicate failed' not in result.stderr:
                    raise RuntimeError('Witness mutation missed its semantic oracle: '+args.control+'\n'+result.stdout+result.stderr)
                print('PASS field witness control:',args.control,arch,label,flush=True)
            elif result.returncode:raise RuntimeError(result.stdout+result.stderr)
            else:print(result.stdout,end='',flush=True)


if __name__=='__main__':
    main()
