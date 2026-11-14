#!/usr/bin/env python3
"""Independent CFG counterexamples: joins, first loop entry and call-island boundaries."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-proof.py'));C=P['C'];W=P['W'];enc=P['instruction']
def op(name,mode,source=0,middle=0,dest=0,secondary=0):return enc((name,mode,source,middle,dest,secondary,b''))
def inputs(data):
 parts=P['CLIENT']['sections'](data);r=C.decode(parts[2]);begin=next(i for i,x in enumerate(r)if x[0]=='IMFRAME');end=next(i for i,x in enumerate(r)if x[0]=='IMCALL')+2;group=[x[-1]for x in r[begin:end]]
 arg=r[begin+1][2];result=r[end-1][4];last=next(x[4]for x in reversed(r)if x[0]=='IMOVW'and x[2]==r[end-1][2]);second=group.copy();second[1]=enc(r[begin+1],source=result);second[-1]=enc(r[end-1],dest=last)
 def image(items,types=None):
  p=dict(parts);p[2]=W(len(items))+b''.join(items)
  if types is not None:p[3]=types
  return P['rebuild'](p)
 prefix=[x[-1]for x in r[:begin]];branch=len(prefix);left=branch+1;right=left+len(group)+1;join=right+len(group)
 diamond=prefix+[op('IBEQW',82,7,7,right)]+group+[op('IJMP',26,dest=join)]+group+second+[C.EXIT]
 yield 'diamond',image(diamond),1
 yield 'early-before-load',image([op('IBEQW',82,0,0,len(r))]+[row[-1]for row in r]),1
 broken=diamond.copy();broken[right+len(group)-1]=enc(r[end-1],dest=last);yield 'join-missing-result',image(broken),0
 # Both static arms remain reachable; only one fails to produce the needed scalar.
 broken=diamond.copy();broken[0]=op('IMOVW',17,0,dest=arg);yield 'no-load',image(broken),0
 # A loop body defines result, but its zero-iteration exit must not inherit that fact.
 bodygroup=group.copy();bodygroup[1]=enc(r[begin+1],mode=21,source=7)
 body=2;head=body+len(group)+1
 loop=[r[0][-1],op('IJMP',26,dest=head)]+bodygroup+[op('IJMP',26,dest=head),op('IBNEW',82,0,0,body)]+second+[C.EXIT]
 yield 'loop-first-entry',image(loop),0
 seeded=[r[0][-1],op('IMOVW',17,7,dest=result),op('IJMP',26,dest=head+1)]+bodygroup+[op('IJMP',26,dest=head+1),op('IBNEW',82,0,0,body+1)]+second+[C.EXIT]
 yield 'loop-initialized',image(seeded),1
 # Fallthrough still reaches the complete group, so reachability alone cannot reject the target.
 for n in range(1,len(group)):
  items=prefix+[op('IBEQW',82,7,7,len(prefix)+1+n)]+group+second+[C.EXIT]
  yield 'setup-'+str(n),image(items),0
 for label,target in [('negative',-1),('outside',len(diamond))]:
  b=diamond.copy();b[branch]=op('IBEQW',82,7,7,target);yield label,image(b),0
 # Scalar node kinds cannot change at a merge even when paths are disjoint.
 b=diamond.copy();b[branch]=op('IBEQW',82,7,7,join+1);yield 'skip-frame',image(b),0
 # More than one machine word of definite-assignment bits.
 def wide(n,initialized,source,frame):
  tail=frame-8;g=group.copy();g[1]=enc(r[begin+1],mode=13,source=source);g[2]=enc(r[begin+2],source=tail);g[-1]=enc(r[end-1],source=tail,dest=240)
  types=W(2,frame,8)+b'\0'*((frame+63)//64)+W(24,8)+b'\xe0'
  return image([r[0][-1]]+[op('IMOVW',17,i,dest=64+4*i)for i in range(initialized)]+g+[C.EXIT],types)
 yield 'high-bits-valid',wide(48,48,252,272),1
 yield 'high-bits-missing',wide(48,47,252,272),0
 yield '64-spans',wide(64,64,316,336),1
 yield '65-spans',wide(65,65,320,344),0

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);parser.add_argument('--control',choices=('join','load','boundary','high'));a=parser.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='client-cfg-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'library.m').write_text('Library:module{id:fn(n:int):int;};');(w/'client.b').write_text(P['CLIENT']['client']('id',1,[1],[7]));image=w/'client.dis64'
  subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(image),str(w/'client.b')],env=env,check=True)
  cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
  if a.control:
   text=(ROOT/'include/dis64-scalar-client.h').read_text();old,new={
    'join':('incoming[0]&=nodes[j].out[0];','incoming[0]|=nodes[j].out[0];'),
    'load':('(nodes[i].needload && !nodes[i].inload)','0'),
    'boundary':('(nodes[i].target<v->count && !nodes[nodes[i].target].boundary)','0'),
    'high':('(nodes[i].need[1]&~nodes[i].in[1])','0')}[a.control];assert text.count(old)==1;(w/'dis64-scalar-client.h').write_text(text.replace(old,new))
  binary=w/'check';subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-scalar-client-proof.c'),'-o',str(binary)],env=env,check=True)
  rows=list(inputs(image.read_bytes()))
  (w/'client.b').write_text('implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";if(m==nil)return;r:=m->id(7);}')
  subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(image),str(w/'client.b')],env=env,check=True)
  nilparts=P['CLIENT']['sections'](image.read_bytes());decoded=C.decode(nilparts[2]);pc=next(i for i,r in enumerate(decoded)if r[0]in('IBEQL','IBNEL')and r[1]==194)
  rows.append(('nil-width-valid',image.read_bytes(),1))
  for label,changed in [('word-nil',{'name':'IBEQW'if decoded[pc][0]=='IBEQL'else'IBNEW'}),('nil-other-cell',{'middle':16}),('nil-source-cell',{'source':8})]:
   items=[r[-1]for r in decoded];items[pc]=enc(decoded[pc],**changed);parts=dict(nilparts);parts[2]=W(len(items))+b''.join(items);rows.append((label,P['rebuild'](parts),0))
  runner=shlex.split(env.get('NATIVE64_RUNNER',''))
  if a.control:rows=[row for row in rows if row[0]=={'join':'join-missing-result','load':'no-load','boundary':'setup-1','high':'high-bits-missing'}[a.control]]
  for name,data,expected in rows:
   f=w/(name+'.bin');f.write_bytes(data);r=subprocess.run(runner+[str(binary),str(f),str(expected)],env=env,capture_output=True,text=True)
   if a.control:assert r.returncode==10 and 'client raw proof mismatch'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr)
   else:assert r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS client CFG proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' independent images',flush=True)
if __name__=='__main__':main()
