"""Three actual compiler files: executable -> importing Command -> dependency."""
INTERFACE='include "sys.m";include "draw.m";include "sh.m";'
def cases():
 bad='{z:=0;bad:=1/z;}'
 args='ctxt:ref Draw->Context,argv:list of string'
 caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;xs:="wm"::"abc"::nil;m->init(nil,xs);m->init(nil,xs);if(len xs!=2)'+bad+'}'
 library='Library:module{inc:fn(n:int):int;};'
 scalar='implement Library;'+library+'inc(n:int):int{return n+1;}'
 root='implement Command;include "command.m";include "library.m";'
 load='m:=load Library "dependency.dis64";if(m==nil)'+bad
 rows=[
 ('required',load.replace('if(m==nil)return;','if(m==nil)'+bad)+'n:=m->inc(7);if(n!=8)'+bad,''),
 ('scalar',load+'n:=m->inc(7);if(n!=8)'+bad,''),
 ('context', 'if(ctxt!=nil || len argv!=2)'+bad+load+'n:=len argv;v:=m->inc(n);if(v!=3)'+bad,''),
 ('branch',load+'n:int;if(len argv==2)n=m->inc(7);else n=m->inc(70);if(n!=8)'+bad,''),
 ('loop',load+'n:=0;while(n<3)n=m->inc(n);if(n!=3)'+bad,''),
 ('reload',load+'n:=m->inc(7);m=load Library "dependency.dis64";if(m==nil)'+bad+'v:=m->inc(n);if(v!=9)'+bad,''),
 ('helper',load+'n:=first(argv);v:=m->inc(n);if(v!=3)'+bad,'first:fn(xs:list of string):int;|first(xs:list of string):int{return len xs;}'),
 ('before','n:=first(7);'+load+'v:=m->inc(n);if(v!=9)'+bad,'first:fn(n:int):int;|first(n:int):int{return n+1;}'),
 ('helper-owned',load+'s:=first(argv);l:=len s;n:=m->inc(l);if(n!=7 || s!="owned!")'+bad,'first:fn(xs:list of string):string;|first(xs:list of string):string{if(len xs!=2)'+bad+'s:="owned";s+="!";return s;}'),
 ('owned', 'xs:="owned"::argv;'+load+'n:=m->inc(7);if(n!=8 || len xs!=3)'+bad,''),
 ('unicode','s:="Привет";'+load+'l:=len s;n:=m->inc(l);if(n!=7 || s!="Привет")'+bad,''),
 ('array','a:=array[2]of int;a[1]=7;'+load+'n:=a[1];v:=m->inc(n);if(v!=8)'+bad,''),
 ('error',load+'n:=m->inc(7);if(n!=8)'+bad+'z:=0;bad:=1/z;','')]
 for name,body,helper in rows:
  decl,definition=helper.split('|')if helper else('','')
  yield 'chain-'+name,root+decl+'init('+args+'){'+body+'}'+definition,caller,1 if name=='error'else 0,scalar,library
 # The same public Command interface is both exported and imported.
 leaf='implement Command;include "command.m";init('+args+'){if(ctxt!=nil || len argv!=2)'+bad+'}'
 for name,body in [('command','m:=load Command "dependency.dis64";if(m==nil)'+bad+'m->init(ctxt,argv);'),('command-loop','m:=load Command "dependency.dis64";if(m==nil)'+bad+'i:=0;while(i<3){m->init(ctxt,argv);i++;}')]:
  yield 'chain-'+name,'implement Command;include "command.m";init('+args+'){'+body+'}',caller,0,leaf,library

 # A constructor must not invoke init automatically, even when Imports exist.
 body=load+'n:=m->inc(7);z:=0;bad:=1/z;'
 quiet='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;go:=0;if(go)m->init(nil,nil);}'
 yield 'chain-load-only',root+'init('+args+'){'+body+'}',quiet,0,scalar,library

 yield 'chain-leaf-error',root+'init('+args+'){'+load+'n:=m->inc(7);}',caller,1,scalar.replace('return n+1;','z:=0;bad:=1/z;return n+1;'),library
