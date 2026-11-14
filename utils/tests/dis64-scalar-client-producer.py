#!/usr/bin/env python3
"""Source boundaries and classic-byte compatibility for the opt-in importing writer."""
import argparse,os,runpy,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--baseline',type=Path);a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='client-producer-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);source=w/'client.b';image=w/'client.dis64';interface=w/'library.m';source.write_text(P['client']('id',1,[1],[7]));interface.write_text('Library:module{id:fn(n:int):int;};')
  command=[str(a.limbo.resolve()),'-I',str(w)];subprocess.run(command+['-64c','-o',str(image),str(source)],env=env,check=True);parts=P['sections'](image.read_bytes());assert P['NS']['bindings']({'imports':parts[5],'strings':parts[7]})==[('Library','id',1,(1,))]
  first=image.read_bytes();subprocess.run(command+['-64c','-i','-o',str(image),str(source)],env=env,check=True);assert first==image.read_bytes()
  for row in P['cases']():
   name,text,fn,kind,args,values,expected,failure,body=row;interface.write_text(P['NS']['declarations'](text));source.write_text(body)
   ordinary=w/'ordinary.dis';subprocess.run(command+['-i','-o',str(ordinary),str(source)],env=env,check=True,capture_output=True)
   if a.baseline:
    old=w/'old.dis';subprocess.run([str(a.baseline.resolve()),'-I',str(w),'-i','-o',str(old),str(source)],env=env,check=True,capture_output=True);assert old.read_bytes()==ordinary.read_bytes(),name
  interface.write_text('Library:module{id:fn(n:int):int;};');base=P['client']('id',1,[1],[7]);negative=[
   base.replace('"library.dis64"','"$Sys"'),base.replace('"library.dis64"','""'),base.replace('"library.dis64"','"'+('p'*1024)+'"'),base.replace('"library.dis64"','"λ.dis64"'),
   base.replace('m=load Library "library.dis64";','m=load Library "other.dis64";'),base.replace('first:=m->id(a0);','first:=m->id(a0+3);'),
   base.replace('init:fn();','init:fn(n:int);').replace('init(){','init(n:int){'),
   base.replace('Client:module{init:fn();};','Client:module{init:fn();helper:fn();};')+'helper(){}',
   base.replace('};init(){','};g:int;init(){').replace('a0:=7;','a0:=7;g=a0;'),
   base.replace('last:=m->id(a0);','last:=m->id(a0);s:="another constant";a0=len s;')]
  for i,body in enumerate(negative):
   source.write_text(body);image.unlink(missing_ok=True);r=subprocess.run(command+['-64c','-o',str(image),str(source)],env=env,capture_output=True,text=True)
   assert r.returncode and ('Dis64 client:'in r.stderr or'Dis64 calls:'in r.stderr)and(not image.exists()or not image.stat().st_size),(i,r.returncode,r.stderr)
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  interface.write_text('Library:module{f:fn('+','.join('a'+str(i)+':int'for i in range(64))+'):int;};');source.write_text('implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "'+('p'*1023)+'";r:=m->f('+','.join(str(i)for i in range(64))+');}')
  subprocess.run(command+['-64c','-o',str(image),str(source)],env=env,check=True,capture_output=True);parts=P['sections'](image.read_bytes());rows=P['NS']['bindings']({'imports':parts[5],'strings':parts[7]});assert rows==[('Library','f',1,(1,)*64)]
  print('PASS importing writer: 17 classic sources checked'+(', prior compiler bytes matched'if a.baseline else'')+', deterministic file, ten source refusals,64 parameters and1023-byte path',flush=True)
if __name__=='__main__':main()
