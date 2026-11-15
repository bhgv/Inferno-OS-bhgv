#!/usr/bin/env python3
"""Independent byte descriptor, zeroing constructor and slice ownership proofs."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));C=P['C'];CMD=P['CMD'];CASES=CMD['CASES'];W=P['W'];enc=P['enc'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('descriptor','unused','classes'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='client-arrays-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CASES['INTERFACE'])
  for case in ('new-context','new-empty','slice-drop-root'):
   _,provider,caller,_=next(r for r in CASES['arraycases']()if r[0]=='arrays-'+case)
   for owner,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
    (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
    base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);types=T['types'](base[3]);label=case+'-'+owner;rows.append((label+'-valid',P['rebuild'](base),owner,1))
    def add(name,b):rows.append((label+'-'+name,P['rebuild'](b),owner,0))
    news=[i for i,r in enumerate(code)if r[0]=='INEWAZ'];slices=[i for i,r in enumerate(code)if r[0]=='ISLICEA']
    if news:
     assert types[-1]==[1,1,b'\0']
     for name,new in [('size',[2,1,b'\0']),('align',[8,8,b'\0']),('map',[8,8,b'\x80'])]:
      ts=[x[:]for x in types];ts[-1]=new;b=dict(base);b[3]=T['typebytes'](ts);add('byte-'+name,b)
     ts=[x[:]for x in types];b=dict(base);b[3]=T['typebytes'](ts[:-1]);add('byte-missing',b)
     idx=news[0];ins=code[idx]
     variants=[('descriptor-frame',dict(middle=0)),('descriptor-mp',dict(middle=1)),('constructor-uninitialized',dict(name='INEWA')),('index-unported',dict(name='IINDB')),('constructor-source-root',dict(mode=(ins[1]&~56)|8,source=64 if owner=='library'else 0)),('constructor-destination-context',dict(dest=64 if owner=='library'else 0)),('constructor-middle-fp',dict(mode=(ins[1]&63)|128,middle=64)),('constructor-destination-mp',dict(mode=ins[1]&~7,dest=8))]
     for name,changes in variants:
      b=dict(base);items=[r[-1]for r in code];items[idx]=enc(ins,**changes);b[2]=W(len(items))+b''.join(items);add(name,b)
    else:
     ts=[x[:]for x in types]+[[1,1,b'\0']];b=dict(base);b[3]=T['typebytes'](ts);add('byte-unused',b)
    if slices:
     idx=slices[0];ins=code[idx]
     for name,changes in [('slice-context',dict(dest=64 if owner=='library'else 0)),('slice-word',dict(source=64,mode=(ins[1]&~56)|8)),('slice-middle-root',dict(middle=64,mode=(ins[1]&63)|128)),('slice-wide-immediate',dict(middle=32768,mode=(ins[1]&63)|64)),('slice-mp',dict(dest=8,mode=ins[1]&~7)),('slice-outside',dict(dest=4088)),('slice-misaligned',dict(dest=ins[4]+1))]:
      b=dict(base);items=[r[-1]for r in code];items[idx]=enc(ins,**changes);b[2]=W(len(items))+b''.join(items);add(name,b)
  if a.control:
   path='dis64-client-managed.h'if a.control=='classes'else'dis64-client-literals.h';text=(ROOT/'include'/path).read_text();old,new={'descriptor':('in->middle!=(long)v->bytetype','0'),'unused':('!!arrays==v->bytearrays','1'),'classes':('actual!=kind','0')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text());chosen={'descriptor':'new-context-library-descriptor-frame','unused':'slice-drop-root-library-byte-unused','classes':'slice-drop-root-library-slice-context'}[a.control];rows=[r for r in rows if r[0]==chosen];assert rows
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,owner,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),owner,str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS byte array raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
