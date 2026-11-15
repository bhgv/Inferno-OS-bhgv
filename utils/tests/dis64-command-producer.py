#!/usr/bin/env python3
"""Ordinary-init determinism, classic compatibility and producer refusal boundaries."""
import argparse,os,re,runpy,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
P=runpy.run_path(str(ROOT/'utils/tests/dis64-scalar-client.py'))
C=runpy.run_path(str(ROOT/'utils/tests/dis64-command-cases.py'))
D=runpy.run_path(str(ROOT/'utils/tests/dis64-command.py'))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);p.add_argument('--baseline',type=Path);p.add_argument('--command-baseline',type=Path);p.add_argument('--field-baseline',type=Path);p.add_argument('--reference-baseline',type=Path);p.add_argument('--literal-baseline',type=Path);p.add_argument('--codec-baseline',type=Path);p.add_argument('--array-baseline',type=Path);p.add_argument('--index-baseline',type=Path);p.add_argument('--int-array-baseline',type=Path);p.add_argument('--local-baseline',type=Path);p.add_argument('--result-baseline',type=Path);p.add_argument('--argument-baseline',type=Path);a=p.parse_args();env=dict(os.environ);env["ROOT"]=str(ROOT);env.pop("EMU",None)
 with tempfile.TemporaryDirectory(prefix='command-producer-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);source=w/'source.b';image=w/'new.dis64';old=w/'old.dis64';plain=w/'plain.dis';oldplain=w/'old.dis'
  def compile(compiler,flag,target,check=True):
   r=subprocess.run([str(compiler.resolve()),flag,'-I',str(ROOT/'module'),'-I',str(w),'-o',str(target),str(source)],env=env,capture_output=True,text=True)
   if check:assert not r.returncode,(source.read_text(),flag,r.stderr)
   return r
  prior=list(P['cases']())
  for name in ('cases','numeric-cases','managed-cases'):prior+=list(runpy.run_path(str(ROOT/('utils/tests/dis64-scalar-client-'+name+'.py')))['cases']())
  for row in prior:
   source.write_text(row[8]);(w/'library.m').write_text(P['NS']['declarations'](row[1]));compile(a.limbo,'-64c',image);compile(a.limbo,'-i',plain)
   if a.baseline or a.codec_baseline or a.array_baseline or a.index_baseline or a.int_array_baseline or a.local_baseline or a.result_baseline or a.argument_baseline:
    priorcompiler=a.baseline or a.codec_baseline or a.array_baseline or a.index_baseline or a.int_array_baseline or a.local_baseline or a.result_baseline or a.argument_baseline;compile(priorcompiler,'-64c',old);compile(priorcompiler,'-i',oldplain);assert image.read_bytes()==old.read_bytes()and plain.read_bytes()==oldplain.read_bytes(),row[0]
  (w/'command.m').write_text(C['INTERFACE']);count=0
  for name,provider,caller,_ in C['cases']():
   for text,flag in ((provider,'-64b'),(caller,'-64c')):
    source.write_text(text);compile(a.limbo,flag,image);data=image.read_bytes();compile(a.limbo,flag,image);assert data==image.read_bytes(),name;compile(a.limbo,'-i',plain)
    if name.startswith('literals-'):
     ps=D['sections'](data)[7];at=4;values=[]
     for item in range(int.from_bytes(ps[:4],'big')):
      nr=int.from_bytes(ps[at:at+4],'big');values.append(''.join(chr(int.from_bytes(ps[j:j+4],'big'))for j in range(at+4,at+4+4*nr,4)));at+=4+4*nr
     assert at==len(ps)
     expected={'ascii':{'hello'},'unicode':{'Привет'},'supplementary':{'a🚀z'},'nul':{'a\0z'},'empty':set(),'arguments':{'wm','-font','你好'},'order':{'a','aé'},'nil-empty':set(),'nul-compare':{'a\0z','a\0y'},'loop':{'a🚀z'},'index-negative':{'text'},'index-end':{'text'},'index-nil':set(),'index-empty':set(),'context':{'owned'},'private-list':{'private'},'caller':{'wm'},'reload':{'reload'},'long':{'é'*255},'duplicates':{'same'},'count-limit':{'v%02d'%j for j in range(32)},'total-limit':{'a'*255,'b'*255,'c'*255,'d'*255,'tail'}}[name[len('literals-'):]] if flag=='-64b'else({'wm','-font','你好','caller🚀'}if name=='literals-caller'else{'wm','-font','你好'})
     assert len(values[4:])==len(expected)and set(values[4:])==expected,(name,flag,values[4:],expected)
     if name=='literals-count-limit'and flag=='-64b':assert int.from_bytes(D['sections'](data)[1][8:12],'big')==280
    if a.baseline:
     compile(a.baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.baseline,flag,old,False);assert r.returncode and(not old.exists()or not old.stat().st_size),(name,r.stderr)
    if a.command_baseline:
     compile(a.command_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.command_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith(('fields-','branches-','constructors-','literals-','strings-','codecs-','arrays-'))and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    if a.field_baseline:
     compile(a.field_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.field_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith(('branches-','constructors-','literals-','strings-','codecs-','arrays-'))and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    if a.reference_baseline:
     compile(a.reference_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.reference_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith(('literals-','strings-','codecs-','arrays-'))and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    if a.literal_baseline:
     compile(a.literal_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.literal_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith(('strings-','codecs-','arrays-'))and(not old.exists()or not old.stat().st_size),(name,r.stderr)
    if a.codec_baseline:
     compile(a.codec_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.codec_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith(('codecs-','arrays-'))and(not old.exists()or not old.stat().st_size),(name,r.stderr)
    if a.array_baseline:
     compile(a.array_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.array_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith('arrays-')and(not old.exists()or not old.stat().st_size),(name,r.stderr)
    if a.index_baseline:
     compile(a.index_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.index_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith('index-')and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    if a.int_array_baseline:
     compile(a.int_array_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.int_array_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith('intarray-')and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    if a.local_baseline:
     compile(a.local_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.local_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith('local-')and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    if a.result_baseline:
     compile(a.result_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.result_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith('result-')and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    if a.argument_baseline:
     compile(a.argument_baseline,'-i',oldplain);assert plain.read_bytes()==oldplain.read_bytes(),name
     old.unlink(missing_ok=True);r=compile(a.argument_baseline,flag,old,False)
     if not r.returncode:assert old.read_bytes()==data,(name,r.stderr)
     else:assert name.startswith('args-')and(not old.exists()or not old.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.stderr)
    count+=1
  prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
  negatives=[prefix+'d:=ctxt.display;img:=d.image;if(img!=nil)return;}',prefix+'local:=ref Draw->Display;if(local!=nil)return;}',prefix+'a:=array[1]of string;a[0]="one";if(len a==0)return;}',prefix+'xs:=array[2]of int;xs[0]=1;if(xs[0]==1)return;}']
  for text in negatives:
   source.write_text(text);compile(a.limbo,'-i',plain);image.unlink(missing_ok=True);r=compile(a.limbo,'-64b',image,False)
   assert r.returncode and 'Dis64 command:'in r.stderr and(not image.exists()or not image.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  for declarations,body,helpers in [
   ('helper:fn(s:string);','helper("unsupported");','helper(s:string){}'),
   ('helper:fn(n:int):int;','n:=helper(1);','helper(n:int):int{return n;}'),
   ('helper:fn(ctxt:ref Draw->Context,argv:list of string);','helper(ctxt,argv);','helper(ctxt:ref Draw->Context,argv:list of string){helper(ctxt,argv);}'),
   ('helper:fn(ctxt:ref Draw->Context,argv:list of string);','helper(ctxt,argv);','helper(ctxt:ref Draw->Context,argv:list of string){sys:=load Sys Sys->PATH;}')]:
   source.write_text('implement Command;include "command.m";'+declarations+'init(ctxt:ref Draw->Context,argv:list of string){'+body+'}'+helpers)
   compile(a.limbo,'-i',plain);image.unlink(missing_ok=True);r=compile(a.limbo,'-64b',image,False);assert r.returncode and(not image.exists()or not image.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  declarations=''.join('h'+str(i)+':fn(ctxt:ref Draw->Context,argv:list of string);'for i in range(9))
  helpers=''.join('h'+str(i)+'(ctxt:ref Draw->Context,argv:list of string){'+('h'+str(i+1)+'(ctxt,argv);'if i<8 else'')+'}'for i in range(9))
  source.write_text('implement Command;include "command.m";'+declarations+'init(ctxt:ref Draw->Context,argv:list of string){h0(ctxt,argv);}'+helpers)
  compile(a.limbo,'-i',plain);image.unlink(missing_ok=True);r=compile(a.limbo,'-64b',image,False);assert r.returncode and(not image.exists()or not image.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  params=','.join('n'+str(i)+':int'for i in range(9));values=','.join(str(i)for i in range(9))
  source.write_text('implement Command;include "command.m";helper:fn('+params+');init(ctxt:ref Draw->Context,argv:list of string){helper('+values+');}helper('+params+'){}')
  compile(a.limbo,'-i',plain);image.unlink(missing_ok=True);r=compile(a.limbo,'-64b',image,False);assert r.returncode and(not image.exists()or not image.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  for values in [['v%02d'%j for j in range(33)],['x'*256],['a'*255,'b'*255,'c'*255,'d'*255,'tails']]:
   body=prefix+'s:string;total:=0;'+''.join('s="'+v+'";total+=len s;'for v in values)+'if(total==0)return;}'
   source.write_text(body);compile(a.limbo,'-i',plain);image.unlink(missing_ok=True);r=compile(a.limbo,'-64b',image,False)
   assert r.returncode and(not image.exists()or not image.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  # A constructor inside prepared call setup is still outside the exact island.
  text=next(r[2]for r in C['constructorcases']()if r[0]=='constructors-local').replace('ctxt:=ref Draw->Context(nil,nil,nil);m->init(ctxt,argv);m->init(ctxt,argv);keepctxt:=ctxt;','m->init(ref Draw->Context(nil,nil,nil),argv);')
  source.write_text(text);compile(a.limbo,'-i',plain);image.unlink(missing_ok=True);r=compile(a.limbo,'-64c',image,False)
  assert r.returncode and(not image.exists()or not image.stat().st_size)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  examples=[re.findall(r'```limbo\n(.*?)```',(ROOT/('doc/dis64-command-init'+language+'.md')).read_text(),re.S)for language in ('','.ru')]
  assert len(examples[0])==3 and examples[0]==examples[1]
  for name,text in zip(('command.m','library.b','client.b'),examples[0]):(w/name).write_text(text)
  for flag,name in [('-64b','library.b'),('-64c','client.b')]:
   source.write_text((w/name).read_text());compile(a.limbo,flag,image)
 print('PASS ordinary init producer:',count,'deterministic files;',len(prior),'prior clients; fourteen well-typed source refusals; RU/EN tutorial compiled'+('; prior Dis64/all classic bytes equal'if a.baseline or a.codec_baseline or a.array_baseline or a.index_baseline or a.int_array_baseline or a.local_baseline or a.result_baseline or a.argument_baseline else'; no baseline'),flush=True)
if __name__=='__main__':main()
