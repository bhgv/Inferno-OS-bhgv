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
 yield from arraycases(pathlength)
 yield from indexcases(pathlength)
 yield from intarraycases(pathlength)
 yield from localcases(pathlength)
 yield from resultcases(pathlength)
 yield from argumentcases(pathlength)
 yield from scalarresultcases(pathlength)

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


def arraycases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 rows=[
 ('new','n:=3;a:=array[n]of byte;s:=string a;if(len a!=3 || len s!=3)'+bad+'c:=s[1];if(c!=0)'+bad,0),
 ('new-empty','n:=0;a:=array[n]of byte;if(a==nil || len a!=0)'+bad,0),
 ('new-immediate','a:=array[4]of byte;s:=string a;if(len s!=4)'+bad,0),
 ('new-replace','a:=array[5]of byte;alias:=a;n:=2;a=array[n]of byte;if(len a!=2 || len alias!=5 || a==alias)'+bad,0),
 ('new-reuse','a:=array[5]of byte;n:=5;a=array[n]of byte;s:=string a;if(len s!=5)'+bad+'c:=s[4];if(c!=0)'+bad,0),
 ('new-context','a:=array[3]of byte;ctxt=ref Draw->Context(nil,nil,nil);s:=string a;if(len s!=3 || ctxt==nil)'+bad,0),
 ('new-loop','i:=0;while(i<4){a:=array[i]of byte;if(len a!=i)'+bad+'i++;}',0),
 ('new-negative','n:=-1;a:=array[n]of byte;',5),
 ('slice','s:="abcdef";a:=array of byte s;b:=a[1:4];t:=string b;if(t!="bcd" || len a!=6)'+bad,0),
 ('slice-drop-root','s:="abcdef";a:=array of byte s;b:=a[1:4];a=nil;t:=string b;if(t!="bcd")'+bad,0),
 ('slice-nested','s:="abcdef";a:=array of byte s;b:=a[1:5];c:=b[1:3];a=nil;b=nil;t:=string c;if(t!="cd")'+bad,0),
 ('slice-inplace','s:="abcdef";a:=array of byte s;a=a[2:5];t:=string a;if(t!="cde")'+bad,0),
 ('slice-alias','s:="abcdef";a:=array of byte s;alias:=a;a=a[2:5];t:=string a;u:=string alias;if(t!="cde" || u!="abcdef")'+bad,0),
 ('slice-empty','s:="abc";a:=array of byte s;b:=a[1:1];a=nil;t:=string b;if(len b!=0 || len t!=0 || b==nil)'+bad,0),
 ('slice-nil','a:array of byte;b:=a[0:0];if(b!=nil)'+bad,0),
 ('slice-tail','s:="abcdef";a:=array of byte s;b:=a[2:];t:=string b;if(t!="cdef")'+bad,0),
 ('slice-utf8','s:="a🚀z";a:=array of byte s;b:=a[1:5];t:=string b;if(t!="🚀" || len b!=4)'+bad,0),
 ('slice-nul','s:="a\\0z";a:=array of byte s;b:=a[1:2];t:=string b;if(len t!=1)'+bad+'c:=t[0];if(c!=0)'+bad,0),
 ('slice-new','a:=array[5]of byte;b:=a[1:4];a=nil;s:=string b;if(len b!=3 || len s!=3)'+bad,0),
 ('slice-to-codec','s:="abcdef";a:=array of byte s;b:=a[1:4];s="new";b=array of byte s;t:=string a;u:=string b;if(t!="abcdef" || u!="new")'+bad,0),
 ('slice-negative','s:="abc";a:=array of byte s;i:=-1;b:=a[i:2];',4),
 ('slice-reverse','s:="abc";a:=array of byte s;i:=2;j:=1;b:=a[i:j];',4),
 ('slice-end','s:="abc";a:=array of byte s;j:=4;b:=a[0:j];',4),
 ('slice-nil-nonempty','a:array of byte;b:=a[0:1];',3)]
 for name,body,failure in rows:
  call='n:=2;buf:=array[n]of byte;part:=buf[1:];buf=nil;if(len part!=1)'+bad+'s:="wm";argv:=s::nil;m->init(nil,argv);keep:=part;'
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;'+call+'}'
  yield 'arrays-'+name,prefix+body+'}',caller,failure


def indexcases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 rows=[
 ('read','s:="abc";a:=array of byte s;i:=1;n:=int a[i];if(n!=98)'+bad,0),
 ('read-immediate','s:="abc";a:=array of byte s;n:=int a[2];if(n!=99)'+bad,0),
 ('byte-local','s:="abc";a:=array of byte s;b:=a[1];n:=int b;if(n!=98)'+bad,0),
 ('write-constant','s:="abc";a:=array of byte s;i:=1;a[i]=byte 90;t:=string a;if(t!="aZc")'+bad,0),
 ('write-word','s:="abc";a:=array of byte s;i:=1;n:=90;a[i]=byte n;t:=string a;if(t!="aZc")'+bad,0),
 ('write-byte','s:="abc";a:=array of byte s;i:=1;n:=90;b:=byte n;a[i]=b;t:=string a;if(t!="aZc")'+bad,0),
 ('unsigned','s:="é";a:=array of byte s;n:=int a[0];m:=int a[1];if(n!=195 || m!=169)'+bad,0),
 ('zero','a:=array[3]of byte;n:=int a[2];if(n!=0)'+bad,0),
 ('new-write','a:=array[3]of byte;a[0]=byte 97;a[1]=byte 98;a[2]=byte 99;s:=string a;if(s!="abc")'+bad,0),
 ('alias','s:="abc";a:=array of byte s;alias:=a;a[1]=byte 90;n:=int alias[1];if(n!=90)'+bad,0),
 ('slice','s:="abcdef";a:=array of byte s;b:=a[1:4];b[1]=byte 90;t:=string a;u:=string b;if(t!="abZdef" || u!="bZd")'+bad,0),
 ('nested','s:="abcdef";a:=array of byte s;b:=a[1:5];c:=b[1:3];a=nil;b=nil;c[1]=byte 90;t:=string c;if(t!="cZ")'+bad,0),
 ('drop-source','s:="abc";a:=array of byte s;b:=a[1:];a=nil;n:=int b[0];b[1]=byte 90;if(n!=98)'+bad,0),
 ('nul','s:="abc";a:=array of byte s;a[1]=byte 0;t:=string a;if(len t!=3)'+bad+'n:=int a[1];if(n!=0)'+bad,0),
 ('utf8','s:="a🚀z";a:=array of byte s;i:=1;n:=int a[i];if(n!=240)'+bad+'i=4;n=int a[i];if(n!=128)'+bad,0),
 ('read-loop','s:="abc";a:=array of byte s;i:=0;sum:=0;while(i<len a){n:=int a[i];sum+=n;i++;}if(sum!=294)'+bad,0),
 ('write-loop','a:=array[4]of byte;i:=0;while(i<len a){n:=97+i;a[i]=byte n;i++;}s:=string a;if(s!="abcd")'+bad,0),
 ('pair-loop','a:=array[4]of byte;i:=0;while(i<4){a[i]=byte 7;n:=int a[i];if(n!=7)'+bad+'i++;}',0),
 ('reassign','s:="abc";a:=array of byte s;n:=int a[0];a=array[2]of byte;a[1]=byte n;m:=int a[1];if(m!=97)'+bad,0),
 ('context','s:="abc";a:=array of byte s;ctxt=ref Draw->Context(nil,nil,nil);n:=int a[0];a[1]=byte 90;if(n!=97 || ctxt==nil)'+bad,0),
 ('negative','s:="abc";a:=array of byte s;i:=-1;n:=int a[i];',4),
 ('end','s:="abc";a:=array of byte s;i:=len a;n:=int a[i];',4),
 ('empty','a:=array[0]of byte;n:=int a[0];',4),
 ('nil','a:array of byte;n:=int a[0];',4),
 ('write-negative','a:=array[2]of byte;i:=-1;a[i]=byte 9;',4),
 ('write-end','a:=array[2]of byte;i:=2;a[i]=byte 9;',4),
 ('write-nil','a:array of byte;a[0]=byte 9;',4),
 ('read-after-slice','s:="abcde";a:=array of byte s;a=a[1:4];n:=int a[2];if(n!=100)'+bad,0)]
 for name,body,failure in rows:
  call='s:="wm";a:=array of byte s;n:=int a[0];a[1]=byte 77;s=string a;if(n!=119 || s!="wM")'+bad+'argv:=s::nil;m->init(nil,argv);keep:=argv;'
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;'+call+'}'
  yield 'index-'+name,prefix+body+'}',caller,failure

def intarraycases(pathlength=13):
 prefix='implement Command;include "command.m";init(ctxt:ref Draw->Context,argv:list of string){'
 bad='{z:=0;bad:=1/z;}'
 rows=[
 ('zero','a:=array[3]of int;n:=a[2];if(n!=0 || len a!=3)'+bad,0),
 ('write','a:=array[3]of int;a[1]=123456;n:=a[1];if(n!=123456)'+bad,0),
 ('negative-value','a:=array[2]of int;n:=-123456;a[0]=n;v:=a[0];if(v!=n)'+bad,0),
 ('word-high','a:=array[2]of int;n:=268435455;n<<=3;n+=6;a[1]=n;v:=a[1];if(v!=n)'+bad,0),
 ('word-min','a:=array[2]of int;n:=-268435456;n<<=3;a[0]=n;v:=a[0];if(v!=n)'+bad,0),
 ('neighbors','a:=array[3]of int;a[0]=1;a[1]=65536;a[2]=-9;x:=a[0];y:=a[1];z:=a[2];if(x!=1 || y!=65536 || z!= -9)'+bad,0),
 ('local-index','a:=array[3]of int;i:=1;n:=90;a[i]=n;v:=a[i];if(v!=90)'+bad,0),
 ('read-loop','a:=array[4]of int;i:=0;while(i<4){n:=i+1;a[i]=n;i++;}sum:=0;i=0;while(i<len a){n:=a[i];sum+=n;i++;}if(sum!=10)'+bad,0),
 ('alias','a:=array[3]of int;b:=a;a[1]=99;n:=b[1];if(n!=99 || a!=b)'+bad,0),
 ('slice','a:=array[4]of int;b:=a[1:3];b[0]=123;n:=a[1];if(n!=123 || len b!=2)'+bad,0),
 ('nested','a:=array[5]of int;b:=a[1:5];c:=b[1:3];a=nil;b=nil;c[1]=123;n:=c[1];if(n!=123)'+bad,0),
 ('drop-source','a:=array[3]of int;b:=a[1:];a=nil;b[0]=90;n:=b[0];if(n!=90)'+bad,0),
 ('inplace','a:=array[4]of int;a[2]=123;a=a[1:];n:=a[1];if(n!=123)'+bad,0),
 ('empty','a:=array[0]of int;if(len a!=0)'+bad,0),
 ('empty-slice','a:=array[2]of int;b:=a[1:1];a=nil;if(len b!=0)'+bad,0),
 ('mixed','a:=array[2]of int;b:=array[2]of byte;a[1]=123456;b[1]=byte 90;n:=a[1];v:=int b[1];if(n!=123456 || v!=90)'+bad,0),
 ('mixed-reverse','b:=array[2]of byte;a:=array[2]of int;b[0]=byte 65;a[0]=65536;n:=a[0];v:=int b[0];if(n!=65536 || v!=65)'+bad,0),
 ('mixed-slice','a:=array[3]of int;b:=array[3]of byte;c:=a[1:];d:=b[1:];a=nil;b=nil;c[0]=65536;d[0]=byte 90;n:=c[0];v:=int d[0];if(n!=65536 || v!=90)'+bad,0),
 ('bytes-codec','s:="abc";b:=array of byte s;a:=array[2]of int;a[1]=123;n:=int b[1];v:=a[1];if(n!=98 || v!=123)'+bad,0),
 ('new-replace','a:=array[2]of int;a[1]=12;a=array[3]of int;n:=a[1];if(n!=0)'+bad,0),
 ('context','a:=array[3]of int;a[1]=12;ctxt=ref Draw->Context(nil,nil,nil);n:=a[1];if(ctxt==nil || n!=12)'+bad,0),
 ('negative-size','n:=-1;a:=array[n]of int;',5),
 ('negative-index','a:=array[2]of int;i:=-1;n:=a[i];',4),
 ('end-index','a:=array[2]of int;n:=a[2];',4),
 ('empty-index','a:=array[0]of int;n:=a[0];',4),
 ('nil-index','a:array of int;n:=a[0];',4),
 ('write-negative','a:=array[2]of int;i:=-1;a[i]=7;',4),
 ('write-end','a:=array[2]of int;a[2]=7;',4),
 ('write-nil','a:array of int;a[0]=7;',4),
 ('slice-negative','a:=array[2]of int;i:=-1;b:=a[i:];',4),
 ('slice-end','a:=array[2]of int;b:=a[1:3];',4),
 ('slice-nil','a:array of int;b:=a[0:0];if(b!=nil)'+bad,0),
 ('slice-nil-nonempty','a:array of int;b:=a[0:1];',3)]
 for name,body,failure in rows:
  call='a:=array[2]of int;a[1]=123456;n:=a[1];if(n!=123456)'+bad+'argv:="wm"::nil;m->init(nil,argv);keep:=argv;'
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;'+call+'}'
  yield 'intarray-'+name,prefix+body+'}',caller,failure

def localcases(pathlength=13):
 args='ctxt:ref Draw->Context,argv:list of string'
 bad='{z:=0;bad:=1/z;}'
 check='if(len argv!=2)'+bad+'s:=hd argv;if(s!="wm")'+bad
 default='first(ctxt,argv);'
 rows=[
 ('once',default,check,[],0),
 ('twice',default+default,check,[],0),
 ('loop','i:=0;while(i<3){first(ctxt,argv);i++;}',check,[],0),
 ('nil-args','first(nil,nil);','if(ctxt!=nil || argv!=nil)'+bad,[],0),
 ('context','ctxt=ref Draw->Context(nil,nil,nil);first(ctxt,argv);if(ctxt==nil)'+bad,'if(ctxt==nil)'+bad+check,[],0),
 ('context-fields','ctxt=ref Draw->Context(nil,nil,nil);first(ctxt,argv);','if(ctxt==nil)'+bad+'d:=ctxt.display;s:=ctxt.screen;w:=ctxt.wm;if(d!=nil || s!=nil || w!=nil)'+bad,[],0),
 ('callee-list',default,'local:="owned"::argv;argv=nil;if(len local!=3)'+bad+'head:=hd local;if(head!="owned")'+bad+'keep:=local;',[],0),
 ('callee-loop',default,'total:=0;while(argv!=nil){s:=hd argv;total+=len s;argv=tl argv;}if(total!=5)'+bad,[],0),
 ('callee-string',default,'s:="a🚀z";s=s[1:2];if(s!="🚀")'+bad,[],0),
 ('callee-bytes',default,'a:=array[3]of byte;a[1]=byte 90;n:=int a[1];if(n!=90)'+bad,[],0),
 ('callee-ints',default,'a:=array[3]of int;a[1]=123456;n:=a[1];if(n!=123456)'+bad,[],0),
 ('callee-slice',default,'a:=array[4]of int;b:=a[1:];a=nil;b[0]=65536;n:=b[0];if(n!=65536)'+bad,[],0),
 ('caller-live','a:=array[2]of int;a[1]=65536;first(ctxt,argv);n:=a[1];if(n!=65536)'+bad,'a:=array[2]of byte;a[1]=byte 90;n:=int a[1];if(n!=90)'+bad,[],0),
 ('early-return',default+default,'if(len argv==2)return;'+bad,[],0),
 ('branch','if(argv!=nil)first(ctxt,argv);',check,[],0),
 ('nested',default,'second(ctxt,argv);', [('second',check)],0),
 ('deep',default,'second(ctxt,argv);',[('second','third(ctxt,argv);'),('third',check)],0),
 ('diamond',default,'second(ctxt,argv);third(ctxt,argv);',[('second','fourth(ctxt,argv);'),('third','fourth(ctxt,argv);'),('fourth',check)],0),
 ('sibling','first(ctxt,argv);second(ctxt,argv);',check,[('second',check)],0),
 ('cross','first(ctxt,argv);second(ctxt,argv);','second(ctxt,argv);',[('second',check)],0),
 ('max-depth',default,'a:=array[2]of int;b:=array[2]of byte;a[1]=65536;b[1]=byte 90;h2(ctxt,argv);n:=a[1];v:=int b[1];if(n!=65536 || v!=90)'+bad,[(('h'+str(i)),('h'+str(i+1)+'(ctxt,argv);')if i<8 else check)for i in range(2,9)],0),
 ('nested-loop',default,'i:=0;while(i<3){second(ctxt,argv);i++;}', [('second',check)],0),
 ('nested-live',default,'a:=array[2]of int;a[1]=65536;second(ctxt,argv);n:=a[1];if(n!=65536)'+bad,[('second','a:=array[2]of byte;a[1]=byte 7;n:=int a[1];if(n!=7)'+bad)],0),
 ('nested-nil',default,'second(nil,nil);',[('second','if(argv!=nil || ctxt!=nil)'+bad)],0),
 ('error',default,'z:=0;bad:=1/z;',[],1),
 ('nested-error',default,'second(ctxt,argv);',[('second','z:=0;bad:=1/z;')],1),
 ('bounds',default,'a:=array[1]of int;n:=a[1];',[],4),
 ('nested-bounds',default,'second(ctxt,argv);',[('second','a:=array[1]of byte;n:=int a[1];')],4)]
 for name,body,helper,others,failure in rows:
  helpers=[('first',helper)]+others
  declarations=''.join(fn+':fn('+args+');'for fn,_ in helpers)
  definitions=''.join(fn+'('+args+'){'+code+'}'for fn,code in helpers)
  provider='implement Command;include "command.m";'+declarations+'init('+args+'){'+body+'}'+definitions
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;argv:="wm"::"a🚀z"::nil;m->init(nil,argv);if(len argv!=2)'+bad+'}'
  yield 'local-'+name,provider,caller,failure

def resultcases(pathlength=13):
 args='ctxt:ref Draw->Context,argv:list of string'
 bad='{z:=0;bad:=1/z;}'
 rows=[
 ('string','string','return "owned";','s:=first(ctxt,argv);if(s!="owned")'+bad,[]),
 ('unicode','string','return "a🚀z";','s:=first(ctxt,argv);if(len s!=3 || s!="a🚀z")'+bad,[]),
 ('nil-string','string','return nil;','s:=first(ctxt,argv);if(s!=nil)'+bad,[]),
 ('empty-string','string','return "";','s:=first(ctxt,argv);if(len s!=0)'+bad,[]),
 ('head','string','s:=hd argv;return s;','s:=first(ctxt,argv);if(s!="wm")'+bad,[]),
 ('concat','string','s:="one";s+="two";return s;','s:=first(ctxt,argv);if(s!="onetwo")'+bad,[]),
 ('slice-string','string','s:="a🚀z";s=s[1:2];return s;','s:=first(ctxt,argv);if(s!="🚀")'+bad,[]),
 ('nul','string','s:="a\0z";return s;','s:=first(ctxt,argv);if(len s!=3)'+bad+'i:=1;n:=s[i];if(n!=0)'+bad,[]),
 ('replace-string','string','s:="new";return s;','s:="old";s=first(ctxt,argv);if(s!="new")'+bad,[]),
 ('repeat-string','string','return "new";','s:string;i:=0;while(i<3){s=first(ctxt,argv);i++;}if(s!="new")'+bad,[]),
 ('list','list of string','local:="owned"::argv;argv=nil;return local;','xs:=first(ctxt,argv);if(len xs!=3)'+bad+'s:=hd xs;if(s!="owned")'+bad,[]),
 ('nil-list','list of string','return nil;','xs:=first(ctxt,argv);if(xs!=nil)'+bad,[]),
 ('tail','list of string','return tl argv;','xs:=first(ctxt,argv);if(len xs!=1)'+bad+'s:=hd xs;if(s!="abc")'+bad,[]),
 ('replace-list','list of string','return "new"::nil;','xs:="old"::argv;xs=first(ctxt,argv);s:=hd xs;if(len xs!=1 || s!="new")'+bad,[]),
 ('context','ref Draw->Context','local:=ref Draw->Context(nil,nil,nil);return local;','c:=first(ctxt,argv);if(c==nil)'+bad+'d:=c.display;if(d!=nil)'+bad,[]),
 ('nil-context','ref Draw->Context','return nil;','c:=first(ctxt,argv);if(c!=nil)'+bad,[]),
 ('borrow-context','ref Draw->Context','return ctxt;','c:=ref Draw->Context(nil,nil,nil);d:=first(c,argv);if(c!=d)'+bad,[]),
 ('bytes','array of byte','a:=array[3]of byte;a[1]=byte 90;return a;','a:=first(ctxt,argv);n:=int a[1];if(n!=90 || len a!=3)'+bad,[]),
 ('nil-bytes','array of byte','return nil;','a:=first(ctxt,argv);if(a!=nil)'+bad,[]),
 ('byte-view','array of byte','s:="abcdef";a:=array of byte s;b:=a[1:4];a=nil;return b;','a:=first(ctxt,argv);s:=string a;if(s!="bcd")'+bad,[]),
 ('ints','array of int','a:=array[3]of int;a[1]=123456;return a;','a:=first(ctxt,argv);n:=a[1];if(n!=123456)'+bad,[]),
 ('int-view','array of int','a:=array[4]of int;a[2]=65536;b:=a[1:];a=nil;return b;','a:=first(ctxt,argv);n:=a[1];if(n!=65536)'+bad,[]),
 ('branch','string','if(argv==nil)return nil;return hd argv;','s:=first(ctxt,argv);t:=first(nil,nil);if(s!="wm" || t!=nil)'+bad,[]),
 ('forward','string','return second(ctxt,argv);','s:=first(ctxt,argv);if(s!="deep")'+bad,[('second','string','return "deep";')]),
 ('forward-list','list of string','return second(ctxt,argv);','xs:=first(ctxt,argv);s:=hd xs;if(s!="deep")'+bad,[('second','list of string','return "deep"::argv;')]),
 ('forward-array','array of int','return second(ctxt,argv);','a:=first(ctxt,argv);n:=a[1];if(n!=65536)'+bad,[('second','array of int','a:=array[2]of int;a[1]=65536;return a;')]),
 ('local-result','string','s:=second(ctxt,argv);s+="!";return s;','s:=first(ctxt,argv);if(s!="deep!")'+bad,[('second','string','return "deep";')]),
 ('caller-live','string','return "new";','a:=array[2]of int;a[1]=65536;s:=first(ctxt,argv);n:=a[1];if(n!=65536 || s!="new")'+bad,[]),
 ('error','string','z:=0;bad:=1/z;return nil;','s:=first(ctxt,argv);',[]),
 ('nested-error','string','return second(ctxt,argv);','s:=first(ctxt,argv);',[('second','string','z:=0;bad:=1/z;return nil;')])]
 for name,kind,helper,body,others in rows:
  functions=[('first',kind,helper)]+others
  declarations=''.join(fn+':fn('+args+'):'+ty+';'for fn,ty,_ in functions)
  definitions=''.join(fn+'('+args+'):'+ty+'{'+code+'}'for fn,ty,code in functions)
  provider='implement Command;include "command.m";'+declarations+'init('+args+'){'+body+'}'+definitions
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;argv:="wm"::"abc"::nil;m->init(nil,argv);if(len argv!=2)'+bad+'}'
  yield 'result-'+name,provider,caller,1 if name in('error','nested-error')else 0

def argumentcases(pathlength=13):
 bad='{z:=0;bad:=1/z;}'
 rootargs='ctxt:ref Draw->Context,argv:list of string'
 ok='return "okay";'
 rows=[
 ('empty','','','first();','',[]),
 ('empty-result','','string','s:=first();if(s!="okay")'+bad,ok,[]),
 ('int','n:int','string','n:=7;s:=first(n);if(s!="okay")'+bad,'if(n!=7)'+bad+ok,[]),
 ('int-immediate','n:int','string','s:=first(7);if(s!="okay")'+bad,'if(n!=7)'+bad+ok,[]),
 ('int-negative','n:int','string','n:=-123456;s:=first(n);if(s!="okay")'+bad,'if(n!= -123456)'+bad+ok,[]),
 ('int-high','n:int','string','n:=268435455;n<<=3;n+=6;s:=first(n);if(s!="okay")'+bad,'e:=268435455;e<<=3;e+=6;if(n!=e)'+bad+ok,[]),
 ('byte','b:byte','string','n:=255;b:=byte n;s:=first(b);if(s!="okay")'+bad,'n:=int b;if(n!=255)'+bad+ok,[]),
 ('byte-immediate','b:byte','string','s:=first(byte 255);if(s!="okay")'+bad,'n:=int b;if(n!=255)'+bad+ok,[]),
 ('byte-zero','b:byte','string','s:=first(byte 0);if(s!="okay")'+bad,'n:=int b;if(n!=0)'+bad+ok,[]),
 ('bytes-packed','a:byte,b:byte,c:byte,n:int','string','s:=first(byte 7,byte 128,byte 255,65536);if(s!="okay")'+bad,'x:=int a;y:=int b;z:=int c;if(x!=7 || y!=128 || z!=255 || n!=65536)'+bad+ok,[]),
 ('big','b:big','string','n:=7;b:=big n;b<<=40;s:=first(b);if(s!="okay")'+bad,'n:=7;e:=big n;e<<=40;if(b!=e)'+bad+ok,[]),
 ('big-negative','b:big','string','n:=-7;b:=big n;b<<=40;s:=first(b);if(s!="okay")'+bad,'n:=-7;e:=big n;e<<=40;if(b!=e)'+bad+ok,[]),
 ('real','r:real','string','n:=5;d:=4;r:=real n;q:=real d;r/=q;s:=first(r);if(s!="okay")'+bad,'n:=5;d:=4;e:=real n;q:=real d;e/=q;if(r!=e)'+bad+ok,[]),
 ('real-negative','r:real','string','n:=-5;d:=4;r:=real n;q:=real d;r/=q;s:=first(r);if(s!="okay")'+bad,'n:=-5;d:=4;e:=real n;q:=real d;e/=q;if(r!=e)'+bad+ok,[]),
 ('mix','a:byte,n:int,b:big,r:real','string','n:=7;b:=big n;r:=real n;s:=first(byte 255,n,b,r);if(s!="okay")'+bad,'x:=int a;e:=7;y:=big e;z:=real e;if(x!=255 || n!=7 || b!=y || r!=z)'+bad+ok,[]),
 ('reverse','r:real,b:big,n:int,a:byte','string','n:=7;b:=big n;r:=real n;s:=first(r,b,n,byte 255);if(s!="okay")'+bad,'x:=int a;e:=7;y:=big e;z:=real e;if(x!=255 || n!=7 || b!=y || r!=z)'+bad+ok,[]),
 ('prefix-int',rootargs+',n:int','string','s:=first(ctxt,argv,7);if(s!="okay")'+bad,'if(n!=7 || len argv!=2 || ctxt!=nil)'+bad+ok,[]),
 ('prefix-mix',rootargs+',a:byte,n:int,b:big,r:real','string','n:=7;b:=big n;r:=real n;s:=first(ctxt,argv,byte 255,n,b,r);if(s!="okay")'+bad,'x:=int a;e:=7;y:=big e;z:=real e;if(x!=255 || n!=7 || b!=y || r!=z || len argv!=2)'+bad+ok,[]),
 ('eight','a:int,b:int,c:int,d:int,e:int,f:int,g:int,h:int','string','s:=first(1,2,3,4,5,6,7,8);if(s!="okay")'+bad,'n:=a+b+c+d+e+f+g+h;if(n!=36)'+bad+ok,[]),
 ('prefix-eight',rootargs+',a:int,b:int,c:int,d:int,e:int,f:int,g:int,h:int','string','s:=first(ctxt,argv,1,2,3,4,5,6,7,8);if(s!="okay")'+bad,'n:=a+b+c+d+e+f+g+h;if(n!=36 || len argv!=2)'+bad+ok,[]),
 ('mutate','n:int','string','n:=7;s:=first(n);if(n!=7 || s!="okay")'+bad,'n+=3;if(n!=10)'+bad+ok,[]),
 ('loop','n:int','string','i:=0;s:string;while(i<3){s=first(i);i++;}if(s!="okay")'+bad,'if(n<0 || n>2)'+bad+ok,[]),
 ('nested','n:int','string','s:=first(7);if(s!="okay")'+bad,'return second(n);',[('second','n:int','string','if(n!=7)'+bad+ok)]),
 ('nested-prefix',rootargs+',n:int','string','s:=first(ctxt,argv,7);if(s!="okay")'+bad,'return second(n);',[('second','n:int','string','if(n!=7)'+bad+ok)]),
 ('nested-to-prefix','n:int','string','s:=first(7);if(s!="okay")'+bad,'xs:="local"::nil;return second(nil,xs,n);',[('second',rootargs+',n:int','string','if(n!=7 || len argv!=1 || ctxt!=nil)'+bad+ok)]),
 ('list-result','n:int','list of string','xs:=first(7);s:=hd xs;if(s!="okay" || len xs!=1)'+bad,'if(n!=7)'+bad+'xs:="okay"::nil;return xs;',[]),
 ('array-result','n:int','array of int','a:=first(7);n:=a[1];if(n!=7)'+bad,'a:=array[2]of int;a[1]=n;return a;',[]),
 ('context-result','n:int','ref Draw->Context','c:=first(7);if(c==nil)'+bad,'if(n!=7)'+bad+'c:=ref Draw->Context(nil,nil,nil);return c;',[]),
 ('live-caller','n:int','string','a:=array[2]of int;a[1]=65536;s:=first(7);n:=a[1];if(n!=65536 || s!="okay")'+bad,'if(n!=7)'+bad+ok,[]),
 ('live-callee','n:int','string','s:=first(7);if(s!="okay")'+bad,'xs:="owned"::nil;second(n);if(len xs!=1)'+bad+ok,[('second','n:int','','if(n!=7)'+bad)]),
 ('error','n:int','','first(7);','if(n!=7)'+bad+'z:=0;bad:=1/z;',[]),
 ('empty-error','','','first();','z:=0;bad:=1/z;',[])]
 for name,args,result,body,helper,others in rows:
  functions=[('first',args,result,helper)]+others
  declarations=''.join(fn+':fn('+params+')'+(':'+ty if ty else'')+';'for fn,params,ty,_ in functions)
  definitions=''.join(fn+'('+params+')'+(':'+ty if ty else'')+'{'+code+'}'for fn,params,ty,code in functions)
  provider='implement Command;include "command.m";'+declarations+'init('+rootargs+'){'+body+'}'+definitions
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;argv:="wm"::"abc"::nil;m->init(nil,argv);}'
  yield 'args-'+name,provider,caller,1 if name in('error','empty-error')else 0

def scalarresultcases(pathlength=13):
 bad='{z:=0;bad:=1/z;}'
 rootargs='ctxt:ref Draw->Context,argv:list of string'
 rows=[]
 for kind,setup,value,check in [
  ('byte','b:=byte 255;','b','n:=int r;if(n!=255)'+bad),
  ('int','n:=-123456;','n','if(r!= -123456)'+bad),
  ('big','n:=7;b:=big n;b<<=40;','b','expectedn:=7;e:=big expectedn;e<<=40;if(r!=e)'+bad),
  ('real','n:=5;d:=4;b:=real n;q:=real d;b/=q;','b','expectedn:=5;expectedd:=4;e:=real expectedn;expectedq:=real expectedd;e/=expectedq;if(r!=e)'+bad)]:
  rows.extend([
   (kind, 'x:'+kind,kind,setup+'r:=first('+value+');'+check,'return x;',[]),
   (kind+'-nested','x:'+kind,kind,setup+'r:=first('+value+');'+check,'return second(x);',[('second','x:'+kind,kind,'return x;')]),
   (kind+'-prefix',rootargs+',x:'+kind,kind,setup+'r:=first(ctxt,argv,'+value+');'+check,'if(len argv!=2)'+bad+'return x;',[]),
   (kind+'-branch','x:'+kind+',go:int',kind,setup+'r:=first('+value+',1);'+check,'if(go)return x;return x;',[]),
   (kind+'-loop','x:'+kind,kind,setup+'i:=0;while(i<3){r:=first('+value+');'+check+'i++;}','return x;',[]),
   (kind+'-caller-live','x:'+kind,kind,setup+'xs:="owned"::nil;r:=first('+value+');'+check+'if(len xs!=1)'+bad,'return x;',[])])
 rows += [
 ('int-immediate','','int','r:=first();if(r!=7)'+bad,'return 7;',[]),
 ('byte-immediate','','byte','r:=first();n:=int r;if(n!=255)'+bad,'return byte 255;',[]),
 ('int-computed','x:int','int','r:=first(7);if(r!=10)'+bad,'return x+3;',[]),
 ('big-computed','x:big','big','n:=7;b:=big n;r:=first(b);e:=b+b;if(r!=e)'+bad,'return x+x;',[]),
 ('real-computed','x:real','real','n:=7;b:=real n;r:=first(b);e:=b+b;if(r!=e)'+bad,'return x+x;',[]),
 ('int-to-byte','x:int','byte','r:=first(511);n:=int r;if(n!=255)'+bad,'return byte x;',[]),
 ('byte-to-int','x:byte','int','r:=first(byte 255);if(r!=255)'+bad,'return int x;',[]),
 ('int-to-big','x:int','big','r:=first(7);n:=7;e:=big n;if(r!=e)'+bad,'return big x;',[]),
 ('int-to-real','x:int','real','r:=first(7);n:=7;e:=real n;if(r!=e)'+bad,'return real x;',[])]
 for name,args,result,body,helper,others in rows:
  functions=[('first',args,result,helper)]+others
  declarations=''.join(fn+':fn('+params+'):'+ty+';'for fn,params,ty,_ in functions)
  definitions=''.join(fn+'('+params+'):'+ty+'{'+code+'}'for fn,params,ty,code in functions)
  provider='implement Command;include "command.m";'+declarations+'init('+rootargs+'){'+body+'}'+definitions
  caller='implement Client;include "command.m";Client:module{init:fn();};init(){m:=load Command "library.dis64";if(m==nil)return;argv:="wm"::"abc"::nil;m->init(nil,argv);}'
  yield 'scalarresult-'+name,provider,caller,0
