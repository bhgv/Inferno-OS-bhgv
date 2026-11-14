"""Ordinary .m Command.init calls: source files own both code bodies."""
INTERFACE='include "sys.m";include "draw.m";include "sh.m";'
def cases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 check='if(ctxt!=nil){z:=0;bad:=1/z;}if(len argv!=2){z:=0;bad:=1/z;}'
 consume='total:=0;while(argv!=nil){part:=hd argv;total+=len part;argv=tl argv;}if(total!='+str(2*pathlength)+'){z:=0;bad:=1/z;}'
 default='m->init(nil,argv);if(len argv!=2){z:=0;bad:=1/z;}m->init(nil,argv);'
 rows=[
 ('consume-two',check+consume,default,0),
 ('empty-argv','if(len argv!=0){z:=0;bad:=1/z;}return;','m->init(nil,nil);m->init(nil,nil);',0),
 ('head-argv',check+'s:=hd argv;if(len s!='+str(pathlength)+'){z:=0;bad:=1/z;}',default,0),
 ('callee-cons',check+'s:=hd argv;local:=s::argv;argv=nil;if(len local!=3){z:=0;bad:=1/z;}head:=hd local;n:=len head;keephead:=head;if(n!='+str(pathlength)+'){z:=0;bad:=1/z;}keep:=local;',default,0),
 ('callee-tail',check+'argv=tl argv;if(len argv!=1){z:=0;bad:=1/z;}',default,0),
 ('context-copy','alias:=ctxt;ctxt=nil;if(alias!=nil){z:=0;bad:=1/z;}'+consume,'ctxt:ref Draw->Context;m->init(ctxt,argv);keepctxt:=ctxt;',0),
 ('context-non-nil','if(ctxt==nil){z:=0;bad:=1/z;}alias:=ctxt;ctxt=nil;if(alias==nil){z:=0;bad:=1/z;}', 'ctxt:ref Draw->Context;m->init(ctxt,argv);keepctxt:=ctxt;',0),
 ('source-context','if(ctxt==nil){z:=0;bad:=1/z;}alias:=ctxt;ctxt=nil;if(alias==nil){z:=0;bad:=1/z;}', 'ctxt:=ref Draw->Context;m->init(ctxt,argv);keepctxt:=ctxt;',0),
 ('callee-context-new','local:=ref Draw->Context;ctxt=nil;alias:=local;if(alias==nil){z:=0;bad:=1/z;}keep:=alias;',default,0),
 ('load-no-init','z:=0;bad:=1/z;','go:=0;if(go)m->init(nil,argv);',0),
 ('explicit-init-error','z:=0;bad:=1/z;','m->init(nil,argv);',1),
 ('early-return','if(len argv==2)return;z:=0;bad:=1/z;',default,0),
 ('reload-loop',check+consume,'i:=0;while(i<3){m->init(nil,argv);m=load Command "library.dis64";if(m==nil)return;i++;}',0)]
 for name,body,invoke,failure in rows:
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;s:="library.dis64";argv:=s::s::nil;'+invoke+'keep:=argv;}'
  yield name,prefix+body+'}',caller,failure
 yield from fieldcases(pathlength)

def fieldcases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 invoke='ctxt:=ref Draw->Context;m->init(ctxt,argv);m->init(ctxt,argv);keepctxt:=ctxt;'
 read='d:=ctxt.display;s:=ctxt.screen;w:=ctxt.wm;'
 check='if(d!=nil){z:=0;bad:=1/z;}if(s!=nil){z:=0;bad:=1/z;}if(w!=nil){z:=0;bad:=1/z;}'
 clear='ctxt.display=nil;ctxt.screen=nil;ctxt.wm=nil;'
 restore='ctxt.display=d;ctxt.screen=s;ctxt.wm=w;'
 rows=[('fields-read',read+check,invoke,0),('fields-clear',clear+read+check,invoke,0),('fields-restore',read+clear+check+restore,invoke,0),
 ('fields-caller',read+check,invoke.replace('m->init(ctxt,argv);m->init(ctxt,argv);',(read+clear+check+restore).replace('s:=ctxt.screen','scr:=ctxt.screen').replace('if(s!=nil)','if(scr!=nil)').replace('ctxt.screen=s','ctxt.screen=scr')+'m->init(ctxt,argv);'),0),
 ('fields-alias','alias:=ctxt;'+read+clear+restore+'ctxt=nil;d=alias.display;s=alias.screen;w=alias.wm;'+check,invoke,0),
 ('fields-two-contexts',read+'local:=ref Draw->Context;local.display=d;local.screen=s;local.wm=w;ctxt=nil;d=local.display;s=local.screen;w=local.wm;'+check,invoke,0),
 ('fields-callee-new','ctxt=ref Draw->Context(nil,nil,nil);'+read+check,'m->init(nil,argv);',0),
 ('fields-loop','i:=0;while(i<3){'+read+clear+restore+check+'i++;}',invoke,0),
 ('fields-nil-read','d:=ctxt.display;keep:=d;','m->init(nil,argv);',3),
 ('fields-nil-write','ctxt.screen=nil;','m->init(nil,argv);',3),
 ('fields-native',read+'if(d==nil){z:=0;bad:=1/z;}if(s==nil){z:=0;bad:=1/z;}if(w==nil){z:=0;bad:=1/z;}'+clear+'aliasd:=d;aliass:=s;aliasw:=w;'+restore,'ctxt:ref Draw->Context;m->init(ctxt,argv);m->init(ctxt,argv);keepctxt:=ctxt;',0),
 ('fields-cons',read+check+'s0:=hd argv;local:=s0::argv;keep:=local;'+clear+restore,invoke,0)]
 for name,body,call,failure in rows:
  client='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;s:="library.dis64";argv:=s::s::nil;'+call+'keep:=argv;}'
  yield name,prefix+body+'}',client,failure
