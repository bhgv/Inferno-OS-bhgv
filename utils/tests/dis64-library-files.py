#!/usr/bin/env python3
"""Compile whole scalar libraries, match structural imports and execute real MCALLs."""
import argparse,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
S=runpy.run_path(str(ROOT/'utils/tests/dis64-source-calls.py'));C=S['CALLS']
def source(decls,body):return 'implement Library;Library:module{'+decls+'};'+body

def cases():
 return [
 ('empty',source('touch:fn();','touch(){}'),'touch',0,[],[],0,0),
 ('single',source('id:fn(n:int):int;','id(n:int):int{return n;}'),'id',1,[1],[7],7,0),
 ('first-called',source('leaf:fn(n:int):int;mid:fn(n:int):int;','leaf(n:int):int{return n+1;}mid(n:int):int{return leaf(n);}'),'mid',1,[1],[7],8,0),
 ('first-caller',source('mid:fn(n:int):int;leaf:fn(n:int):int;','mid(n:int):int{a:=leaf(n);return leaf(a);}leaf(n:int):int{return n+1;}'),'mid',1,[1],[7],9,0),
 ('deep',source('mid:fn(n:int):int;next:fn(n:int):int;leaf:fn(n:int):int;','mid(n:int):int{return next(n);}next(n:int):int{return leaf(n);}leaf(n:int):int{return n+1;}'),'mid',1,[1],[7],8,0),
 ('loop',source('sum:fn(n:int):int;','sum(n:int):int{a:=0;while(n>0){a+=n;n--;}return a;}'),'sum',1,[1],[7],28,0),
 ('branch',source('abs:fn(n:int):int;','abs(n:int):int{if(n<0)return -n;return n;}'),'abs',1,[1],[-7],7,0),
 ('big',source('mid:fn(n:big):big;leaf:fn(n:big):big;','mid(n:big):big{return leaf(n);}leaf(n:big):big{return n+n;}'),'mid',2,[2],[-3],(-6)&((1<<64)-1),0),
 ('real',source('mid:fn(n:real):real;leaf:fn(n:real):real;','mid(n:real):real{return leaf(n);}leaf(n:real):real{return n;}'),'mid',3,[3],[42.0],struct.unpack('>Q',struct.pack('>d',42.0))[0],0),
 ('mixed',source('mid:fn(a:int,b:big,c:real):big;leaf:fn(a:int,b:big,c:real):big;','mid(a:int,b:big,c:real):big{return leaf(a,b,c);}leaf(a:int,b:big,c:real):big{return b;}'),'mid',2,[1,2,3],[7,-3,42.0],(-3)&((1<<64)-1),0),
 ('void',source('touch:fn(n:int);leaf:fn(n:int);','touch(n:int){leaf(n);}leaf(n:int){a:=n+1;}'),'touch',0,[1],[7],0,0),
 ('no-init-execution',source('init:fn();id:fn(n:int):int;','init(){one:=1;zero:=0;a:=one/zero;}id(n:int):int{return n;}'),'id',1,[1],[7],7,0),
 ('explicit-init-call',source('init:fn();id:fn(n:int):int;','init(){one:=1;zero:=0;a:=one/zero;}id(n:int):int{return n;}'),'init',0,[],[],0,1),
 ('fault',source('mid:fn(n:int):int;leaf:fn(n:int):int;','mid(n:int):int{return leaf(n);}leaf(n:int):int{one:=1;return one/n;}'),'mid',1,[1],[0],0,1)]

def refusals():
 rows=[]
 for kind in ('byte','string','array of int','list of int','(int,int)'):
  rows.append(source('id:fn(n:'+kind+'):'+kind+';','id(n:'+kind+'):'+kind+'{return n;}'))
 rows += [source('g:int;id:fn(n:int):int;','id(n:int):int{return n+g;}'),
  source('id:fn(n:int):int;','id(n:int):int{return id(n);}'),
  source('mid:fn(n:int):int;id:fn(n:int):int;','mid(n:int):int{if(n>0)return id(n);return n;}id(n:int):int{return n;}'),
  source('id:fn(n:int):int;','id(n:int):int{return hidden(n);}hidden(n:int):int{return n;}'),
  'implement Library;Other:module{touch:fn();};Library:module{id:fn(p:Other,n:int):int;};id(p:Other,n:int):int{return n;}']
 return rows

def pool(names):return C.words(len(names))+b''.join(C.words(len(n),*map(ord,n))for n in names)
def request(name,result,args,module='Library'):
 sig=C.words(1,result,len(args),*args);return C.words(1,0,1,len(sig))+sig,pool([module,name])

def allrequests(data,name,result,args):
 s=S['sections'](data);r=C.BASE.Reader(s[6]);records=[];nr=C.BASE.Reader(s[7]);names=[''.join(chr(nr.word())for _ in range(nr.word()))for _ in range(nr.word())]
 for _ in range(r.word()):
  symbol,pc,frame,length=[r.word()for _ in range(4)];sig=r.take(length);records.append((names[symbol],sig))
 chosen=next(x for x in records if x[0]==name);assert chosen[1]==C.words(1,result,len(args),*args)
 ordered=[chosen]+[x for x in reversed(records)if x[0]!=name]
 return C.words(len(ordered))+b''.join(C.words(0,i+1,len(sig))+sig for i,(_,sig)in enumerate(ordered)),pool(['Library']+[n for n,_ in ordered])

def unit():
 s=S['unit']();s=s[:s.index('/* Read immutable fixture bytes')];extract=S['FILES']['FILES']['function'];xec=(ROOT/'libinterp/xec.c').read_text()
 loader=(ROOT/'libinterp/load64.c').read_text();begin=loader.index('typedef struct Dis64LibraryBuild');end=loader.index('/* Consume a staging lease under VM exclusion',begin);s+='\n'+loader[begin:end]+'\n'
 # A library has no EXIT opcode; RX dispatch is generated only for its used leaves.
 s=s.replace('require(module->jit->dispatch[IEXIT] != nil);','if(module->entry!=nil)require(module->jit->dispatch[IEXIT] != nil);',1)
 pin=extract(s,'void\nptradd(');s=s.replace(pin,'static int librarypinfail;\n'+pin.replace('\n{','\n{\n    if(librarypinfail)error(exNomem);',1),1)
 s+='\n'+extract(xec,'int\nnative64diallea8step(')+'\n'+extract(xec,'int\nnative64stringlea8step(')+'\n'
 s=s.replace(extract(s,'int\nnative64typedlea8step('),'',1)
 s+='\n#include \"native64-local-bind.h\"\n'+extract(xec,'int\nnative64typedlea8step(')+'\n'+extract(xec,'static int\nmoduleentry(')+'\n'+extract(xec,'OP(mframe)')+'\n'+extract(xec,'OP(mcall)')+'\n'
 fixture=(ROOT/'utils/tests/dis64-library-files.c').read_text().replace('static int librarypinfail;','')
 fixture=fixture.replace('LIBRARY_HANDLERS','\n'.join('optab[I'+n.upper()+']=actual_'+n+';'for n in ('cvtwl','subw','mulw','modw','andw','orw','xorw','shrw','beqw','bnew','bltw','blew','bgtw','bgew','jmp','addl','subl','mull','divl','modl','andl','orl','xorl','shll','shrl','beql','bnel','bltl','blel','bgtl','bgel','beqf','bnef','bltf','blef','bgtf','bgef')))
 return s+fixture

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--baseline',type=Path);p.add_argument('--materialize-only',action='store_true');p.add_argument('--control',choices=('module','signature','entry','atomic'));a=p.parse_args();env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
 with tempfile.TemporaryDirectory(prefix='library-files-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w);rows=[]
  for name,text,fn,result,args,values,expected,failure in cases():
   b=w/(name+'.b');b.write_text(text);image=w/(name+'.dis64');cmd=[str(a.limbo.resolve()),'-64b','-o',str(image),str(b)]
   S['TYPED']['READ']['checked_run'](cmd,env=env,capture_output=True,check=True)
   _,report=S['TYPED']['compile_source'](a.limbo.resolve(),w,None,filename=b,multi=True);S['verify'](image.read_bytes(),report,library=True)
   again=w/'again.dis64';S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-64b','-i','-o',str(again),str(b)],env=env,capture_output=True,check=True);assert image.read_bytes()==again.read_bytes()
   if 'init:'not in text:
    S['TYPED']['READ']['checked_run']([str(a.limbo.resolve()),'-64c','-o',str(again),str(b)],env=env,capture_output=True,check=True);assert image.read_bytes()==again.read_bytes()
   if a.baseline:
    old=w/'baseline.dis';S['TYPED']['READ']['checked_run']([str(a.baseline.resolve()),'-i','-o',str(old),str(b)],env=env,capture_output=True,check=True);assert old.read_bytes()==(w/'plain.dis').read_bytes()
   requests,strings=allrequests(image.read_bytes(),fn,result,args);(w/(name+'.imports')).write_bytes(requests);(w/(name+'.strings')).write_bytes(strings)
   argv=[str(image),str(w/(name+'.imports')),str(w/(name+'.strings')),str(result),str(expected),str(failure)]+[str(struct.unpack('>Q',struct.pack('>d',v))[0]if k==3 else v&((1<<64)-1))for k,v in zip(args,values)]
   rows.append((name,argv))
  for i,text in enumerate(refusals()):
   b=w/('refuse-'+str(i)+'.b');b.write_text(text);image=w/'refused.dis64'
   r=subprocess.run([str(a.limbo.resolve()),'-64b','-o',str(image),str(b)],env=env,capture_output=True,text=True)
   assert r.returncode and 'Dis64 calls:'in r.stderr and (not image.exists()or image.stat().st_size==0),(i,r.returncode,r.stderr)
   assert 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,r.stderr
  print('PASS scalar library source boundary: ten unsupported whole libraries emit no payload',flush=True)
  file=w/'library.c';file.write_text(unit());binary=w/'library';cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch=='arm64':cc+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
  elif arch=='riscv64':cc+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
  elif arch!='amd64':raise RuntimeError('Unsupported target '+arch)
  cc+=['-I'+str(w),'-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  if a.control:
   if a.control=='entry':
    text=file.read_text();before='if(work->image.module.profile)module->entry = module->prog+work->image.module.entry;';assert text.count(before)==1;file.write_text(text.replace(before,'module->entry = module->prog+work->image.module.entry;'))
   else:
    text=(ROOT/'include/dis64-call-library.h').read_text()
    before,after={'module':('|| order || !disstringpoolget(&pool,in.symbol,&symbol)','|| 0 || !disstringpoolget(&pool,in.symbol,&symbol)'),
     'signature':('if(!dis64signatureequal(&in.signature,&out.signature,&equal) || !equal)','if(0)'),
     'atomic':('links.entry[i]=out.entry;','links.entry[i]=out.entry;*output=links;')}[a.control]
    assert text.count(before)==1;(w/'dis64-call-library.h').write_text(text.replace(before,after))
   rows=[x for x in rows if x[0]==('deep'if a.control=='atomic'else'single')]
  cc+=[str(file),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)];subprocess.run(cc,env=env,check=True)
  runner=shlex.split(env.get('NATIVE64_RUNNER',''))
  for name,argv in rows:
   r=subprocess.run(runner+[str(binary)]+argv+(['materialize']if a.materialize_only else[]),env=env,capture_output=True,text=True,timeout=120)
   if a.control:
    diagnostic={'module':'library semantic failure: exact import identity','signature':'library semantic failure: exact import identity','entry':'library semantic failure: no automatic init','atomic':'memcmp(&plan,&original,sizeof(plan))==0'}[a.control]
    assert r.returncode==10 and diagnostic in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(a.control,r.returncode,r.stderr)
    print('PASS scalar library active predicate control:',arch,a.control,flush=True);return
   assert r.returncode==0,(name,r.returncode,r.stderr);print('PASS scalar library:',arch,name,flush=True)
  print('PASS scalar library source/matching/ownership pipeline:',arch,len(rows),'whole libraries',flush=True)
if __name__=='__main__':main()
