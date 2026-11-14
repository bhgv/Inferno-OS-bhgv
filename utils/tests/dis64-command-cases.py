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
