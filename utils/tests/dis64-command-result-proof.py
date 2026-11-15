#!/usr/bin/env python3
"""Managed result classes, mapped binders, return definitions and forwarding guards."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));enc=runpy.run_path(str(ROOT/'utils/tests/dis64-client-index-proof.py'))['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('defined','classes'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='command-result-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('string','list','ints','context','forward-list','branch','tail'):
   _,text,_,_=next(r for r in CMD['CASES']['resultcases']()if r[0]=='result-'+case)
   (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);types=T['types'](base[3]);rows.append((case+'-valid',P['rebuild'](base),1))
   def add(name,b):rows.append((case+'-'+name,P['rebuild'](b),0))
   def mutate(name,changes):
    b=dict(base);items=[r[-1]for r in code]
    for pc,fields in changes:items[pc]=enc(code[pc],**fields)
    b[2]=W(len(items))+b''.join(items);add(name,b)
   binder=next(i for i,r in enumerate(code)if r[0]=='ILEA');bound=code[binder];store=next(i for i,r in enumerate(code)if r[0]in('IMOVP','IHEADP','ITAIL')and r[1]&7==5 and r[4]==32);ret=code[store]
   for name,fields in [('binder-header',dict(source=32)),('binder-result-tail',dict(source=types[0][0]-8)),('binder-address-tail',dict(source=types[0][0]-16)),('binder-interior',dict(source=bound[2]+4)),('binder-destination',dict(dest=48)),('binder-offset',dict(secondary=64)),('binder-zero-offset',dict(secondary=0)),('binder-mp',dict(mode=5)),('binder-inherit-root',dict(name='IMOVL',source=32))]:mutate(name,[(binder,fields)])
   for name,fields in [('return-offset',dict(secondary=8)),('return-header',dict(dest=0)),('return-frame',dict(dest=40)),('return-word',dict(name='IMOVW')),('return-source-header',dict(mode=13,source=32)),('return-unmapped',dict(mode=13,source=types[-1][0]-8))]:mutate(name,[(store,fields)])
   mutate('undefined-return',[(store,dict(name='IMOVP',mode=1,source=8,dest=64,secondary=0))])
   ts=[t[:]for t in types];map=bytearray(ts[0][2]);bit=bound[2]//8;map[bit//8]&=~(0x80>>(bit%8));ts[0][2]=bytes(map);b=dict(base);b[3]=T['typebytes'](ts);add('result-map',b)
   if case=='list':mutate('wrong-class',[(store,dict(name='IMOVP',mode=5,source=24))])
   if case=='forward-list':
    inherited=next(i for i,r in enumerate(code)if r[0]=='IMOVL'and r[1]&7==5 and r[5]==32)
    for name,fields in [('inherit-header',dict(source=40)),('inherit-root',dict(source=64)),('inherit-offset',dict(secondary=64)),('inherit-word',dict(name='IMOVW'))]:mutate(name,[(inherited,fields)])
   frame=next(i for i,r in enumerate(code)if r[0]=='IFRAME')
   branches=[i for i,r in enumerate(code)if r[0]in('IJMP','IBEQW','IBNEW','IBEQL','IBNEL','IBEQC','IBNEC')]
   if branches:mutate('branch-binder',[(branches[0],dict(dest=binder))]);mutate('branch-call',[(branches[0],dict(dest=binder+1))])
  if a.control:
   chosen={'defined':'string-undefined-return','classes':'list-wrong-class'}[a.control];rows=[r for r in rows if r[0]==chosen];assert rows
   path='dis64-scalar-client.h'if a.control=='defined'else'dis64-client-local.h';text=(ROOT/'include'/path).read_text();old,new={'defined':('node->needload=library && v->returning;','node->needload=0;'),'classes':('v->localresult[i-1]=body.returnkind;','v->localresult[i-1]=Dis64ClientRoot;')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new))
   if path!='dis64-scalar-client.h':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),'library',str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS managed result raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
