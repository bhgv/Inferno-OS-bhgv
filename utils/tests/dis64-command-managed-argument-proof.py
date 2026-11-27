#!/usr/bin/env python3
"""Managed local parameter classes come from callee uses and caller ownership."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));enc=runpy.run_path(str(ROOT/'utils/tests/dis64-command-scalar-result-proof.py'))['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('classes','inference','order'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='command-managed-argument-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('string','string-nil','string-return','string-nested','string-void','list','list-head','list-tail','list-nested','bytes','ints','context','prefix-mixed','prefix-arrays','rebound','bytes-length','ints-length','nil-array-length','packed'):
   _,text,_,_=next(r for r in CMD['CASES']['managedargumentcases']()if r[0]=='managedarg-'+case)
   (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),'-64b','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);types=T['types'](base[3]);rows.append((case+'-valid',P['rebuild'](base),1))
   def add(name,b):rows.append((case+'-'+name,P['rebuild'](b),0))
   def mutate(name,changes):
    b=dict(base);items=[r[-1]for r in code]
    for pc,fields in changes:items[pc]=enc(code[pc],**fields)
    b[2]=W(len(items))+b''.join(items);add(name,b)
   frame=next(i for i,r in enumerate(code)if r[0]=='IFRAME');call=next(i for i in range(frame+1,len(code))if code[i][0]=='ICALL');typ=code[frame][2];entry=code[call][4]
   stores=[i for i in range(frame+1,call)if code[i][0]=='IMOVP'];params=stores[2:]if case.startswith('prefix-')else stores
   for j,pc in enumerate(params):
    ins=code[pc]
    for label,fields in [('offset',dict(secondary=ins[5]+8)),('unaligned',dict(secondary=ins[5]+1)),('header',dict(secondary=32)),('other-frame',dict(dest=48)),('word',dict(name='IMOVW')),('source-header',dict(mode=13,source=32)),('source-context',dict(mode=13,source=64)),('source-list',dict(mode=13,source=72)),('middle',dict(mode=ins[1]|64,middle=0)),('immediate-pointer',dict(mode=21,source=7))]:
     # Context and list inputs may legitimately match these existing roots.
     if label=='source-context'and case=='context' or label=='source-list'and(case.startswith('list')or case=='prefix-mixed'and j==1 or case=='packed'and j==1):continue
     mutate('arg'+str(j)+'-'+label,[(pc,fields)])
    ts=[t[:]for t in types];v=bytearray(ts[typ][2]);bit=ins[5]//8;v[bit//8]&=~(0x80>>(bit%8));ts[typ][2]=bytes(v);b=dict(base);b[3]=T['typebytes'](ts);add('arg'+str(j)+'-unmapped',b)
   if case=='list':mutate('string-as-list',[(params[0],dict(mode=5,source=24))])
   if case=='prefix-arrays':mutate('array-class-swap',[(params[0],dict(source=code[params[1]][2]))])
   if case=='string-void':
    use=next(i for i in range(entry,len(code))if code[i][0]=='ILENC')
    mutate('unknown-input',[(use,dict(name='IMOVW',mode=17,source=3))])
   branches=[i for i,r in enumerate(code)if r[0]in('IJMP','IBEQW','IBNEW','IBEQL','IBNEL','IBEQC','IBNEC')]
   if branches:mutate('branch-argument',[(branches[0],dict(dest=params[0]))])
   if case in('string','list'):
    use=next(i for i in range(entry,len(code))if code[i][0]in('ILENC','ILENL'))
    mutate('length-source-header',[(use,dict(source=32))]);mutate('length-result-offset',[(use,dict(secondary=1))])
  if a.control:
   selected={'classes':'list-string-as-list','inference':'list-valid','order':'string-void-valid'}[a.control];rows=[r for r in rows if r[0]==selected];assert rows
   path='dis64-client-local.h'if a.control in('classes','order')else'dis64-scalar-client.h';text=(ROOT/'include'/path).read_text();old,new={'classes':('!dis64clientrootoperand(slots,*count,USRC(in->mode),in->source,kind,need)','0'),'inference':('v->argkinds[i]=kind;','v->argkinds[i]=Dis64ClientString;'),'order':('for(j=0;j<v->localargc[i];j++)if(dis64clientisroot(v->localargs[i][j]))resultmask|=1U<<i;','/* managed input dependency omitted */')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new))
   if path!='dis64-scalar-client.h':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),'library',str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS managed argument raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
