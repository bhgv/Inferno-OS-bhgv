#!/usr/bin/env python3
"""Check writer publication/rollback and active codec/semantic omissions."""
import os,resource,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory(prefix='call-writer-unit-',dir=ROOT)as td:
        w=Path(td);base=(ROOT/'utils/tests/dis64-typed.c').read_text().replace('int main(void)','int typedbase(void)');tests=(ROOT/'utils/tests/dis64-source-calls.c').read_text();fixture=w/'fixture.c';fixture.write_text(base+'\n#line 1 "writer-fixture.c"\n'+tests)
        production=(ROOT/'limbo/dis64-calls.c').read_text();writer=w/'writer.c';writer.write_text(production);binary=w/'writer'
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        command+=['-Dfree=typedfree','-ffunction-sections','-fdata-sections']+['-I'+str(ROOT/x)for x in ('Linux/386/include','include','limbo')]+[str(ROOT/'limbo/dis64-layout.c'),str(ROOT/'limbo/dis64-typed.c'),str(writer),str(fixture),'-Wl,--gc-sections','-o',str(binary)]
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''));subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=30)
        print('PASS scalar call writer: complete bytes, raw uninitialized-source refusal, no partial output and allocation retirement',flush=True)
        old='if(!dis64callinfercheck(image,used,CallFileObject,&proof->inference,proof->decoded,proof->scratch,proof->queue,CallFileCode,\n       Dis64CallLimit,proof->records,Dis64LocalFunctions,&records,&proof->image) || records!=(unsigned int)nf)'
        for before,after,marker in ((old,'if(0)','WRITER_PROOF'),('if(in.op==ILEA)in.source=tail;','if(0)in.source=tail;','WRITER_POSITIVE'),('free(b[i].pcs);',';','WRITER_OWNERS')):
            assert production.count(before)==1
            writer.write_text(production.replace(before,after));subprocess.run(command,check=True)
            r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30);line=next(i+1 for i,x in enumerate(tests.splitlines())if marker in x)
            assert r.returncode and 'Assertion'in r.stderr and f'writer-fixture.c:{line}:'in r.stderr,r.stderr
            assert 'runtime error:'not in r.stderr and 'Sanitizer'not in r.stderr,r.stderr
            print('PASS scalar call writer predicate control:',marker,flush=True)
if __name__=='__main__':main()
