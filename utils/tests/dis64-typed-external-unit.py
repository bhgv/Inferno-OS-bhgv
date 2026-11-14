#!/usr/bin/env python3
"""Allocation-accounted imported-call IR and active predicate controls."""
import os
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
def main():
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory(dir=ROOT,prefix='multi-unit-')as td:
        work=Path(td);fixture=work/'fixture.c';core=work/'core.c';binary=work/'unit'
        base=(ROOT/'utils/tests/dis64-typed.c').read_text().replace('int main(void)','int typedbase(void)')
        tests=(ROOT/'utils/tests/dis64-typed-external.c').read_text();fixture.write_text(base+'\n#line 1 "external-fixture.c"\n'+tests)
        core.write_text((ROOT/'limbo/dis64-typed.c').read_text())
        (work/'dis64-multi.h').write_text((ROOT/'limbo/dis64-multi.h').read_text())
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        command+=['-Dfree=typedfree','-ffunction-sections','-fdata-sections']+['-I'+str(ROOT/x)for x in ('Linux/386/include','include','limbo')]
        command+=[str(ROOT/'limbo/dis64-layout.c'),str(core),str(fixture),'-Wl,--gc-sections','-o',str(binary)]
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''));subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=30)
        print('PASS imported-call IR: exact module/import identity, fixed arguments, setup isolation, inherited result, legacy-profile refusal and allocation retirement',flush=True)
        production=(ROOT/'limbo/dis64-external.h').read_text()
        for before,after,marker in (('call->d.reg!=frame->s.reg','0','EXTERNAL_MODULE'),('in->d.offset!=arg->offset','0','EXTERNAL_ARGUMENT'),('frame->m.decl->iface->offset>32767','0','EXTERNAL_ORDINAL'),('inherit?32:0','0','EXTERNAL_INHERITED')):
            assert production.count(before)==1
            (work/'dis64-external.h').write_text(production.replace(before,after));subprocess.run(command,check=True)
            result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30)
            line=next(i+1 for i,x in enumerate(tests.splitlines())if marker in x)
            assert result.returncode and 'Assertion'in result.stderr and f'external-fixture.c:{line}:'in result.stderr,result.stderr
            assert 'runtime error:'not in result.stderr and 'AddressSanitizer'not in result.stderr,result.stderr
            print('PASS imported-call compiler predicate control:',marker,flush=True)
if __name__=='__main__':main()
