#!/usr/bin/env python3
"""Checked fstat result geometry and private caller-frame repacking controls."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    base=(ROOT/'utils/tests/dis64-typed.c').read_text().replace('int main(void)','int old_stat_main(void)')
    core=(ROOT/'limbo/dis64-typed.c').read_text().replace('#include \"limbo.h\"','',1)
    # One translation unit exposes the private proof helper to forged-IR controls.
    source=base+'\n#define free typedfree\n'+core+'\n'+(ROOT/'utils/tests/dis64-stat-result.c').read_text()
    cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    cc+=['-ffunction-sections','-fdata-sections']+['-I'+str(ROOT/p)for p in('Linux/386/include','include','limbo')]
    runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
    with tempfile.TemporaryDirectory(prefix='stat-ir-',dir=ROOT)as td:
        w=Path(td);f=w/'unit.c';f.write_text(source);binary=w/'test'
        allocator=w/'allocator.h';allocator.write_text('#include "lib9.h"\n#undef calloc\n#undef free\n#define free typedfree\nvoid typedfree(void*);\n')
        cmd=cc+['-include',str(allocator),str(ROOT/'limbo/dis64-layout.c'),str(f),'-Wl,--gc-sections','-o',str(binary)]
        subprocess.run(cmd,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=30)
        header=(ROOT/'limbo/dis64-external.h').read_text();before='if(relocate){';assert header.count(before)==1
        (w/'dis64-multi.h').write_text((ROOT/'limbo/dis64-multi.h').read_text());(w/'dis64-external.h').write_text(header.replace(before,'if(0){',1))
        subprocess.run(cmd,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30)
        assert r.returncode==10 and 'fstat IR predicate: repacked result group'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
        print('PASS fstat IR active control: aggregate repacking required',flush=True)
if __name__=='__main__':main()
