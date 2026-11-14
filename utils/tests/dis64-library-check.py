#!/usr/bin/env python3
"""Whole-library raw proof, ordinal-zero semantics and atomic refusal controls."""
import argparse,os,shlex,struct,subprocess,tempfile,runpy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
L=runpy.run_path(str(ROOT/'utils/tests/dis64-library-files.py'));S=L['S'];C=S['CALLS']
def mutants(name,data):
 s=S['sections'](data);decoded=C.decode(s[2]);items=[x[-1]for x in decoded]
 def changed(label,edits):
  t=dict(s);raw=items.copy()
  for pc,encoded in edits.items():raw[pc]=encoded
  t[2]=C.IMAGE(raw);return name+'-'+label,S['rebuild'](t)
 if name=='single':
  yield changed('return-missing',{0:C.INIT.move(0,64)})
  yield changed('exit',{len(items)-1:C.EXIT})
 if name=='first-caller':
  frame=next(i for i,x in enumerate(decoded)if x[0]=='IFRAME');call=next(i for i,x in enumerate(decoded)if x[0]=='ICALL')
  yield changed('self-cycle',{frame:C.frame(0,decoded[frame][4]),call:C.call(0,decoded[call][2])})
 if name=='deep':
  frames=[i for i,x in enumerate(decoded)if x[0]=='IFRAME'];calls=[i for i,x in enumerate(decoded)if x[0]=='ICALL'];f,c=frames[1],calls[1]
  yield changed('mutual-cycle',{f:C.frame(0,decoded[f][4]),c:C.call(0,decoded[c][2])})
 for label,part in (('trailer',data+b'\0'),('short',data[:-1])):yield name+'-'+label,part

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='library-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);fixtures=[]
  for name,text,*_ in L['cases']():
   b=w/(name+'.b');b.write_text(text);image=w/(name+'.dis64');S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-64b','-o',str(image),str(b)],env=env,capture_output=True,check=True)
   data=image.read_bytes();_,report=S['TYPED']['compile_source'](a.limbo.resolve(),w,None,filename=b,multi=True)
   S['verify'](data,report,library=True);fixtures.append((name,data,1,C.LIMIT,64))
   fixtures.extend((name+'-'+kind,data,0,limit,cap)for kind,limit,cap in [('budget',0 if name=='empty'else 1,64),('capacity',C.LIMIT,0)])
   for label,bad in mutants(name,data):
    if not label.endswith(('trailer','short')):S['verify'](bad,report,expected=False,library=True)
    fixtures.append((label,bad,0,C.LIMIT,64))
  def protocol(rows,path):path.write_bytes(C.words(len(rows))+b''.join(C.words(len(d),e,l,c)+d for _,d,e,l,c in rows))
  command=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
  command+=['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-chain-check.c'),'-o',str(w/'proof')]
  runner=shlex.split(env.get('NATIVE64_RUNNER',''));protocol(fixtures,w/'cases.bin');subprocess.run(command,env=env,check=True);subprocess.run(runner+[str(w/'proof'),str(w/'cases.bin'),'library'],env=env,check=True,timeout=120)
  print('PASS raw library:',len(fixtures),'cases, ordinal0 arguments/results/calls, complete proof and atomic output/count/image refusal',flush=True)
  header=(ROOT/'include/dis64-call-chain.h').read_text()
  for name,before,after,case in [('first-return','if((library || i) && !dis64returncover','if(i && !dis64returncover','single-return-missing'),('self-cycle','if(j<n)continue;','if(0)continue;','first-caller-self-cycle'),('mutual-cycle','if(j<n)continue;','if(0)continue;','deep-mutual-cycle')]:
   assert header.count(before)==1;(w/'dis64-call-chain.h').write_text(header.replace(before,after));subprocess.run(command,env=env,check=True)
   selected=[x for x in fixtures if x[0]==case];assert len(selected)==1;protocol(selected,w/'control.bin')
   r=subprocess.run(runner+[str(w/'proof'),str(w/'control.bin'),'library'],env=env,capture_output=True,text=True,timeout=120)
   assert r.returncode==10 and 'accepted=1 expected=0'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   print('PASS raw library active predicate control:',name,flush=True)
if __name__=='__main__':main()
