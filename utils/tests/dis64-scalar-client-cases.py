"""CFG source pairs; one path/table and scalar interface remain explicit."""
from pathlib import Path
import runpy
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-library-files.py'))
def command(body):return 'implement Client;include "library.m";Client:module{init:fn();};init(){'+body+'}'
def cases():
 provider=BASE['source']('inc:fn(n:int):int;','inc(n:int):int{return n+1;}')
 load='m:=load Library "library.dis64";'
 guard='if(m==nil)return;'
 rows=[
 ('nil-return',load+guard+'last:=m->inc(7);',8),
 ('nil-not-equal',load+'if(nil!=m){last:=m->inc(7);}else return;',8),
 ('early-after-call',load+guard+'a:=m->inc(7);if(a==8)return;last:=m->inc(a);',None),
 ('diamond-left',load+guard+'a:=7;v:int;if(a<10)v=m->inc(a);else v=m->inc(70);last:=m->inc(v);',9),
 ('diamond-right',load+guard+'a:=17;v:int;if(a<10)v=m->inc(a);else v=m->inc(70);last:=m->inc(v);',72),
 ('loop-zero',load+guard+'a:=10;while(a<10){a=m->inc(a);}last:=m->inc(a);',11),
 ('loop-one',load+guard+'a:=9;while(a<10){a=m->inc(a);}last:=m->inc(a);',11),
 ('loop-many',load+guard+'a:=0;while(a<10){a=m->inc(a);}last:=m->inc(a);',11),
 ('loop-reload',load+guard+'a:=0;while(a<4){m=load Library "library.dis64";if(m==nil)return;a=m->inc(a);}last:=m->inc(a);',5),
 ('loop-break',load+guard+'a:=0;while(a<10){a=m->inc(a);if(a==4)break;}last:=m->inc(a);',5),
 ('loop-continue',load+guard+'a:=0;total:=0;while(a<5){a=m->inc(a);if(a<3)continue;total+=a;}last:=m->inc(total);',13),
 ('big-branch',load+guard+'seed:=3;wide:=big seed;boundseed:=4;bound:=big boundseed;v:int;if(wide<bound)v=m->inc(7);else v=m->inc(70);last:=m->inc(v);kept:=wide|bound;',9),
 ('big-high-half',load+guard+'seed:=1;wide:=big seed;wide<<=40;bound:=big seed;v:int;if(wide>bound)v=m->inc(7);else v=m->inc(70);last:=m->inc(v);kept:=wide|bound;',9),
 ('real-negative',load+guard+'seed:=-42;r:=real seed;boundseed:=-43;bound:=real boundseed;v:int;if(r>bound)v=m->inc(7);else v=m->inc(70);last:=m->inc(v);kept:=r;keptbound:=bound;',9),
 ('real-branch',load+guard+'seed:=42;r:=real seed;boundseed:=43;bound:=real boundseed;v:int;if(r<bound)v=m->inc(7);else v=m->inc(70);last:=m->inc(v);kept:=r;keptbound:=bound;',9)]
 for name,body,expected in rows:
  if expected is not None:
   if name=='nil-not-equal':body=body.replace('if(nil!=m){last:=','last:int;if(nil!=m){last=')
   check='if(last!='+str(expected)+'){zero:=0;bad:=1/zero;}'
   if 'kept:='in body:body=body.replace('kept:=',check+'kept:=',1)
   else:body+=check
  yield (name,provider,'inc',0 if expected is None else 1,[1],[7],expected or 0,0,command(body),True)
