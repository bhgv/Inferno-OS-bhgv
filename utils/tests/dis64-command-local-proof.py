#!/usr/bin/env python3
"""Independent local managed-frame, call-island and acyclic-body admission guards."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));enc=runpy.run_path(str(ROOT/'utils/tests/dis64-client-index-proof.py'))['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--control',choices=('arguments','cycle'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='command-local-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('once','callee-list','cross','max-depth','nested-live'):
   _,text,_,_=next(r for r in CMD['CASES']['localcases']()if r[0]=='local-'+case)
   (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);types=T['types'](base[3]);rows.append((case+'-valid',P['rebuild'](base),1))
   def add(name,b):rows.append((case+'-'+name,P['rebuild'](b),0))
   def mutate(name,changes):
    b=dict(base);items=[r[-1]for r in code]
    for pc,fields in changes:items[pc]=enc(code[pc],**fields)
    b[2]=W(len(items))+b''.join(items);add(name,b)
   frame=next(i for i,r in enumerate(code)if r[0]=='IFRAME');f=code[frame];entry=code[frame+3][4];typ=f[2]
   for name,fields in [('frame-header',dict(dest=32)),('frame-root',dict(dest=64)),('frame-context',dict(source=1)),('frame-mp',dict(mode=1,source=0)),('frame-outside',dict(source=13))]:mutate(name,[(frame,fields)])
   for name,pc,fields in [('context-slot',frame+1,dict(secondary=72)),('argv-slot',frame+2,dict(secondary=64)),('argv-string',frame+2,dict(mode=5,source=24)),('argument-word',frame+1,dict(name='IMOVW')),('argument-other-frame',frame+2,dict(dest=48)),('argument-header',frame+1,dict(secondary=0)),('call-header',frame+3,dict(source=32)),('call-middle',frame+3,dict(mode=74,middle=1)),('call-into-body',frame+3,dict(dest=entry+1)),('call-root-entry',frame+3,dict(dest=0)),('call-mcall',frame+3,dict(name='IMCALL')),('missing-call',frame+3,dict(name='IJMP',mode=26,source=0,dest=frame+4)),('missing-frame',frame,dict(name='IJMP',mode=26,source=0,dest=frame+1))]:mutate(name,[(pc,fields)])
   for name,offset,mask in [('map-context',1,0x80),('map-argv',1,0x40),('map-header',0,0x80)]:
    ts=[t[:]for t in types];v=bytearray(ts[typ][2]);v[offset]^=mask;ts[typ][2]=bytes(v);b=dict(base);b[3]=T['typebytes'](ts);add(name,b)
   ts=[t[:]for t in types];ts[typ][0]=88;b=dict(base);b[3]=T['typebytes'](ts);add('frame-extent',b)
   ts=[t[:]for t in types];ts[typ][1]=4;b=dict(base);b[3]=T['typebytes'](ts);add('frame-align',b)
   b=dict(base);b[3]=T['typebytes'](types+[types[typ]]);add('unused-frame',b)
   b=dict(base);b[3]=T['typebytes'](types[:typ]+types[typ+1:]);add('missing-type',b)
   if case=='callee-list':
    ts=[t[:]for t in types];v=bytearray(ts[typ][2]);# Remove a helper-local root while preserving both argument roots.
    roots=[bit for bit in range(10,ts[typ][0]//8-2)if v[bit//8]&(0x80>>(bit%8))];assert roots
    bit=roots[0];v[bit//8]^=0x80>>(bit%8);ts[typ][2]=bytes(v);b=dict(base);b[3]=T['typebytes'](ts);add('helper-local-map',b)
   if case=='cross':
    inner=next(i for i,r in enumerate(code)if i>=entry and r[0]=='IFRAME')
    mutate('cycle',[(inner,dict(source=typ)),(inner+3,dict(dest=entry))])
    mutate('target-type-mismatch',[(inner,dict(source=typ))])
   branches=[i for i,r in enumerate(code)if i>=entry and r[0]in('IJMP','IBEQW','IBNEW','IBEQL','IBNEL','IBEQC','IBNEC')]
   if branches:
    mutate('branch-cross-body',[(branches[0],dict(dest=0))]);mutate('branch-call-interior',[(branches[0],dict(dest=frame+1))])
   mutate('helper-exit',[(len(code)-1,dict(name='IEXIT'))])
  if a.control:
   chosen={'arguments':'once-context-slot','cycle':'cross-cycle'}[a.control];rows=[r for r in rows if r[0]==chosen];assert rows
   text=(ROOT/'include/dis64-client-local.h').read_text();old,new={'arguments':('d->secondary!=64+8*j','0'),'cycle':('if(edges[i+1]&(1U<<i))return 0;','if(0)return 0;')}[a.control];assert text.count(old)==1;(w/'dis64-client-local.h').write_text(text.replace(old,new));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),'library',str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS local managed-call raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
