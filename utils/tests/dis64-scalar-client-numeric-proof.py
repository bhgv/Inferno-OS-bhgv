#!/usr/bin/env python3
"""Raw numeric role counterexamples; no malformed image is ever executed."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-proof.py'));C=P['C'];W=P['W'];enc=P['instruction']
def op(name,source,dest,mode=9,middle=0):return enc((name,mode,source,middle,dest,0,b''))
def inputs(data):
 parts=P['CLIENT']['sections'](data);r=C.decode(parts[2]);begin=next(i for i,x in enumerate(r)if x[0]=='IMFRAME');end=next(i for i,x in enumerate(r)if x[0]=='IMCALL')+2
 group=[x[-1]for x in r[begin:end]];group[1]=enc(r[begin+1],mode=13,source=104);group[2]=enc(r[begin+2],source=136);group[-1]=enc(r[end-1],source=136,dest=108)
 prefix=[r[0][-1],op('IMOVW',7,64,17),op('ICVTWL',64,72),op('ICVTWF',64,80),op('ICVTWF',64,88)]
 def image(instruction):
  p=dict(parts);items=prefix+[instruction,op('IMOVW',1,104,17)]+group+[C.EXIT];p[2]=W(len(items))+b''.join(items);p[3]=W(2,144,8)+b'\0\0\0'+W(24,8)+b'\xe0';return P['rebuild'](p)
 for name,source,dest in [('ICVTLW',72,104),('ICVTFW',80,104),('ICVTLF',72,88),('ICVTFL',80,96),('INEGF',80,88)]:
  yield name,image(op(name,source,dest)),1
  yield name+'-immediate',image(op(name,7,dest,17)),0
  yield name+'-uninitialized',image(op(name,112,dest)),0
  yield name+'-wrong-source',image(op(name,64,dest)),0
  yield name+'-hidden-middle',image(op(name,source,dest,73,1)),0
 for name in ('IADDF','ISUBF','IMULF','IDIVF'):
  yield name,image(op(name,80,88)),1 # implicit middle reads the old destination
  yield name+'-three-operands',image(op(name,80,88,137,88)),1
  for label,source,dest,mode,middle in [('middle-immediate',80,88,73,1),('source-immediate',1,88,17,0),('uninitialized-middle',80,88,137,112),('wrong-middle',80,88,137,72),('wrong-result',80,72,137,88),('overlap',80,84,137,88),('mp-source',16,88,1,0)]:
   yield name+'-'+label,image(op(name,source,dest,mode,middle)),0
 yield 'conversion-result-kind',image(op('ICVTLF',72,72)),0
 for name,dest in [('ICVTWL',72),('ICVTWF',88)]:yield name+'-word-immediate',image(op(name,7,dest,17)),1

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('source','result','initialized'));a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='client-numeric-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'library.m').write_text('Library:module{id:fn(n:int):int;};');(w/'client.b').write_text(P['CLIENT']['client']('id',1,[1],[7]));image=w/'client.dis64'
  subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(image),str(w/'client.b')],env=env,check=True)
  if a.control:
   text=(ROOT/'include/dis64-scalar-client.h').read_text();old,new={
    'source':('case ICVTLW:*source=Dis64Big;','case ICVTLW:*source=Dis64Int;'),
    'result':('case ICVTLF:*source=Dis64Big;*destination=Dis64Real;','case ICVTLF:*source=Dis64Big;*destination=Dis64Big;'),
    'initialized':('(nodes[i].need[0]&~nodes[i].in[0])','0')}[a.control];assert text.count(old)==1;(w/'dis64-scalar-client.h').write_text(text.replace(old,new))
  binary=w/'check';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
  subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-scalar-client-proof.c'),'-o',str(binary)],env=env,check=True)
  rows=list(inputs(image.read_bytes()));runner=shlex.split(env.get('NATIVE64_RUNNER',''))
  if a.control:rows=[r for r in rows if r[0]=={'source':'ICVTLW-wrong-source','result':'conversion-result-kind','initialized':'ICVTFW-uninitialized'}[a.control]]
  for name,data,expected in rows:
   path=w/(name+'.bin');path.write_bytes(data);r=subprocess.run(runner+[str(binary),str(path),str(expected)],env=env,capture_output=True,text=True)
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
   if a.control:assert r.returncode==10 and 'client raw proof mismatch'in r.stderr,(name,r.returncode,r.stderr)
   else:assert not r.returncode,(name,r.returncode,r.stderr)
  print('PASS numeric client raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' independent images',flush=True)
if __name__=='__main__':main()
