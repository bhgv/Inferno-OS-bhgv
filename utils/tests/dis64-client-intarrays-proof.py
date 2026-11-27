#!/usr/bin/env python3
"""Independent scalar-array descriptor, element-class and borrowed-pair proofs."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));enc=runpy.run_path(str(ROOT/'utils/tests/dis64-client-index-proof.py'))['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--control',choices=('classes','unused','shape'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='client-intarrays-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('write','nested','mixed','bytes-codec'):
   _,provider,caller,_=next(r for r in CMD['CASES']['intarraycases']()if r[0]=='intarray-'+case)
   for owner,text,flag in [('library',provider,'-64b'),('client',caller,'-64c')]:
    (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
    base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);ts=T['types'](base[3]);label=case+'-'+owner;rows.append((label+'-valid',P['rebuild'](base),owner,1))
    def add(name,b):rows.append((label+'-'+name,P['rebuild'](b),owner,0))
    def mutate(name,changes):
     b=dict(base);items=[r[-1]for r in code]
     for pc,fields in changes:items[pc]=enc(code[pc],**fields)
     b[2]=W(len(items))+b''.join(items);add(name,b)
    inttype=next(i for i,t in enumerate(ts)if t==[4,4,b'\0'])
    for name,new in [('size',[2,2,b'\0']),('align',[4,1,b'\0']),('map',[4,4,b'\x80']),('wide',[8,8,b'\0']),('zero',[0,1,b'\0'])]:
     types=[t[:]for t in ts];types[inttype]=new;b=dict(base);b[3]=T['typebytes'](types);add('descriptor-'+name,b)
    for name,types in [('missing',ts[:inttype]+ts[inttype+1:]),('duplicate',ts+[[4,4,b'\0']])]:
     b=dict(base);b[3]=T['typebytes'](types);add('descriptor-'+name,b)
    index=next(i for i,r in enumerate(code)if r[0]=='IINDW');ins=code[index];use=code[index+1];scratch=ins[3];read=(use[1]>>3&7)==5;field='source'if read else'dest'
    for name,offset in [('header',40),('owner',ins[2]),('result',scratch+8),('outside',scratch+16),('unaligned',scratch+1)]:mutate('scratch-'+name,[(index,dict(middle=offset)),(index+1,{field:offset})])
    mutate('byte-index',[(index,dict(name='IINDB'))]);mutate('byte-consumer',[(index+1,dict(name='IMOVB'))]);mutate('displaced-consumer',[(index+1,dict(secondary=4))]);mutate('wrong-consumer-slot',[(index+1,{field:scratch+8})]);mutate('context-owner',[(index,dict(source=64 if owner=='library'else 0))]);mutate('root-index',[(index,dict(mode=(ins[1]&~7)|1,dest=ins[2]))])
    for i,r in enumerate(code):
     if r[0]=='INEWAZ'and r[3]==inttype:
      mutate('descriptor-context',[(i,dict(middle=1))]);break
    if case=='mixed'and owner=='library':
     byte=next(i for i,r in enumerate(code[:-1])if r[0]=='IINDB'and code[i+1][0]=='ICVTBW')
     mutate('byte-as-word',[(byte,dict(name='IINDW')),(byte+1,dict(name='IMOVW'))])
     for i,r in enumerate(code):
      if r[0]=='INEWAZ'and r[3]!=inttype:mutate('constructor-class',[(i,dict(middle=inttype))]);break
     types=[t[:]for t in ts];types[-2],types[-1]=types[-1],types[-2];b=dict(base);b[3]=T['typebytes'](types);items=[r[-1]for r in code]
     for i,r in enumerate(code):
      if r[0]=='INEWAZ':items[i]=enc(r,middle=inttype-1 if r[3]==inttype else inttype)
     b[2]=W(len(items))+b''.join(items);add('descriptor-order',b)
    if case=='write'and owner=='library':
     # Remove all consumers/allocators of the appended int type without adding a DATA payload.
     bytecase=next(r for r in CMD['CASES']['indexcases']()if r[0]=='index-read')
     (w/'source.b').write_text(bytecase[1]);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'byte'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
     b=CMD['sections']((w/'byte').read_bytes());b[3]=T['typebytes'](T['types'](b[3])+[[4,4,b'\0']]);add('unused-int',b)
  if a.control:
   chosen={'classes':'mixed-library-byte-as-word','unused':'write-library-unused-int','shape':'write-library-descriptor-align'}[a.control];rows=[r for r in rows if r[0]==chosen];assert rows
   path='dis64-client-managed.h'if a.control=='classes'else'dis64-client-literals.h';text=(ROOT/'include'/path).read_text();old,new={'classes':('actual!=kind','0'),'unused':('!!ints==v->intarrays','1'),'shape':('byte.bytes==4 && byte.align==4','byte.bytes==4')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new));(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,owner,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),owner,str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS int array raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
