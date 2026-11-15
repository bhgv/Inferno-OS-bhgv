#!/usr/bin/env python3
"""Extra literals with ordinary scalar libraries and both native result binders."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--baseline',type=Path);a=p.parse_args();env=dict(os.environ);env["ROOT"]=str(ROOT);env.pop("EMU",None);arch=env.get('NATIVE64_ARCH','amd64')
 cases=[('unicode','s:="Привет";n:=len s;',7),('source-mp','i:=1;n:="a🚀"[i];',128641),('compare','s:="é";n:=0;if(s>"a")n=4;else n=9;',5),('nul','s:="a\\0z";i:=1;n:=s[i];',1)]
 cases += [('concat','a:="one";b:="two";s:=a+b;n:=len s;',7),('append','s:="a";s+="b";n:=len s;',3),('store','s:="abc";alias:=s;s[1]=90;if(alias!="abc"){z:=0;bad:=1/z;}n:=s[1];',91),('promote','s:="abc";s[1]=128640;n:=s[1];',128641),('grow','s:string;s[0]=65;n:=s[0];',66),('slice','s:="a🚀z";s=s[1:2];n:=s[0];',128641),('slice-empty','s:="abc";s=s[0:0];n:=len s;',1),('self','s:="ab";s+=s;n:=len s;',5)]
 cases += [('parse-word','s:="42";n:=int s;',43),('parse-big','s:="4294967296";b:=big s;b>>=32;n:=int b;',2),('parse-real','s:="1.25e2";r:=real s;n:=int r;',126),('format-word','v:=-123;s:=string v;n:=len s;',5),('format-big','v:=7;b:=big v;b<<=40;s:=string b;n:=len s;',14),('format-real','v:=5;d:=4;r:=real v;den:=real d;r/=den;s:=string r;n:=len s;',5),('bytes','s:="a🚀z";a:=array of byte s;s=string a;n:=len s;',4),('bytes-nul','s:="a\\0z";a:=array of byte s;t:=string a;n:=len a;if(t!=s){z:=0;bad:=1/z;}',4)]
 cases += [('array-new','n:=3;a:=array[n]of byte;n=len a;',4),('array-zero','a:=array[2]of byte;s:=string a;n:=s[1];',1),('array-slice','s:="abcd";a:=array of byte s;b:=a[1:3];a=nil;t:=string b;n:=len t;',3),('array-nested','s:="abcd";a:=array of byte s;b:=a[1:];c:=b[1:2];a=nil;b=nil;t:=string c;n:=t[0];',100),('array-alias','a:=array[4]of byte;b:=a[1:3];n:=len a;n+=len b;',7),('array-empty','a:=array[0]of byte;n:=len a;',1),('array-inplace','s:="abcd";a:=array of byte s;a=a[2:];t:=string a;n:=len t;',3),('array-new-view','a:=array[5]of byte;b:=a[2:4];a=nil;n:=len b;',3)]
 with tempfile.TemporaryDirectory(prefix='literal-scalar-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'library.m').write_text('Library:module{inc:fn(n:int):int;};');(w/'library.b').write_text('implement Library;include "library.m";inc(n:int):int{return n+1;}')
  subprocess.run([str(a.limbo.resolve()),'-H','-R','-M','Library',str(w/'library.m')],env=env,check=True,stdout=(w/'library.h').open('w'))
  text=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-gc.py'))['instrument'](P['unit'](codecs=True));(w/'test.c').write_text(text)
  cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch=='arm64':cc+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
  elif arch!='amd64':raise ValueError(arch)
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="library.h"','-DNAMESPACE_ORDINAL=0']+['-I'+str(x)for x in [w]+[ROOT/x for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]]
  binary=w/'test';subprocess.run(cc+[str(w/'test.c'),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)],env=env,check=True)
  for name,body,expected in cases:
   source='implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";if(m==nil)return;'+body+'last:=m->inc(n);if(last!='+str(expected)+'){z:=0;bad:=1/z;}}';(w/'client.b').write_text(source)
   for file,flag in [('library','-64b'),('client','-64c')]:
    r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(w),'-o',str(w/(file+'.dis64')),str(w/(file+'.b'))],env=env,capture_output=True,text=True);assert not r.returncode,(name,r.stderr)
   if a.baseline:
    for compiler,file in [(a.limbo,'plain.dis'),(a.baseline,'old.dis')]:subprocess.run([str(compiler.resolve()),'-i','-I',str(w),'-o',str(w/file),str(w/'client.b')],env=env,check=True,capture_output=True)
    assert (w/'plain.dis').read_bytes()==(w/'old.dis').read_bytes(),name
   r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'client.dis64'),str(w/'library.dis64'),'1',str(expected),'0','guarded'],env=env,capture_output=True,text=True,timeout=120);assert not r.returncode,(name,r.returncode,r.stderr)
   print('PASS literal scalar result:',arch,name,flush=True)
 print('PASS literal scalar pipeline:',arch,str(len(cases))+' pairs /'+str(len(cases)*96)+' scheduler roots; actual GC',flush=True)
if __name__=='__main__':main()
