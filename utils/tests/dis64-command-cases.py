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
 yield from branchcases(pathlength)
 yield from constructorcases(pathlength)
 yield from literalcases(pathlength)
 yield from stringwritecases(pathlength)
 yield from codeccases(pathlength)

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

def branchcases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 nilcheck='if(ctxt.display!=nil)'+bad+'if(nil!=ctxt.screen)'+bad+'if(ctxt.wm!=nil)'+bad
 invoke='ctxt:=ref Draw->Context;m->init(ctxt,argv);m->init(ctxt,argv);keepctxt:=ctxt;'
 seeded='ctxt:ref Draw->Context;m->init(ctxt,argv);m->init(ctxt,argv);keepctxt:=ctxt;'
 rows=[('branches-fields',nilcheck,invoke,0),
 ('branches-equal','if(ctxt.display==nil){if(ctxt.screen==nil){if(ctxt.wm==nil)return;}}z:=0;bad:=1/z;',invoke,0),
 ('branches-context-alias','alias:=ctxt;if(alias!=ctxt)'+bad+'if(ctxt!=alias)'+bad,invoke,0),
 ('branches-context-distinct','other:=ref Draw->Context;if(other==ctxt)'+bad+'if(ctxt==other)'+bad,invoke,0),
 ('branches-context-nil','alias:ref Draw->Context;if(alias==ctxt)'+bad+'if(ctxt==alias)'+bad,invoke,0),
 ('branches-list-alias','copy:=argv;if(copy!=argv)'+bad+'if(argv!=copy)'+bad,invoke,0),
 ('branches-list-distinct','head:=hd argv;other:=head::argv;if(other==argv)'+bad+'if(argv==other)'+bad,invoke,0),
 ('branches-field-alias','d:=ctxt.display;d2:=d;s:=ctxt.screen;s2:=s;w:=ctxt.wm;w2:=w;if(d!=d2)'+bad+'if(s!=s2)'+bad+'if(w!=w2)'+bad,invoke,0),
 ('branches-caller',nilcheck,invoke.replace('m->init(ctxt,argv);m->init(ctxt,argv);',nilcheck+'alias:=ctxt;if(alias!=ctxt)'+bad+'m->init(ctxt,argv);'),0),
 ('branches-short-circuit','if(ctxt==nil || ctxt.display==nil)return;z:=0;bad:=1/z;','m->init(nil,argv);ctxt:=ref Draw->Context;m->init(ctxt,argv);',0),
 ('branches-loop','i:=0;while(ctxt.wm==nil && i<3){i++;}if(i!=3)'+bad,invoke,0),
 ('branches-nil-error','if(ctxt.display==nil)return;','m->init(nil,argv);',3),
 ('branches-native','if(ctxt.display==nil)'+bad+'if(ctxt.screen==nil)'+bad+'if(ctxt.wm==nil)'+bad+'d:=ctxt.display;alias:=d;if(d!=alias)'+bad,seeded,0),
 ('branches-native-loop','w:=ctxt.wm;i:=0;while(ctxt.wm!=nil){ctxt.wm=nil;i++;}if(i!=1)'+bad+'ctxt.wm=w;',seeded,0)]
 mixed=''
 for field,local in [('display','d'),('screen','scr'),('wm','w')]:
  mixed+=local+':=ctxt.'+field+';ctxt.'+field+'=nil;'
  for other in ('display','screen','wm'):mixed+='if(ctxt.'+other+('!=nil)'if other==field else'==nil)')+bad
  mixed+='ctxt.'+field+'='+local+';'
 rows.append(('branches-native-mixed',mixed,seeded,0))
 for name,body,call,failure in rows:
  client='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;s:="library.dis64";argv:=s::s::nil;'+call+'keep:=argv;}'
  yield name,prefix+body+'}',client,failure

def constructorcases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 check='if(ctxt==nil)'+bad+'if(ctxt.display!=nil)'+bad+'if(ctxt.screen!=nil)'+bad+'if(ctxt.wm!=nil)'+bad
 call='ctxt:=ref Draw->Context(nil,nil,nil);m->init(ctxt,argv);m->init(ctxt,argv);keepctxt:=ctxt;'
 rows=[('constructors-local',check,call,0),
 ('constructors-assignment',check,call.replace('ctxt:=ref Draw->Context(nil,nil,nil);','ctxt:ref Draw->Context;ctxt=ref Draw->Context(nil,nil,nil);'),0),
 ('constructors-reload',check,call.replace('m->init(ctxt,argv);m->init(ctxt,argv);','m->init(ctxt,argv);m=load Command "library.dis64";if(m==nil)return;m->init(ctxt,argv);'),0),
 ('constructors-callee', 'other:=ref Draw->Context(nil,nil,nil);ctxt=other;'+check,'m->init(nil,argv);',0),
 ('constructors-distinct',check+'other:=ref Draw->Context(nil,nil,nil);if(other==ctxt)'+bad,call,0),
 ('constructors-reassign',check,'ctxt:=ref Draw->Context(nil,nil,nil);m->init(ctxt,argv);ctxt=ref Draw->Context(nil,nil,nil);m->init(ctxt,argv);keepctxt:=ctxt;',0),
 ('constructors-loop',check,'i:=0;while(i<3){ctxt:=ref Draw->Context(nil,nil,nil);m->init(ctxt,argv);i++;}',0),
 ('constructors-native','d:=ctxt.display;s:=ctxt.screen;w:=ctxt.wm;other:=ref Draw->Context(d,s,w);d2:=other.display;s2:=other.screen;w2:=other.wm;if(d2!=d)'+bad+'if(s2!=s)'+bad+'if(w2!=w)'+bad+'if(other==ctxt)'+bad,'ctxt:ref Draw->Context;m->init(ctxt,argv);m->init(ctxt,argv);keepctxt:=ctxt;',0)]
 for name,body,call,failure in rows:
  client='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;s:="library.dis64";argv:=s::s::nil;'+call+'keep:=argv;}'
  yield name,prefix+body+'}',client,failure

def literalcases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 pairs=[('ascii','hello',5,104),('unicode','Привет',6,1055),('supplementary','a🚀z',3,97),('nul','a\\0z',3,97),('empty','',0,None)]
 rows=[]
 for name,text,length,first in pairs:
  body='s:="'+text+'";if(len s!='+str(length)+')'+bad
  if first is not None:body+='c:=s[0];if(c!='+str(first)+')'+bad
  if name=='supplementary':body+='c1:=s[1];if(c1!=128640)'+bad
  if name=='nul':body+='c1:=s[1];c2:=s[2];if(c1!=0 || c2!=122)'+bad
  rows.append(('literals-'+name,body,'m->init(nil,argv);',0))
 rows += [
 ('literals-arguments','p:=hd argv;if(p!="wm")'+bad+'argv=tl argv;p=hd argv;if(p!="-font")'+bad+'argv=tl argv;p=hd argv;if(p!="你好")'+bad+'if(len argv!=1)'+bad,'m->init(nil,argv);',0),
 ('literals-order','a:="a";b:="aé";if(a>=b || b<=a || a>b || b<a || a==b || !(a!=b))'+bad,'m->init(nil,argv);',0),
 ('literals-nil-empty','a:string;b:="";if(a!=b || a!="" || b!=nil)'+bad,'m->init(nil,argv);',0),
 ('literals-nul-compare','a:="a\\0z";b:="a\\0y";if(a<=b || a==b)'+bad,'m->init(nil,argv);',0),
 ('literals-loop','a:="a🚀z";total:=0;i:=0;while(i<len a){total+=a[i];i++;}if(total!=128859)'+bad,'m->init(nil,argv);',0),
 ('literals-index-negative','a:="text";i:=-1;c:=a[i];','m->init(nil,argv);',4),
 ('literals-index-end','a:="text";i:=len a;c:=a[i];','m->init(nil,argv);',4),
 ('literals-index-nil','a:string;c:=a[0];','m->init(nil,argv);',3),
 ('literals-index-empty','a:="";c:=a[0];','m->init(nil,argv);',3),
 ('literals-context','a:="owned";ctxt=ref Draw->Context(nil,nil,nil);if(ctxt.display!=nil)'+bad+'if(len a!=5)'+bad,'ctxt:=ref Draw->Context(nil,nil,nil);m->init(ctxt,argv);keepctxt:=ctxt;',0),
 ('literals-private-list','a:="private";local:=a::argv;argv=nil;head:=hd local;if(head!="private")'+bad+'if(len local!=4)'+bad,'m->init(nil,argv);',0),
 ('literals-caller','p:=hd argv;if(p!="wm")'+bad,'a:="caller🚀";if(len a!=7)'+bad+'c:=a[6];if(c!=128640)'+bad+'m->init(nil,argv);',0),
 ('literals-reload','a:="reload";if(len a!=6)'+bad,'i:=0;while(i<3){m->init(nil,argv);m=load Command "library.dis64";if(m==nil)return;i++;}',0),
 ('literals-long','a:="'+('é'*255)+'";if(len a!=255)'+bad+'c:=a[254];if(c!=233)'+bad,'m->init(nil,argv);',0),
 ('literals-duplicates','a:="same";b:="same";if(a!=b)'+bad,'m->init(nil,argv);',0)]
 for label,values in [('count-limit',['v%02d'%i for i in range(32)]),('total-limit',['a'*255,'b'*255,'c'*255,'d'*255,'tail'])]:
  body='s:string;total:=0;'
  for value in values:body+='s="'+value+'";total+=len s;'
  body+='if(total!='+str(sum(map(len,values)))+')'+bad
  rows.append(('literals-'+label,body,'m->init(nil,argv);',0))
 for name,body,call,failure in rows:
  client='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;argv:="wm"::"-font"::"你好"::nil;'+call+'keep:=argv;}'
  yield name,prefix+body+'}',client,failure


def stringwritecases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 rows=[
 ('concat','a:="ab";b:="cd";s:=a+b;if(s!="abcd" || a!="ab" || b!="cd")'+bad,0),
 ('mixed','a:="a";b:="🚀";s:=a+b;c:=s[1];if(len s!=2 || c!=128640)'+bad,0),
 ('nul','a:="a\\0";b:="z";s:=a+b;if(len s!=3 || s[1]!=0 || s[2]!=122)'+bad,0),
 ('nil','a:string;b:="x";s:=a+b;t:=b+a;u:=a+a;if(s!="x" || t!="x" || u!=nil)'+bad,0),
 ('append','s:="a";s+="b";s+="🚀";if(s!="ab🚀")'+bad,0),
 ('self','s:="ab";s+=s;if(s!="abab")'+bad,0),
 ('cow','s:="abc";alias:=s;s[1]=90;if(s!="aZc" || alias!="abc")'+bad,0),
 ('widen','s:="abc";s[1]=128640;if(s!="a🚀c")'+bad,0),
 ('wide','s:="a🚀c";s[1]=90;if(s!="aZc")'+bad,0),
 ('grow','s:string;s[0]=97;s[1]=128640;s[2]=0;c0:=s[0];c1:=s[1];c2:=s[2];if(len s!=3 || c0!=97 || c1!=128640 || c2!=0)'+bad,0),
 ('slice','s:="abcde";alias:=s;t:=s[1:4];if(t!="bcd" || alias!="abcde")'+bad,0),
 ('slice-wide','s:="a🚀éz";t:=s[1:3];if(t!="🚀é" || len t!=2)'+bad,0),
 ('slice-nul','s:="a\\0z";t:=s[1:2];if(len t!=1 || t[0]!=0)'+bad,0),
 ('slice-empty','s:="abc";t:=s[1:1];if(len t!=0)'+bad,0),
 ('slice-nil','s:string;t:=s[0:0];if(len t!=0)'+bad,0),
 ('slice-inplace','s:="abcde";s=s[1:4];if(s!="bcd")'+bad,0),
 ('loop','s:string;i:=0;while(i<8){s+="x";i++;}if(len s!=8)'+bad,0),
 ('list','s:="ab";xs:=s::nil;s[0]=90;old:=hd xs;if(old!="ab" || s!="Zb")'+bad,0),
 ('write-negative','s:="a";i:=-1;s[i]=90;',4),
 ('write-gap','s:="a";i:=2;s[i]=90;',4),
 ('slice-negative','s:="a";i:=-1;t:=s[i:1];',4),
 ('slice-reversed','s:="abc";i:=2;j:=1;t:=s[i:j];',4),
 ('slice-end','s:="a";j:=2;t:=s[0:j];',4)]
 for name,body,failure in rows:
  client='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;s:="wm";argv:=s::nil;'+('a:="client";b:="🚀";c:=a+b;c[0]=67;d:=c[1:];if(d!="lient🚀")'+bad if not failure else '')+'m->init(nil,argv);keep:=argv;}'
  yield 'strings-'+name,prefix+body+'}',client,failure


def codeccases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 rows=[]
 for name,value,expected in [('word','123',123),('word-negative',' -123tail',-123),('word-nul','42\\0junk',42),('word-unicode','42é',42),('word-empty','',0)]:
  rows.append((name,'s:="'+value+'";n:=int s;if(n!='+str(expected)+')'+bad))
 rows += [
 ('word-max','s:="2147483647";n:=int s;want:=33554431;want<<=6;want+=63;if(n!=want)'+bad),
 ('word-min','s:="-2147483648";n:=int s;want:=-33554432;want<<=6;if(n!=want)'+bad),
 ('big','s:="7696581394432";n:=big s;seed:=7;want:=big seed;want<<=40;if(n!=want)'+bad),
 ('big-negative','s:="-7696581394432";n:=big s;seed:=-7;want:=big seed;want<<=40;if(n!=want)'+bad),
 ('big-nul','s:="42\\0junk";n:=big s;seed:=42;want:=big seed;if(n!=want)'+bad),
 ('real','s:="1.25";r:=real s;seed:=4;scale:=real seed;r*=scale;n:=int r;if(n!=5)'+bad),
 ('real-exponent','s:="1.25e2";r:=real s;n:=int r;if(n!=125)'+bad),
 ('real-nil','s:string;r:=real s;n:=int r;if(n!=0)'+bad),
 ('format-word','n:=-123;s:=string n;if(s!="-123")'+bad),
 ('format-word-min','n:=-33554432;n<<=6;s:=string n;if(s!="-2147483648")'+bad),
 ('format-big','seed:=7;n:=big seed;n<<=40;s:=string n;if(s!="7696581394432")'+bad),
 ('format-big-negative','seed:=-7;n:=big seed;n<<=40;s:=string n;if(s!="-7696581394432")'+bad),
 ('format-real','seed:=5;denom:=4;r:=real seed;d:=real denom;r/=d;s:=string r;if(s!="1.25")'+bad),
 ('format-reuse','s:="a🚀bcdefghijklmnopqrstuvwxyz";s+="!";alias:=s;n:=123;s=string n;if(s!="123" || alias!="a🚀bcdefghijklmnopqrstuvwxyz!")'+bad),
 ('roundtrip','n:=-987;s:=string n;back:=int s;if(back!=n)'+bad),
 ('bytes-ascii','s:="abc";a:=array of byte s;t:=string a;if(len a!=3 || t!=s)'+bad),
 ('bytes-unicode','s:="Привет";a:=array of byte s;t:=string a;if(len a!=12 || t!=s)'+bad),
 ('bytes-supplementary','s:="a🚀z";a:=array of byte s;t:=string a;if(len a!=6 || t!=s)'+bad),
 ('bytes-nul','s:="a\\0z";a:=array of byte s;t:=string a;if(len a!=3 || t!=s)'+bad),
 ('bytes-empty','s:string;a:=array of byte s;t:=string a;if(len a!=0 || len t!=0)'+bad),
 ('bytes-nil','a:array of byte;t:=string a;if(len t!=0)'+bad),
 ('bytes-alias','s:="Привет";a:=array of byte s;alias:=a;s="z";a=array of byte s;t:=string alias;u:=string a;if(t!="Привет" || u!="z")'+bad),
 ('bytes-reuse','s:="longer";a:=array of byte s;s="x";a=array of byte s;t:=string a;if(len a!=1 || t!="x")'+bad),
 ('bytes-string-alias','s:="old";alias:=s;raw:=array of byte "new";s=string raw;if(s!="new" || alias!="old")'+bad),
 ('bytes-identity','s:="abc";a:=array of byte s;alias:=a;if(a==nil || alias!=a)'+bad),
 ('bytes-fused','s:="a🚀z";s=string array of byte s;if(s!="a🚀z")'+bad),
 ('bytes-loop','s:="é";i:=0;while(i<3){a:=array of byte s;s=string a;i++;}if(s!="é")'+bad)]
 for name,body in rows:
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;s:="42";n:=int s;s=string n;raw:=array of byte s;s=string raw;argv:=s::nil;m->init(nil,argv);keep:=argv;}'
  yield 'codecs-'+name,prefix+body+'}',caller,0
