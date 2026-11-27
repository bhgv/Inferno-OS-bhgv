#!/usr/bin/env python3
"""Run source recipe call sites through the complete native VM and leaf JIT.

Formatting producers are bounded here; the formatter gate separately runs
these recipes with the real formatter, heap and collector.
"""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='sys-program-vm-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True)
  runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))['generate'](a.limbo.resolve(),w,dict(os.environ))
  (w/'sys-native64.h').write_text(subprocess.check_output([str(a.limbo.resolve()),'-H','-M','Sys',str(ROOT/'module/sys.m')],text=True))
  src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
  src+='\n#define LIMBO_NATIVE64_PREFIXES\n#include "sys-native64.h"\n#include "native64-sys-frame.h"\n#include "native64-sys-sites.h"\n#include "native64-sys-program.h"\n'
  extract=CMD['NS']['S']['FILES']['FILES']['function'];runt=(ROOT/'libinterp/runt.c').read_text()
  src+='\n'+extract((ROOT/'emu/port/errstr.c').read_text(),'void\nkgerrstr(')+'\n'
  for sig in ('int\nSys_setprogram64(','void\nSys_programcall64(','int\nSys_clearframes64('):src+='\n'+extract(runt,sig)+'\n'
  start=src.index(' optab[IMOVW]=countmovw;');stop=src.index(' require(prepare64calls(',start)
  src+='\n#line 1 "native64-sys-program-vm.c"\n'+(ROOT/'utils/tests/native64-sys-program-vm.c').read_text().replace(' PROGRAM_HANDLERS',src[start:stop])
  f=w/'unit.c';f.write_text(src);arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  elif arch!='amd64':p.error(arch)
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(f),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(cc+['-DDIS_NATIVE64_SCALAR_BOOT=1','-DDIS_NATIVE64_BOOT_SYS_IO=1','-DDIS_NATIVE64_FORMATTER=1','-c',str(ROOT/'libinterp/runt.c'),'-o',str(w/'runt.o')],check=True)
  subprocess.run(command,check=True);runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
  subprocess.run(runner+[str(binary)],check=True,timeout=180)
  header=(ROOT/'include/native64-sys-program.h').read_text();before=' || !native64sysprogramtarget(site)';assert header.count(before)==1
  (w/'native64-sys-program.h').write_text(header.replace(before,''));subprocess.run(command,check=True)
  r=subprocess.run(runner+[str(binary),'target'],capture_output=True,text=True,timeout=180)
  assert r.returncode==10 and 'program VM predicate: foreign native entry reached'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
  print('PASS Sys program active control: foreign native target rejected before dispatch',flush=True)
if __name__=='__main__':main()
