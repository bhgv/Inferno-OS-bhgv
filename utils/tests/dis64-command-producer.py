#!/usr/bin/env python3
"""Ordinary-init determinism, classic compatibility and producer refusal boundaries."""
import argparse,os,re,runpy,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'))
C=runpy.run_path(str(ROOT/'utils/tests/dis64-command-cases.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--baseline',type=Path);p.add_argument('--command-baseline',type=Path);a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='command-producer-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);source=w/'source.b';image=w/'new.dis64';old=w/'old.dis64';plain=w/'plain.dis';oldplain=w/'old.dis'
  def compile(compiler,flag,target,check=True):
   return subprocess.run([str(compiler.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(target),str(source)],env=env,check=check,capture_output=True,text=True)
  prior=list(P['cases']())
  for name in ('cases','numeric-cases','managed-cases'):prior+=list(runpy.run_path(str(ROOT/('utils/tests/dis64-scalar-client-'+name+'.py')))['cases']())
  for row in prior:
   source.write_text(row[8]);(w/'library.m').write_text(P['NS']['declarations'](row[1]));compile(a.limbo,'-64c',image);compile(a.limbo,'-i',plain)
   if a.baseline:
    compile(a.baseline,'-64c',old);compile(a.baseline,'-i',oldplain);assert image.read_bytes()==old.read_bytes()and plain.read_bytes()==oldplain.read_bytes(),row[0]
  (w/'command.m').write_text(C['INTERFACE']);count=0
  for name,provider,caller,_ in C['cases']():
   for text,flag in ((provider,'-64b'),(caller,'-64c')):
    source.write_text(text);compile(a.limbo,flag,image);data=image.read_bytes();compile(a.limbo,flag,image);assert data==image.read_bytes(),name;compile(a.limbo,'-i',plain)
    if a.baseline:
     compile(a.baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.baseline,flag,old,False);assert r.returncode and(not old.exists()or not old.stat().st_size),(name,r.stderr)
    if a.command_baseline:
     compile(a.command_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.command_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith('fields-')and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    count+=1
  prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
  negatives=[prefix+'d:=ctxt.display;img:=d.image;if(img!=nil)return;}',prefix+'local:=ref Draw->Display;if(local!=nil)return;}',prefix+'s:="another literal";n:=len s;if(n==0)return;}',prefix+'xs:=array[2]of int;xs[0]=1;if(xs[0]==1)return;}']
  for text in negatives:
   source.write_text(text);compile(a.limbo,'-i',plain);image.unlink(missing_ok=True);r=compile(a.limbo,'-64b',image,False)
   assert r.returncode and 'Dis64 command:'in r.stderr and(not image.exists()or not image.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  examples=[re.findall(r'```limbo\n(.*?)```',(ROOT/('doc/dis64-command-init'+language+'.md')).read_text(),re.S)for language in ('','.ru')]
  assert len(examples[0])==3 and examples[0]==examples[1]
  for name,text in zip(('command.m','library.b','client.b'),examples[0]):(w/name).write_text(text)
  for flag,name in [('-64b','library.b'),('-64c','client.b')]:
   source.write_text((w/name).read_text());compile(a.limbo,flag,image)
 print('PASS ordinary init producer:',count,'deterministic files;',len(prior),'prior clients; four well-typed source refusals; RU/EN tutorial compiled'+('; prior Dis64/all classic bytes equal'if a.baseline else'; no baseline'),flush=True)
if __name__=='__main__':main()
