#!/usr/bin/env python3
"""Numeric source coverage, deterministic Dis64 and prior compiler byte compatibility."""
import argparse,os,re,runpy,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'))
NEW={'IADDF','ISUBF','IMULF','IDIVF','INEGF','ICVTLW','ICVTFW','ICVTLF','ICVTFL'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--baseline',type=Path);a=p.parse_args();env=dict(os.environ);seen=set();counts=[0,0]
 with tempfile.TemporaryDirectory(prefix='client-numeric-producer-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w)
  oldrows=[r+(False,)for r in P['cases']()]+list(runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-cases.py'))['cases']())
  newrows=list(runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-numeric-cases.py'))['cases']())
  for isnew,rows in [(False,oldrows),(True,newrows)]:
   for name,text,fn,kind,args,values,expected,failure,body,guarded in rows:
    (w/'library.m').write_text(P['NS']['declarations'](text));source=w/'client.b';source.write_text(body);image=w/'new.dis64';plain=w/'new.dis';old=w/'old.dis';refused=w/'old.dis64'
    def compile(compiler,flag,target,check=True):return subprocess.run([str(compiler.resolve()),flag,'-I',str(w),'-o',str(target),str(source)],env=env,check=check,capture_output=True,text=True)
    compile(a.limbo,'-64c',image);first=image.read_bytes();compile(a.limbo,'-64c',image);assert first==image.read_bytes(),name
    compile(a.limbo,'-i',plain)
    if isnew:
     names={r[0]for r in P['S']['CALLS'].decode(P['sections'](first)[2])};assert names&NEW,name;seen|=names&NEW
    if a.baseline:
     compile(a.baseline,'-i',old);assert plain.read_bytes()==old.read_bytes(),name
     refused.unlink(missing_ok=True);r=compile(a.baseline,'-64c',refused,False)
     if isnew:assert r.returncode and 'Dis64 client:'in r.stderr and(not refused.exists()or not refused.stat().st_size),(name,r.returncode,r.stderr)
     else:assert not r.returncode and refused.read_bytes()==first,(name,r.returncode,r.stderr)
     assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
    counts[isnew]+=1
  examples=[]
  for language in ('', '.ru'):
   examples.append(re.findall(r'```limbo\n(.*?)```',(ROOT/('doc/dis64-client-numeric'+language+'.md')).read_text(),re.S))
  assert len(examples[0])==3 and examples[0]==examples[1]
  for name,text in zip(('library.m','library.b','client.b'),examples[0]):(w/name).write_text(text)
  for flag,name in [('-64b','library.b'),('-64c','client.b')]:
   subprocess.run([str(a.limbo.resolve()),flag,'-I',str(w),'-o',str(w/'tutorial.dis64'),str(w/name)],env=env,check=True,capture_output=True)
  assert seen==NEW,seen
 print('PASS numeric producer:',counts[1],'new sources/all nine new operations;',counts[0],'prior sources; RU/EN tutorial compiled; deterministic output'+('; prior Dis64 and all classic files identical'if a.baseline else'; no prior compiler selected'),flush=True)
if __name__=='__main__':main()
