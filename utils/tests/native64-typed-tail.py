#!/usr/bin/env python3
"""Run compiler-lowered Limbo return bodies through real FRAME/CALL/RET and JIT."""
import argparse
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);args=p.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));env=dict(os.environ)
    arch=env.get('NATIVE64_ARCH','amd64');runner=shlex.split(env.get('NATIVE64_RUNNER',''))
    with tempfile.TemporaryDirectory(prefix='typed-calls-',dir=ROOT) as td:
        work=Path(td);env['TMPDIR']=str(work)
        runpy.run_path(str(ROOT/'utils/tests/dis64-typed-tail.py'))['generate'](args.limbo.resolve(),work)
        source=runpy.run_path(str(ROOT/'utils/tests/native64-local-calls.py'))['unit']()
        source=source[:source.index('/* Keep scalar file admission separate')]
        extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
        source+='\n'+extract((ROOT/'libinterp/xec.c').read_text(),'OP(movb)')+'\n'
        source+='\n#line 1 "native64-typed-tail.c"\n'+(ROOT/'utils/tests/native64-typed-tail.c').read_text()
        file=work/'typed.c';file.write_text(source)
        command=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        if arch=='arm64':command+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch=='riscv64':command+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
        elif arch!='amd64':raise RuntimeError(arch)
        command+=['-DDIS_WORD32=1']+['-I'+str(ROOT/x) for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'typed'
        command+=['-I'+str(work),str(file),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)]
        subprocess.run(command,check=True,env=env)
        subprocess.run(runner+[str(binary)],check=True,env=env,timeout=120)
        production=(ROOT/'include/native64-typed-code.h').read_text()
        for before,case in (('(unsigned long)s->d>=(unsigned int)callee->size',1),('s->s!=32',2)):
            assert production.count(before)==1
            (work/'native64-typed-code.h').write_text(production.replace(before,'0'))
            subprocess.run(command,check=True,env=env)
            result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,env=env,timeout=120)
            assert result.returncode==10 and f'forwarding native refusal removed: case={case}' in result.stderr,result.stderr
            assert 'runtime error:'not in result.stderr and 'Sanitizer'not in result.stderr,result.stderr
            print('PASS forwarding native predicate control:',arch,'case='+str(case),flush=True)
if __name__=='__main__':main()
