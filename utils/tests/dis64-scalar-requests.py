#!/usr/bin/env python3
"""Check optional .m scalar request bytes/ordinals without changing ordinary -H output."""
import argparse,os,runpy,subprocess,tempfile,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
N=runpy.run_path(str(ROOT/'utils/tests/dis64-library-namespace.py'));S=N['S']
def interface(module,functions,extra=''):
 kinds={0:'void',1:'int',2:'big',3:'real'}
 return module+': module{'+extra+''.join(name+':fn('+','.join('n'+str(i)+':'+kinds[k]for i,k in enumerate(args))+')'+(':'+kinds[result]if result else'')+';'for name,result,args in functions)+'};'
def run(limbo,path,*flags,env):return S['TYPED']['READ']['checked_run']([str(limbo),*flags,str(path)],env=env,capture_output=True,text=True)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--baseline',type=Path);a=p.parse_args();limbo=a.limbo.resolve();env=dict(os.environ)
 with tempfile.TemporaryDirectory(prefix='scalar-requests-',dir=ROOT)as td:
  w=Path(td);env['TMPDIR']=str(w)
  positives=[('empty','Probe',[], 'PATH:con "/p";'),('mixed','Probe',[('get',2,[1,2,3]),('touch',0,[]),('id',3,[3])],''),('functions64','Probe',[('f'+str(i),1,[1])for i in range(64)],''),('parameters64','Probe',[('f',2,[2]*64)],''),('names255','P'*255,[('f'*255,1,[1])],'')]
  for label,module,functions,extra in positives:
   m=w/(label+'.m');m.write_text(interface(module,functions,extra));result=run(limbo,m,'-H','-R','-M',module,env=env);assert result.returncode==0,result.stderr
   data=N['arrays'](result.stdout);actual=N['bindings'](data);expected={(module,name,kind,tuple(args))for name,kind,args in functions};assert set(actual)==expected and len(actual)==len(expected)
   for ordinal,(_,name,_,_)in enumerate(actual):assert re.search(r'^#define LIMBO_NATIVE64_IMPORT_'+module+'_'+name+' '+str(ordinal)+r' ',result.stdout,re.M)
   plain=run(limbo,m,'-H','-M',module,env=env);assert plain.returncode==0 and result.stdout.startswith(plain.stdout)
   if a.baseline:assert run(a.baseline.resolve(),m,'-H','-M',module,env=env).stdout==plain.stdout
   print('PASS scalar .m request boundary:',label,flush=True)
  negatives=[('functions65',interface('Probe',[('f'+str(i),1,[1])for i in range(65)])),('parameters65',interface('Probe',[('f',2,[2]*65)]))]
  for kind in ('byte','fixed(1.0)','string','array of int','list of int','chan of int','(int,int)'):
   negatives.append((kind,'Probe:module{f:fn(n:'+kind+'):int;};'))
  negatives += [('reference','Probe:module{T:adt{n:int;};f:fn(n:ref T):int;};'),('adt','Probe:module{T:adt{n:int;};f:fn(n:T):int;};'),('variadic','Probe:module{f:fn(s:string,*):int;};')]
  for index,(label,text)in enumerate(negatives):
   m=w/('bad-'+str(index)+'.m');m.write_text(text);ordinary=run(limbo,m,'-H','-M','Probe',env=env);assert ordinary.returncode==0,(label,ordinary.stderr)
   result=run(limbo,m,'-H','-R','-M','Probe',env=env);assert result.returncode and not result.stdout and 'native64 scalar requests:'in result.stderr,(label,result.stdout,result.stderr)
   assert 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,result.stderr
  for suffix in ('imports','strings'):
   m=w/('collision-'+suffix+'.m');m.write_text(interface('limbo',[('native64_limbo_scalar_'+suffix,1,[1])]))
   for flags in (('-H','-M','limbo'),('-H','-R','-M','limbo')):
    result=run(limbo,m,*flags,env=env)
    assert result.returncode and not result.stdout and 'reserved metadata prefix'in result.stderr,result.stderr
  print('PASS scalar .m request names: reserved prefixes still refuse with/without -R before C output',flush=True)
  m=w/'cli.m';m.write_text(interface('Probe',[('id',1,[1])]))
  for flags in (['-R'],['-H','-R'],['-R','-M','Probe']):
   result=run(limbo,m,*flags,env=env);assert result.returncode and not result.stdout and 'usage:'in result.stderr,result.stderr
  print('PASS scalar .m requests:',len(negatives),'well-typed unsupported interfaces publish no C bytes; three CLI refusals',flush=True)
if __name__=='__main__':main()
