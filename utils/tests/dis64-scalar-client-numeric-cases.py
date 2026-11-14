"""Source-owned numeric continuations around actual file LOAD/MCALL boundaries."""
from pathlib import Path
import runpy,struct
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-library-files.py'))
def cases():
 provider=BASE['source']('inc:fn(n:int):int;','inc(n:int):int{return n+1;}')
 rows=[]
 for name,expression,expected in [('add','x+y',13),('subtract','x-y',7),('multiply','x*y',30),('divide','x/y',3)]:
  rows.append(('real-'+name,'a:=10;b:=3;x:=real a;y:=real b;r:='+expression+';n:=int r;',expected,'keepx:=x;keepy:=y;keepr:=r;',0))
 rows += [
  ('real-negate','a:=7;x:=real a;r:=-x;n:=int r;',-7,'keepx:=x;keepr:=r;',0),
  ('real-in-place','a:=10;b:=3;x:=real a;y:=real b;x+=y;x-=y;x*=y;x/=y;n:=int x;',10,'keepx:=x;keepy:=y;',0),
  ('real-round-positive','a:=3;b:=2;x:=real a;y:=real b;r:=x/y;n:=int r;',2,'keepx:=x;keepy:=y;keepr:=r;',0),
  ('real-round-negative','a:=-3;b:=2;x:=real a;y:=real b;r:=x/y;n:=int r;',-2,'keepx:=x;keepy:=y;keepr:=r;',0),
  ('big-word-low-half','a:=1;w:=big a;w<<=40;b:=7;z:=big b;w+=z;n:=int w;',7,'keepw:=w;keepz:=z;',0),
  ('big-word-negative','a:=-7;w:=big a;n:=int w;',-7,'keepw:=w;',0),
  ('big-real-big','a:=1;w:=big a;w<<=40;r:=real w;z:=big r;z>>=40;n:=int z;',1,'keepw:=w;keepr:=r;keepz:=z;',0),
  ('real-big-positive','a:=3;b:=2;x:=real a;y:=real b;r:=x/y;w:=big r;n:=int w;',2,'keepx:=x;keepy:=y;keepr:=r;keepw:=w;',0),
  ('real-big-negative','a:=-3;b:=2;x:=real a;y:=real b;r:=x/y;w:=big r;n:=int w;',-2,'keepx:=x;keepy:=y;keepr:=r;keepw:=w;',0),
  ('word-min-real-roundtrip','a:=1;a<<=31;r:=real a;n:=int r;n>>=31;',-1,'keepr:=r;',0),
  ('real-word-range-error','a:=1;w:=big a;w<<=40;r:=real w;n:=123;n=int r;',0,'keepw:=w;keepr:=r;',2),
  ('real-big-range-error','a:=1;w:=big a;w<<=40;r:=real w;r*=r;canary:=123;z:=big canary;z=big r;n:=int z;',0,'keepw:=w;keepr:=r;keepz:=z;',2)]
 for name,body,expected,keep,failure in rows:
  # Keep a completed call before the continuation and another after it.
  text='implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";if(m==nil)return;first:=m->inc(7);'+body+'arg:=n+100;last:=m->inc(arg);'
  text+='if(last!='+str(expected+101)+'){zero:=0;bad:=1/zero;}'+keep+'}'
  yield(name,provider,'inc',1,[1],[7],expected+101,failure,text,True)

 # Provider results feed arithmetic and conversions before another typed MCALL.
 realprovider=BASE['source']('id:fn(n:real):real;','id(n:real):real{return n;}')
 text='implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";if(m==nil)return;a:=7;r:=real a;first:=m->id(r);sum:=first+r;last:=m->id(sum);e:=14;expected:=real e;if(last!=expected){zero:=0;bad:=1/zero;}keepr:=r;keepfirst:=first;keepsum:=sum;}'
 yield('real-provider-continuation',realprovider,'id',3,[3],[7.0],struct.unpack('>Q',struct.pack('>d',14.0))[0],0,text,True)
 bigprovider=BASE['source']('twice:fn(n:big):big;','twice(n:big):big{return n+n;}')
 text='implement Client;include "library.m";Client:module{init:fn();};init(){m:=load Library "library.dis64";if(m==nil)return;a:=7;w:=big a;first:=m->twice(w);r:=real first;r+=r;converted:=big r;last:=m->twice(converted);e:=56;expected:=big e;if(last!=expected){zero:=0;bad:=1/zero;}keepw:=w;keepfirst:=first;keepr:=r;keepconverted:=converted;}'
 yield('big-provider-conversion',bigprovider,'twice',2,[2],[7],56,0,text,True)
