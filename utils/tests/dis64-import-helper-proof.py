#!/usr/bin/env python3
"""Import roots and entryless helper bodies keep distinct permission/flow rules."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];P=runpy.run_path(str(ROOT/'utils/tests/dis64-command-proof.py'));CMD=P['CMD'];C=P['C'];W=P['W'];T=runpy.run_path(str(ROOT/'utils/tests/dis64-client-literals-proof.py'));enc=runpy.run_path(str(ROOT/'utils/tests/dis64-command-scalar-result-proof.py'))['enc']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--control',choices=('load','root'));a=p.parse_args();env=dict(os.environ);rows=[]
 with tempfile.TemporaryDirectory(prefix='import-helper-proof-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);(w/'command.m').write_text(CMD['CASES']['INTERFACE'])
  for case in ('before','after','byte','nested','loop','array','string','rebound','construct','literal','context'):
   _,_,text,_=next(r for r in CMD['CASES']['importhelpercases']()if r[0]=='importhelper-'+case)
   (w/'source.b').write_text(text);r=subprocess.run([str(a.limbo.resolve()),'-64c','-I',str(ROOT/'module'),'-I',str(w),'-o',str(w/'image'),str(w/'source.b')],env=env,capture_output=True,text=True);assert not r.returncode,r.stderr
   base=CMD['sections']((w/'image').read_bytes());code=C.decode(base[2],managed=True);types=T['types'](base[3]);rows.append((case+'-valid',P['rebuild'](base),1))
   def mutate(name,changes):
    b=dict(base);items=[r[-1]for r in code]
    for pc,fields in changes:items[pc]=enc(code[pc],**fields)
    b[2]=W(len(items))+b''.join(items);rows.append((case+'-'+name,P['rebuild'](b),0))
   frame=next(i for i,r in enumerate(code)if r[0]=='IFRAME');call=next(i for i in range(frame+1,len(code))if code[i][0]=='ICALL');entry=code[call][4];load=next(i for i,r in enumerate(code)if r[0]=='ILOAD');binder=next(i for i in range(frame+1,call)if code[i][0]=='ILEA')
   for name,changes in [('frame-root',[(frame,dict(source=0))]),('frame-header',[(frame,dict(dest=48))]),('entry-root',[(call,dict(dest=0))]),('entry-interior',[(call,dict(dest=entry+1))]),('call-header',[(call,dict(source=48))]),('argument-header',[(frame+1,dict(secondary=32))]),('argument-frame',[(frame+1,dict(dest=48))]),('result-header',[(binder,dict(source=32))]),('result-cell',[(binder,dict(secondary=40))]),('helper-exit',[(len(code)-1,dict(name='IEXIT'))]),('missing-load',[(load,dict(name='IJMP',mode=26,source=0,middle=0,dest=load+1))])]:
    if name.startswith('argument-')and binder==frame+1:continue
    mutate(name,changes)
   exit=next(i for i,r in enumerate(code)if r[0]=='IEXIT');mutate('root-ret',[(exit,dict(name='IRET'))])
   mutate('helper-load',[(len(code)-1,dict(name='ILOAD',mode=64,source=16,middle=0,dest=0))])
   branches=[i for i,r in enumerate(code)if i<entry and r[0]in('IJMP','IBEQW','IBNEW','IBEQL','IBNEL','IBEQC','IBNEC')]
   if branches:
    for name,target in [('cross',entry),('setup',frame+1),('binder',binder),('call',call)]:mutate('branch-'+name,[(branches[0],dict(dest=target))])
   typ=code[frame][2];ts=[t[:]for t in types];ts.append(ts[typ]);b=dict(base);b[3]=T['typebytes'](ts);rows.append((case+'-unused-frame',P['rebuild'](b),0))
  if a.control:
   chosen={'load':'before-missing-load','root':'before-valid'}[a.control];rows=[r for r in rows if r[0]==chosen];assert rows
   path='dis64-scalar-client.h'if a.control=='load'else'dis64-client-local.h';text=(ROOT/'include'/path).read_text();old,new={'load':('node->needload=1;ordinal=in->middle;','node->needload=0;ordinal=in->middle;'),'root':('dis64clientbodyprove(&body,i || !v->module.profile,1)','dis64clientbodyprove(&body,1,1)')}[a.control];assert text.count(old)==1;(w/path).write_text(text.replace(old,new))
   if path!='dis64-scalar-client.h':(w/'dis64-scalar-client.h').write_text((ROOT/'include/dis64-scalar-client.h').read_text())
  binary=w/'proof';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'));subprocess.run(cc+['-I'+str(w),'-I'+str(ROOT/'include'),str(ROOT/'utils/tests/dis64-command-proof.c'),'-o',str(binary)],env=env,check=True)
  for name,data,expected in rows:
   (w/'test.bin').write_bytes(data);r=subprocess.run(shlex.split(env.get('NATIVE64_RUNNER',''))+[str(binary),str(w/'test.bin'),'client',str(expected)],env=env,capture_output=True,text=True);assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr);assert (r.returncode==10 and 'ordinary init raw proof mismatch'in r.stderr)if a.control else r.returncode==0,(name,r.returncode,r.stderr)
  print('PASS importing helper raw proof:',env.get('NATIVE64_ARCH','amd64'),a.control or str(len(rows))+' images',flush=True)
if __name__=='__main__':main()
