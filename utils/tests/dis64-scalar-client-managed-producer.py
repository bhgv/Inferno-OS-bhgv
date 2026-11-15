#!/usr/bin/env python3
"""Managed source coverage, prior byte compatibility and unsupported source boundaries."""
import argparse,os,re,runpy,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'))
OPS={'IMOVP','ILENC','ILENL','ICONSP','IHEADP','ITAIL'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--baseline',type=Path);a=p.parse_args();env=dict(os.environ);seen=set();counts=[0,0]
 with tempfile.TemporaryDirectory(prefix='client-managed-producer-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);source=w/'client.b';interface=w/'library.m';image=w/'new.dis64';old=w/'old.dis64';plain=w/'plain.dis';oldplain=w/'old.dis'
  previous=[r+(False,)for r in P['cases']()]
  for path in ('dis64-scalar-client-cases.py','dis64-scalar-client-numeric-cases.py'):previous+=list(runpy.run_path(str(ROOT/'utils/tests'/path))['cases']())
  managed=list(runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-managed-cases.py'))['cases']())
  def compile(compiler,flag,target,check=True):return subprocess.run([str(compiler.resolve()),flag,'-I',str(w),'-o',str(target),str(source)],env=env,check=check,capture_output=True,text=True)
  for isnew,rows in [(False,previous),(True,managed)]:
   for name,text,fn,kind,args,values,expected,failure,body,guarded in rows:
    source.write_text(body);interface.write_text(P['NS']['declarations'](text));compile(a.limbo,'-64c',image);data=image.read_bytes();compile(a.limbo,'-64c',image);assert data==image.read_bytes(),name;compile(a.limbo,'-i',plain)
    if isnew:seen|={r[0]for r in P['S']['CALLS'].decode(P['sections'](data)[2])}&OPS
    if a.baseline:
     compile(a.baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name;old.unlink(missing_ok=True);r=compile(a.baseline,'-64c',old,False)
     if isnew:assert r.returncode and 'Dis64 client:'in r.stderr and(not old.exists()or not old.stat().st_size),(name,r.returncode,r.stderr)
     else:assert not r.returncode and old.read_bytes()==data,(name,r.returncode,r.stderr)
     assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
    counts[isnew]+=1
  assert seen==OPS,seen
  base=managed[4][8];interface.write_text('Library:module{inc:fn(n:int):int;};')
  negative=[base.replace('l:=s::nil;','l:=7::nil;').replace('len h','h'),base.replace('l:=s::nil;','inner:=s::nil;l:=inner::nil;'),base.replace('s:="library.dis64";','s:="unrelated literal";s[0]=65;'),base.replace('init:fn();','init:fn(argv:list of string);').replace('init(){','init(argv:list of string){')]
  for body in negative:
   source.write_text(body);image.unlink(missing_ok=True);r=compile(a.limbo,'-64c',image,False)
   assert r.returncode and 'Dis64 client:'in r.stderr and(not image.exists()or not image.stat().st_size),r.stderr
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  examples=[re.findall(r'```limbo\n(.*?)```',(ROOT/('doc/dis64-client-managed'+language+'.md')).read_text(),re.S)for language in ('','.ru')]
  assert len(examples[0])==3 and examples[0]==examples[1]
  for name,text in zip(('library.m','library.b','client.b'),examples[0]):(w/name).write_text(text)
  for flag,name in [('-64b','library.b'),('-64c','client.b')]:
   subprocess.run([str(a.limbo.resolve()),flag,'-I',str(w),'-o',str(w/'tutorial.dis64'),str(w/name)],env=env,check=True,capture_output=True)
 print('PASS managed producer:',counts[1],'new sources/six managed operations;',counts[0],'prior sources; four source refusals; RU/EN tutorial compiled; deterministic'+('; prior Dis64/all classic bytes equal'if a.baseline else'; no baseline selected'),flush=True)
if __name__=='__main__':main()
