#!/usr/bin/env python3
"""Mapped-root and String/List class counterexamples; malformed files never execute."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-proof.py'));C=P['C'];W=P['W'];enc=P['instruction']
def inputs(data):
 parts=P['CLIENT']['sections'](data);rows=C.decode(parts[2]);first=lambda name:next(i for i,r in enumerate(rows)if r[0]==name)
 yield 'valid',data,1
 moves=[i for i,r in enumerate(rows)if r[0]=='IMOVP'];cons=first('ICONSP');head=first('IHEADP');tail=first('ITAIL');lenc=first('ILENC');lenl=first('ILENL');string=rows[cons][2];list_=rows[head][2]
 def image(pc,changes):
  p=dict(parts);items=[r[-1]for r in rows];items[pc]=enc(rows[pc],**changes);p[2]=W(len(items))+b''.join(items);return P['rebuild'](p)
 for name,pc,changes in [
  ('length-class',lenl,{'source':string}),('string-class',lenc,{'source':list_}),('head-class',head,{'source':string}),('tail-class',tail,{'source':string}),
  ('cons-element-class',cons,{'source':list_}),('cons-tail-class',cons,{'dest':string}),('cons-hidden-middle',cons,{'mode':137,'middle':list_}),
  ('cons-immediate',cons,{'mode':17,'source':7}),('root-overlap',head,{'dest':string+4}),('root-module-read',moves[0],{'mode':1,'source':0}),
  ('root-mp-write',head,{'mode':8,'dest':8}),('root-header-write',head,{'dest':40}),('root-tail-write',head,{'dest':struct.unpack_from('>I',parts[3],4)[0]-8}),
  ('raw-root-word-read',lenc,{'name':'IMOVW','source':string}),('raw-root-big-read',lenc,{'name':'ICVTLW','source':string}),
  ('head-word',head,{'name':'IHEADW'}),('cons-word',cons,{'name':'ICONSW'}),('movp-scalar',moves[0],{'mode':17,'source':1})]:yield name,image(pc,changes),0
 frame=struct.unpack_from('>I',parts[3],4)[0]
 for label,offset,on in [('missing-string-map',string,False),('missing-list-map',list_,False),('prefix-map',0,True),('tail-map',frame-8,True)]:
  p=dict(parts);types=bytearray(p[3]);at=12+offset//64;mask=0x80>>((offset//8)%8)
  if on:types[at]|=mask
  else:types[at]&=~mask
  p[3]=bytes(types);yield label,P['rebuild'](p),0

 # Even/odd alias order needs repeated backward propagation from the final use.
 begin=first('IMFRAME');end=first('IMCALL')+2;group=[r[-1]for r in rows[begin:end]]
 group[1]=enc(rows[begin+1],mode=13,source=192);group[2]=enc(rows[begin+2],source=216);group[-1]=enc(rows[end-1],source=216,dest=196)
 def aliasimage(conflict):
  items=[rows[0][-1]]
  for i in list(range(0,15,2))+list(range(1,15,2)):items.append(enc(('IMOVP',9,64+8*i,0,72+8*i,0,b'')))
  if conflict:items.append(enc(('ICONSP',1,16,0,64,0,b'')))
  items.append(enc(('ILENC',9,184,0,192,0,b'')));items+=group+[C.EXIT]
  p=dict(parts);p[2]=W(len(items))+b''.join(items);p[3]=W(2,224,8)+b'\0\xff\xff\0'+W(24,8)+b'\xe0';return P['rebuild'](p)
 yield 'initial-alias-chain',aliasimage(False),1
 yield 'alias-class-conflict',aliasimage(True),0

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('class','tail','seed','alias'));a=p.parse_args();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='client-managed-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'library.m').write_text('Library:module{inc:fn(n:int):int;};');body=next(r[8]for r in runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-managed-cases.py'))['cases']()if r[0]=='list-cons-head-tail');(w/'client.b').write_text(body);image=w/'client.dis64'
  subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(image),str(w/'client.b')],env=env,check=True)
  if a.control:
   path='dis64-client-managed.h'if a.control=='class'else'dis64-scalar-client.h';text=(ROOT/'include'/path).read_text();old,new={
    'class':('(actual!=Dis64ClientRoot && kind!=Dis64ClientRoot && actual!=kind && !dis64clientmergekind(actual,kind))','0'),
    'tail':('8*i>v->frame-24','0'),
    'seed':('if(dis64clientisroot(slots[i].kind))seed[i/32]','if(0)seed[i/32]'),
    'alias':('if(!changed)break;','break;')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new))
   # Quoted nested includes must resolve the controlled managed helper locally.
   if a.control=='class':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'check';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-scalar-client-proof.c'),'-o',str(binary)],env=env,check=True)
  rows=list(inputs(image.read_bytes()));runner=shlex.split(env.get('NATIVE64_RUNNER',''))
  body=next(r[8]for r in runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client-managed-cases.py'))['cases']()if r[0]=='list-empty');(w/'client.b').write_text(body)
  subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(w),'-o',str(image),str(w/'client.b')],env=env,check=True);rows.append(('implicit-nil',image.read_bytes(),1))
  if a.control:rows=[r for r in rows if r[0]=={'class':'length-class','tail':'tail-map','seed':'implicit-nil','alias':'alias-class-conflict'}[a.control]]
  for name,data,expected in rows:
   f=w/(name+'.bin');f.write_bytes(data);r=subprocess.run(runner+[str(binary),str(f),str(expected)],env=env,capture_output=True,text=True)
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
   if a.control:assert r.returncode==10 and 'client raw proof mismatch'in r.stderr,(name,r.returncode,r.stderr)
   else:assert not r.returncode,(name,r.returncode,r.stderr)
  print('PASS mapped client raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' independent images',flush=True)
if __name__=='__main__':main()
