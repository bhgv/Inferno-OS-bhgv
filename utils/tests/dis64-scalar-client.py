#!/usr/bin/env python3
"""Compile importing Limbo commands and libraries, then execute production file LOAD."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
NS=runpy.run_path(str(ROOT/'utils/tests/dis64-library-namespace.py'));L=NS['L'];S=NS['S']
def sections(data):
 result={}
 for i in range(struct.unpack_from('>I',data,28)[0]):
  kind,flags,at,size=struct.unpack_from('>HHII',data,32+12*i);assert flags==1 and kind not in result;result[kind]=data[at:at+size]
 assert set(result)=={1,2,3,5,7};return result
def client(fn,kind,args,values):
 declarations=[];names=[]
 for i,(k,v)in enumerate(zip(args,values)):
  n='a'+str(i);names.append(n);declarations.append(('seed'+str(i)+':='+str(int(v))+';'+n+':='+('big 'if k==2 else'real ')+'seed'+str(i)+';')if k in (2,3)else n+':='+str(int(v))+';')
 invoke='m->'+fn+'('+','.join(names)+');'
 return 'implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";'+''.join(declarations)+('first:='if kind else'')+invoke+'m=load Library "library.dis64";'+('last:='if kind else'')+invoke+'}'
def cases():
 for row in L['cases']():
  name,text,fn,kind,args,values,expected,failure=row
  yield row+(client(fn,kind,args,values),)
 text=L['source']('wide:fn(n:big):big;inc:fn(n:int):int;','wide(n:big):big{return n+n;}inc(n:int):int{return n+1;}')
 body='implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";first:=m->inc(7);seed:=3;wide:=big seed;twice:=m->wide(wide);m=load Library "library.dis64";last:=m->inc(first);kept:=twice;}'
 yield ('multiple-exports',text,'inc',1,[1],[7],9,0,body)
 text=L['source']('inc:fn(n:int):int;','inc(n:int):int{return n+1;}')
 for label,argument,expected in [('reload-chain','first',9),('continuation-word','first+3',12)]:
  body=client('inc',1,[1],[7]).replace('last:=m->inc(a0);','last:=m->inc('+argument+');')
  if label=='continuation-word':body=body.replace('last:=m->inc(first+3);','next:=first+3;last:=m->inc(next);after:=next+1;')[:-1]+'mask:=last^12;negated:=0-mask;nonzero:=mask|negated;shifted:=nonzero>>31;denom:=shifted+1;probe:=1/denom;}'
  yield (label,text,'inc',1,[1],[7],expected,0,body)

def unit():
 s=NS['unit'](iload=True);assert s.count('int main(int argc,char**argv)')==1;s=s.replace('int main(int argc,char**argv)','int oldiloadmain(int argc,char**argv)')
 extract=S['FILES']['FILES']['function'];f=extract(s,'int\nkopen(')
 pos=s.index('int\nkopen(');s=s[:pos]+'static uchar *clientfile,*providerfile;static unsigned int clientfilebytes,providerfilebytes;\n'+s[pos:]
 f2=f.replace('\n{','\n{\n if(!strcmp(path,"client.dis64")){filesource=clientfile;filebytes=clientfilebytes;}else if(!strcmp(path,"library.dis64")){filesource=providerfile;filebytes=providerfilebytes;}else return -1;',1);s=s.replace(f,f2,1)
 base=L['unit']();start=base.rfind(' optab[IMOVW]=countmovw;');end=base.index(' require(prepare64calls(',start);handlers=base[start:end]
 return s+'\n'+(ROOT/'utils/tests/dis64-scalar-client.c').read_text().replace(' CLIENT_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--materialize-only',action='store_true');p.add_argument('--case',action='append',choices=[r[0]for r in cases()]);a=p.parse_args();env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
 with tempfile.TemporaryDirectory(prefix='scalar-client-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);source=w/'test.c';source.write_text(unit());cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch=='arm64':cc+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
  elif arch=='riscv64':cc+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
  elif arch!='amd64':raise ValueError(arch)
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']+['-I'+str(x)for x in [w]+[ROOT/x for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]]
  runner=shlex.split(env.get('NATIVE64_RUNNER',''))
  rows=[r for r in cases()if not a.case or r[0]in a.case]
  for name,text,fn,kind,args,values,expected,failure,callertext in rows:
   b=w/'library.b';b.write_text(text);m=w/'library.m';m.write_text(NS['declarations'](text));provider=w/(name+'-library.dis64');caller=w/(name+'-client.dis64');c=w/'client.b';c.write_text(callertext)
   subprocess.run([str(a.limbo.resolve()),'-64b','-o',str(provider),str(b)],env=env,check=True,capture_output=True)
   r=subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(caller),str(c)],env=env,capture_output=True,text=True);assert not r.returncode,(name,r.stderr)
   parts=sections(caller.read_bytes());records=NS['bindings']({'imports':parts[5],'strings':parts[7]});assert records==([('Library','inc',1,(1,)),('Library','wide',2,(2,))]if name=='multiple-exports'else[('Library',fn,kind,tuple(args))])
   decoded=S['CALLS'].decode(parts[2]);assert [r[0]for r in decoded].count('ILOAD')==2 and [r[0]for r in decoded].count('IMCALL')==(3 if name=='multiple-exports'else 2)
   header=w/'library.h';header.write_bytes(subprocess.check_output([str(a.limbo.resolve()),'-H','-R','-M','Library',str(m)],env=env));binary=w/name
   subprocess.run(cc+['-DNAMESPACE_HEADER="library.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_'+fn,str(source),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)],env=env,check=True)
   r=subprocess.run(runner+[str(binary),str(caller),str(provider),str(kind),str(expected),str(failure)]+(['materialize']if a.materialize_only else[]),env=env,capture_output=True,text=True,timeout=120);assert not r.returncode,(name,r.returncode,r.stderr)
   print('PASS '+('file client construction/retirement:'if a.materialize_only else'file client -> ILOAD -> namespace library -> MCALL:'),arch,name,flush=True)
  print('PASS scalar importing client pipeline:',arch,len(rows),'source pairs',flush=True)
if __name__=='__main__':main()
