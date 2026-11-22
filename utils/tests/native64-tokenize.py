#!/usr/bin/env python3
"""Execute an unchanged Sys.tokenize call through checked native64 C/JIT frames."""
import argparse, copy, os, runpy, shlex, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'))
CMD,E=G['CMD'],G['E']
SOURCE='''implement Tokens;include "sys.m";
Tokens:module{run:fn();open:fn();};
sys:Sys;text,seps:string;count:int;words:list of string;
run(){(n,l):=sys->tokenize(text,seps);count=n;words=l;}
open(){sys=load Sys Sys->PATH;}
'''
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='native64-tokenize-',dir=ROOT) as directory:
  w=Path(directory);limbo=a.limbo.resolve()
  CMD['generate'](limbo,w,'Command',False,argv=True,local=True,pair=True,raises=True)
  rows=G['generate'](limbo,w,SOURCE,'Tokens',(2,1,0,'~0U'),source_label='original Sys.tokenize call')
  header=w/'full.h';header.write_text(header.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
  data,proofs=E['compile_source'](limbo,w,SOURCE,interface='',loads=True)
  imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
  for k in range(6):
   damaged=copy.deepcopy(proofs[0]);damaged['code'][damaged['externals'][0]['end']-1][k]+=1
   try:CMD['verify_nilbranches'](data,damaged,imports,rows=proofs)
   except (AssertionError,KeyError,IndexError,ValueError):pass
   else:raise AssertionError(('missed tuple binding corruption',k))
  # Imported storage selection depends on types, never on Sys/tokenize names.
  for scalar in ('int','big','real'):
   for root in ('string','list of string','array of byte','ref P->R','chan of int','P'):
    interface='P:module{R:adt{v:int;}; pair:fn():('+scalar+','+root.replace('P->','')+');};'
    source='implement Client;include "provider.m";Client:module{run:fn();open:fn();};p:P;out:'+root+';number:'+scalar+';run(){(n,v):=p->pair();number=n;out=v;}open(){p=load P "provider";}'
    old,cases=E['compile_source'](limbo,w,source,interface=interface,loads=True)
    assert all(r['accepted']for r in cases),[(r['name'],r.get('reason'))for r in cases]
    imported=E['BASE']['READ']['legacy_instructions'](old,with_imports=True)[-1]
    for r in cases:E['verify'](old,r,imported,loads=True,rows=cases)
    assert cases[0]['externals'][0]['contract']==16
  print('PASS imported scalar/root results: 18 declarations across six managed root kinds',flush=True)
  frontier=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](limbo,'appl/cmd/sh/sh.b',w)
  assert next(f for f in frontier['functions']if f['name']=='getbq')['accepted']
  print('PASS unchanged system Sh.getbq: read and tokenize imported calls accepted',flush=True)
  offsets={x[0]:x[1]for x in rows[0]['globals']}
  with header.open('a')as out:out.write('static const uint token_offsets[]={'+','.join(str(offsets[n])for n in ('sys','text','seps','count','words'))+'};\n')
  base=CMD['unit']().replace('int main(int argc,char **argv)','int old_command_main(int argc,char **argv)')
  extract=CMD['NS']['S']['FILES']['FILES']['function']
  runt=(ROOT/'libinterp/runt.c').read_text();string=(ROOT/'libinterp/string.c').read_text()
  start=runt.index('typedef struct TokenizeResult TokenizeResult;');end=runt.index('/* Keep partial words',start)
  base+='\n#include "sys-native64.h"\n'+extract(string,'String*\nslicer(')+'\n'+extract(runt,'static int\ntokdelim(')+'\n'+runt[start:end]+'\n'+extract(runt,'static int\nbuildtokens(')+'\n'+extract(runt,'void\nSys_tokenize(')
  mainat=base.index('int old_command_main(')
  setup=base[base.index(' optab[ILOAD]=actual_iload;',mainat):base.index('\n if(command_local)optab[ICALL]',mainat)]
  base+='\n'+(ROOT/'utils/tests/native64-tokenize.c').read_text().replace('TOKEN_OPTAB',setup)
  unit=w/'unit.c';unit.write_text(base)
  arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(unit),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True)
  subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
if __name__=='__main__':main()
