#!/usr/bin/env python3
"""Checked computed byte argument proof."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    base=(ROOT/'utils/tests/dis64-typed.c').read_text().replace('int main(void)','int original_self_main(void)')
    production=(ROOT/'limbo/dis64-typed.c').read_text()
    cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    cc+=['-ffunction-sections','-fdata-sections']+['-I'+str(ROOT/p)for p in('Linux/386/include','include','limbo')]
    runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
    with tempfile.TemporaryDirectory(prefix='typed-self-unit-',dir=ROOT)as td:
        w=Path(td);source=w/'test.c';core=w/'core.c';binary=w/'test'
        source.write_text(base+'\n'+(ROOT/'utils/tests/dis64-computed-args.c').read_text());core.write_text(production)
        allocator=w/'allocator.h';allocator.write_text('#include \"lib9.h\"\n#undef calloc\n#undef free\n#define free typedfree\nvoid typedfree(void*);\n')
        command=cc+['-include',str(allocator),str(ROOT/'limbo/dis64-layout.c'),str(core),str(source),'-Wl,--gc-sections','-o',str(binary)]
        subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=30)
        multi=(ROOT/'limbo/dis64-multi.h').read_text();old='t->tof->kind==Tbyte';assert multi.count(old)==1
        (w/'dis64-multi.h').write_text(multi.replace(old,'1',1));subprocess.run(command,check=True)
        r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30)
        assert r.returncode==10 and 'computed argument IR: wrong array element refused'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
        print('PASS computed argument active control: byte element class required',flush=True)
if __name__=='__main__':main()
