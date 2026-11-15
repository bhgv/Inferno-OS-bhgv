"""Numeric import results compose with local CALLs and remapped helper entries."""
from pathlib import Path
import runpy
ROOT=Path(__file__).resolve().parents[2];BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-library-files.py'))
def cases():
 provider=BASE['source']('inc:fn(n:int):int;','inc(n:int):int{return n+1;}')
 load='m:=load Library "library.dis64";if(m==nil)return;'
 bad='{zero:=0;bad:=1/zero;}'
 rows=[
 ('before','n:int','int','a:=first(7);'+load+'last:=m->inc(a);',9,'return n+1;',[]),
 ('after','n:int','int',load+'a:=m->inc(7);b:=first(a);last:=m->inc(b);',10,'return n+1;',[]),
 ('branch','n:int','int',load+'a:=m->inc(7);v:int;if(a==8)v=first(a);else v=first(70);last:=m->inc(v);',10,'return n+1;',[]),
 ('loop','n:int','int',load+'a:=m->inc(0);while(a<4)a=first(a);last:=m->inc(a);',5,'return n+1;',[]),
 ('nested','n:int','int',load+'a:=m->inc(7);b:=first(a);last:=m->inc(b);',10,'return second(n);',[('second','n:int','int','return n+1;')]),
 ('byte','n:int','byte',load+'b:=first(511);n:=int b;last:=m->inc(n);',256,'return byte n;',[]),
 ('big','n:int','big',load+'b:=first(7);n:=int b;last:=m->inc(n);',8,'return big n;',[]),
 ('string-input','s:string','int',load+'n:=first("abc");last:=m->inc(n);',4,'return len s;',[]),
 ('real','n:int','real',load+'r:=first(7);n:=int r;last:=m->inc(n);',8,'return real n;',[])]
 # Keep the ordinary int provider last for the full-console wrong-result control.
 for kind,ty,expected in [(2,'big',30),(3,'real',4620693217682128896)]:
  text=BASE['source']('inc:fn(n:'+ty+'):'+ty+';','inc(n:'+ty+'):'+ty+'{return '+('n+n'if kind==2 else'n')+';}')
  caller='implement Client;include "library.m";Client:module{init:fn();};first:fn(n:'+ty+'):'+ty+';init(){'+load+'n:=7;x:='+ty+' n;a:=m->inc(x);b:=first(a);last:=m->inc(b);e:='+('30'if kind==2 else'8')+';expected:='+ty+' e;if(last!=expected)'+bad+'}first(n:'+ty+'):'+ty+'{v:=1;one:='+ty+' v;return n+one;}'
  yield ('import-local-'+ty+'-import',text,'inc',kind,[kind],[7],expected,0,caller,True)
 for name,params,result,body,expected,helper,others in rows:
  functions=[('first',params,result,helper)]+others
  declarations=''.join(fn+':fn('+ps+'):'+ty+';'for fn,ps,ty,_ in functions)
  definitions=''.join(fn+'('+ps+'):'+ty+'{'+code+'}'for fn,ps,ty,code in functions)
  caller='implement Client;include "library.m";Client:module{init:fn();};'+declarations+'init(){'+body+'if(last!='+str(expected)+')'+bad+'}'+definitions
  yield ('import-local-'+name,provider,'inc',1,[1],[7],expected,0,caller,True)
