#!/usr/bin/env python3
"""Allocation-accounted compiler IR checks for private source variadics."""
import os,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
 base=(ROOT/'utils/tests/dis64-typed.c').read_text().replace('int main(void)','int base_main(void)')
 cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
 cc+=['-ffunction-sections','-fdata-sections']+['-I'+str(ROOT/p)for p in ('Linux/386/include','include','limbo')]
 runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
 with tempfile.TemporaryDirectory(prefix='variadic-ir-',dir=ROOT)as td:
  w=Path(td);(w/'dis64-multi.h').write_text((ROOT/'limbo/dis64-multi.h').read_text());(w/'unit.c').write_text(base+'\n'+(ROOT/'utils/tests/dis64-variadic.c').read_text());(w/'core.c').write_text((ROOT/'limbo/dis64-typed.c').read_text())
  (w/'allocator.h').write_text('#include "lib9.h"\n#undef calloc\n#undef free\n#define free typedfree\nvoid typedfree(void*);\n')
  binary=w/'unit';cmd=cc+['-include',str(w/'allocator.h'),str(ROOT/'limbo/dis64-layout.c'),str(w/'core.c'),str(w/'unit.c'),'-Wl,--gc-sections','-o',str(binary)]
  subprocess.run(cmd,check=True);subprocess.run(runner+[str(binary)],check=True)
  for header,before,after,why in [('dis64-variadic.h','(unsigned int)sign(fn)==0x4c0624b6U','1','signature refused'),('core.c','p->sourceroles && reference','reference','temporary refinement LOAD-only')]:
   target=w/header;source=(ROOT/'limbo/dis64-typed.c').read_text()if header=='core.c'else(ROOT/'limbo'/header).read_text();assert source.count(before)==1;target.write_text(source.replace(before,after,1))
   subprocess.run(cmd,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True)
   assert r.returncode==10 and 'variadic IR: '+why in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
   if header!='core.c':target.unlink()
   print('PASS variadic IR active control:',why,flush=True)
if __name__=='__main__':main()
