#!/usr/bin/env python3
"""Allocation-accounted forwarding-call IR boundary and predicate controls."""
import os
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
def main():
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory(dir=ROOT,prefix='tail-unit-') as td:
        work=Path(td);fixture=work/'fixture.c'
        base=(ROOT/'utils/tests/dis64-typed.c').read_text().replace('int main(void)','int typedbase(void)')
        tests=(ROOT/'utils/tests/dis64-typed-tail.c').read_text();fixture.write_text(base+'\n#line 1 "tail-fixture.c"\n'+tests)
        core=ROOT/'limbo/dis64-typed.c';binary=work/'unit'
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        command+=['-Dfree=typedfree','-ffunction-sections','-fdata-sections']+['-I'+str(ROOT/x)for x in ('Linux/386/include','include','limbo')]
        command+=[str(ROOT/'limbo/dis64-layout.c'),str(core),str(fixture),'-Wl,--gc-sections','-o',str(binary)]
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=30)
        print('PASS forwarding IR: exact target/descriptor/arguments/result, cycle refusal and complete allocation retirement',flush=True)
        production=core.read_text()
        for before,after,marker in (('callee->desc->id!=frame->s.offset','0','TAIL_DESCRIPTOR'),('binding->s.reg!=16','0','TAIL_BINDING'),('in->d.offset!=arg->offset','0','TAIL_ARGUMENT')):
            assert production.count(before)==1
            altered=work/(marker+'.c');altered.write_text(production.replace(before,after))
            subprocess.run([str(altered)if x==str(core)else x for x in command],check=True)
            result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30)
            line=next(i+1 for i,x in enumerate(tests.splitlines())if marker in x)
            assert result.returncode and 'Assertion' in result.stderr and f'tail-fixture.c:{line}:'in result.stderr,result.stderr
            assert 'runtime error:'not in result.stderr and 'AddressSanitizer'not in result.stderr,result.stderr
            print('PASS forwarding predicate control:',marker,flush=True)
if __name__=='__main__':main()
