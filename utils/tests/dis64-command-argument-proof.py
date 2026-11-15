#!/usr/bin/env python3
"""Exact scalar parameter classes, alignment, entry definitions and mapped spans."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));enc=runpy.run_path(str(ROOT/'utils/tests/dis64-client-index-proof.py'))['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('seed','kind'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='command-argument-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('empty','empty-result','int','bytes-packed','big','real','mix','prefix-mix','eight','nested-to-prefix'):
   _,text,_,_=next(r for r in CMD['CASES']['argumentcases']()if r[0]=='args-'+case)
   (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);types=T['types'](base[3]);rows.append((case+'-valid',P['rebuild'](base),1))
   def add(name,b):rows.append((case+'-'+name,P['rebuild'](b),0))
   def mutate(name,changes):
    b=dict(base);items=[r[-1]for r in code]
    for pc,fields in changes:items[pc]=enc(code[pc],**fields)
    b[2]=W(len(items))+b''.join(items);add(name,b)
   frame=next(i for i,r in enumerate(code)if r[0]=='IFRAME');call=next(i for i in range(frame+1,len(code))if code[i][0]=='ICALL');entry=code[call][4];typ=code[frame][2]
   stores=[i for i in range(frame+1,call)if code[i][0]in('IMOVB','IMOVW','IMOVL','IMOVF')and code[i][5]>=64]
   for j,pc in enumerate(stores):
    ins=code[pc]
    for name,fields in [('offset',dict(secondary=ins[5]+1)),('header',dict(secondary=32)),('other-frame',dict(dest=48)),('source-header',dict(mode=13,source=32)),('source-root',dict(mode=13,source=64)),('wide-middle',dict(mode=ins[1]|64,middle=1)),('pointer-kind',dict(name='IMOVP'))]:mutate('arg'+str(j)+'-'+name,[(pc,fields)])
    ts=[t[:]for t in types];v=bytearray(ts[typ][2]);bit=ins[5]//8;v[bit//8]|=0x80>>(bit%8);ts[typ][2]=bytes(v);b=dict(base);b[3]=T['typebytes'](ts);add('arg'+str(j)+'-mapped',b)
   for pc in stores:
    ins=code[pc]
    if ins[0]in('IMOVL','IMOVF'):
     mutate('scalar-kind',[(pc,dict(name='IMOVF'if ins[0]=='IMOVL'else'IMOVL'))]);break
   if stores:
    mutate('missing-argument',[(stores[0],dict(name='IJMP',mode=26,middle=0,source=0,dest=call))])
    ts=[t[:]for t in types];ts[typ][0]=80;b=dict(base);b[3]=T['typebytes'](ts);add('frame-too-small',b)
   mutate('call-root',[(call,dict(dest=0))]);mutate('frame-root',[(frame,dict(source=0))]);mutate('call-other-frame',[(call,dict(source=48))])
   if case=='bytes-packed':mutate('packed-word',[(stores[0],dict(name='IMOVW'))])
   branches=[i for i,r in enumerate(code)if r[0]in('IJMP','IBEQW','IBNEW','IBEQL','IBNEL','IBEQC','IBNEC')]
   if branches and stores:mutate('branch-argument',[(branches[0],dict(dest=stores[0]))])
  if a.control:
   chosen={'seed':'int-valid','kind':'big-scalar-kind'}[a.control];rows=[r for r in rows if r[0]==chosen];assert rows
   text=(ROOT/'include/dis64-scalar-client.h').read_text();old,new={'seed':('seed[0]|=bits[0];seed[1]|=bits[1];','/* numeric entry definitions omitted */'),'kind':('slots[i].kind!=kind','0')}[a.control];assert text.count(old)==1;(w/'dis64-scalar-client.h').write_text(text.replace(old,new))
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),'library',str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS scalar argument raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
