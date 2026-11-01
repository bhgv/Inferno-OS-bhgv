#!/usr/bin/env python3
"""Cross-build and run actual native64 Dis64 scalar, scheduler and vmachine source closures."""
from pathlib import Path
import argparse
import os
import re
import resource
import runpy
import shlex
import signal
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]


# Cross-execute real VM lifecycle bodies; process/pool/native boundaries remain the existing explicit fixtures.
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=('arm64', 'riscv64'), default='arm64')
    parser.add_argument('--cc', required=True, help='Hosted target compiler command, including static link options if needed')
    parser.add_argument('--system', choices=('Linux','Android'), default='Linux', help='Use headers matching the selected hosted C library')
    parser.add_argument('--runner', required=True, help='Target process runner command')
    args=parser.parse_args()
    if args.arch == 'riscv64' and args.system != 'Linux':
        parser.error('The RV64 runtime gate currently requires Linux headers')
    label = args.arch.upper()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    runtime=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    scheduler=runpy.run_path(str(ROOT/'libinterp/tests/dis64-scheduler.py'))
    machine=runpy.run_path(str(ROOT/'libinterp/tests/dis64-vmachine.py'))
    files=runpy.run_path(str(ROOT/'libinterp/tests/dis64-fileloader.py'))
    default_flags = os.environ.get('NATIVE64_CFLAGS', '-O2 -g -DNDEBUG -fno-strict-aliasing')
    compatibility_flags = os.environ.get('ARM64_RUNTIME_CFLAGS', default_flags)
    flags = shlex.split(os.environ.get('NATIVE64_RUNTIME_CFLAGS', compatibility_flags))
    command=shlex.split(args.cc)+flags+['-DDIS64_TEST_'+label+'=1','-DDIS64_TEST_COMPILER="comp-'+args.arch+'.c"']
    command+=['-I'+str(ROOT/p) for p in (args.system+'/'+args.arch+'/include',args.system+'/386/include','emu/port','include','libinterp')]
    runner=shlex.split(args.runner)
    with tempfile.TemporaryDirectory(prefix=args.arch+'-runtime-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work=Path(directory)
        env=dict(os.environ,TMPDIR=str(work))
        # Compile each complete source closure for the selected target C library.
        def build(name, source, options):
            if args.system == 'Android' and 'getcallerpc(void *argument)' in source:
                source=source.replace(runtime['function'](source,'uintptr\ngetcallerpc('),'',1)
                source=source.replace('extern void __real_free(void*);', 'extern void __real_free(void*) __asm__("free");')
                source=source.replace('void\n__wrap_free(', 'void\nfree_(', 1)
            unit=work/(name+'.c'); unit.write_text(source)
            binary=work/name
            subprocess.run(command+options+[str(unit),str(ROOT/'libinterp/dec.c')]+([] if args.system == 'Android' else ['-Wl,--wrap=free'])+['-lm','-o',str(binary)],env=env,check=True)
            return binary


        # Execute target code through the explicitly selected runner.
        def run(binary, *arguments, **options):
            return subprocess.run(runner+[str(binary)]+list(map(str,arguments)),env=env,timeout=90,**options)
        scalar=build('scalar',runtime['buildunit'](),['-DDIS_WORD32=1'])
        cases=runtime['runtimecases']()
        predicates=0
        for index,(payload,width,expected) in enumerate(cases):
            image=work/('scalar-'+str(index)+'.dis64'); image.write_bytes(payload)
            result=run(scalar,image,width,format(expected,'x'),capture_output=True,text=True)
            if result.returncode != 0:
                raise RuntimeError(label+' scalar case '+str(index)+': '+result.stdout+result.stderr)
            if index and index%128 == 0: print(label+' scalar progress:',index,'images',flush=True)
            predicates+=int(re.search(r': (\d+) predicates',result.stdout).group(1))
        print('PASS '+label+' scalar runtime:',len(cases),'whole images,',predicates,'actual C/RX predicates',flush=True)
        legacy=build('legacy',runtime['buildunit'](),[])
        image=work/'legacy.dis64'; image.write_bytes(cases[0][0])
        run(legacy,'--unsupported',image,check=True)
        fixtures=[]
        for name in ('pilot42','pilot-init42','divide-zero','shift-invalid'):
            image=work/(name+'.dis64')
            if name in ('divide-zero','shift-invalid'):
                operation='43100000' if name=='divide-zero' else '4e102000'
                image.write_bytes(runtime['image']([bytes.fromhex(operation),bytes.fromhex('0f1b')]))
            else:
                image.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64'/(name+'.hex')).read_text()))
            fixtures.append((image,1 if name=='divide-zero' else 2 if name=='shift-invalid' else 0))
        for name,unit,options in (
                ('scheduler',scheduler['buildunit'](),[]),
                ('file',files['buildunit'](),['-DDIS64_PILOT_LOADER=1']),
                ('vmachine',machine['buildunit'](),[]),
                ('vmachine-file',machine['buildunit'](True),['-DDIS64_PILOT_LOADER=1'])):
            binary=build(name,unit,['-DDIS_WORD32=1']+options)
            for image,mode in fixtures:
                print('ARM64',name,image.stem,flush=True)
                run(binary,image,mode,check=True)
        controlimage=fixtures[2][0]
        for name,signature,replacement in (
                ('no-propagation','static void\npropex(', 'static void\npropex(Prog *p, char *text) { (void)p; (void)text; }'),
                ('no-selection','int\nactualhandler(', 'int\nactualhandler(char *text) { (void)text; return 0; }')):
            unit=machine['buildunit']()
            unit=unit.replace(machine['function'](unit,signature),replacement,1)
            binary=build(name,unit,['-DDIS_WORD32=1'])
            result=run(binary,controlimage,1,capture_output=True,text=True)
            if result.returncode not in (-signal.SIGABRT,128+signal.SIGABRT) or 'Dis64 runtime check failed' not in result.stderr:
                raise RuntimeError(label+' lifecycle control missed its predicate: '+name+'\n'+result.stdout+result.stderr)
            print('PASS '+label+' lifecycle control:',name,flush=True)


if __name__=='__main__':
    main()
