#!/usr/bin/env python3
"""Raw DAG admission, atomic refusal and active semantic omission controls."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
S=runpy.run_path(str(ROOT/'utils/tests/dis64-source-calls.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='chain-check-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);fixtures=[];nested=None;deep=None
  for name,text,kind,value,failure in S['cases']():
   source=w/(name+'.b');source.write_text(text);image=w/(name+'.dis64')
   S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-64c','-o',str(image),str(source)],env=env,check=True,capture_output=True)
   data=image.read_bytes();fixtures.append((name,data,1,S['CALLS'].LIMIT,64))
   fixtures.extend((name+'-'+label,data,0,limit,cap)for label,limit,cap in [('budget',1,64),('capacity',S['CALLS'].LIMIT,1)])
   if name=='nested-forward':nested=data
   if name=='nested-deep':deep=data
  bad=list(S['nested_damaged'](nested))+list(S['mutual_damaged'](deep));fixtures.extend((name,data,0,S['CALLS'].LIMIT,64)for name,data in bad)
  for name,data,expected,limit,cap in fixtures:
   if expected and name.startswith('nested-'):
    # Source layout supplies only the independent test's expected spans, never C input.
    b=w/(name+'.b');_,rows=S['TYPED']['compile_source'](a.limbo.resolve(),w,None,filename=b,multi=True);S['verify'](data,rows)
  for name,data in bad:
   b=w/('nested-deep.b'if name=='mutual-cycle'else'nested-forward.b')
   _,rows=S['TYPED']['compile_source'](a.limbo.resolve(),w,None,filename=b,multi=True)
   S['verify'](data,rows,expected=False)
  def protocol(rows,path):
   path.write_bytes(struct.pack('>I',len(rows))+b''.join(struct.pack('>IIII',len(d),e,l,c)+d for _,d,e,l,c in rows))
  protocol(fixtures,w/'cases.bin');binary=w/'check'
  cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'))
  command=cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-chain-check.c'),'-o',str(binary)]
  runner=shlex.split(env.get('NATIVE64_RUNNER',''));header=(ROOT/'include/dis64-call-chain.h').read_text();local=w/'dis64-call-chain.h';local.write_text(header)
  subprocess.run(command,check=True,env=env);subprocess.run(runner+[str(binary),str(w/'cases.bin')],check=True,env=env,timeout=120)
  print('PASS raw DAG:',len(fixtures),'cases; old proof summaries preserved; atomic output/count/image refusal',flush=True)
  controls=[('cycle','if(j<n)continue;','if(0)continue;','self-cycle'),
   ('mutual-cycle','if(j<n)continue;','if(0)continue;','mutual-cycle'),
   ('atomic','done[i]=1;used++;progress=1;','output[i]=results[i];done[i]=1;used++;progress=1;','deep-return-missing'),
   ('return-cover','if(i && !dis64returncover','if(0 && i && !dis64returncover','deep-leaf-return-missing'),
   ('bind-owner','if((unsigned long)instruction->destination != pointer)','if(0)','deep-result-owner'),
   ('return-owner','instruction->destination!=32','0','deep-return-owner')]
  for name,before,after,case in controls:
   assert header.count(before)==1;local.write_text(header.replace(before,after));subprocess.run(command,check=True,env=env)
   selected=[x for x in fixtures if x[0]==case];assert len(selected)==1;protocol(selected,w/'control.bin')
   r=subprocess.run(runner+[str(binary),str(w/'control.bin')],env=env,capture_output=True,text=True,timeout=120)
   assert r.returncode==10 and ('accepted=1 expected=0'in r.stderr if name!='atomic'else'chain predicate failed'in r.stderr)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   print('PASS raw DAG active predicate control:',name,flush=True)
if __name__=='__main__':main()
