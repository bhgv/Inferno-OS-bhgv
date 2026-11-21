#!/usr/bin/env python3
"""Execute original Wmlib snarf, local connect, wmctl/input and cursor workers."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
B=runpy.run_path(str(ROOT/'utils/tests/native64-bufio-library.py'));G,CMD,E=B['G'],B['CMD'],B['E']
CLIENT=re.search(r'(?m)^Client: adt\{.*?\n\};',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]
SOURCE='''implement Wmlib;include "sys.m";include "draw.m";include "wmsrv.m";draw:Draw;Context,Wmcontext,Image,Point,Pointer:import draw;Wmlib:module{run:fn(n:int);};sys:Sys;contextchannel:chan of(ref Context,string);receivedcontext:ref Context;receivedtext:string;clientregistry:array of ref Client;Ptrsize:con 49;wptr:chan of Point;cswitch:chan of(string,int,chan of string);kbdstarted,ptrstarted:int;snarfbuf,out,clienterr,clientformatted:string;clientbytes:array of byte;clientcount:int;zclient:Client;client,allocated:ref Client;aggregatefirst,aggregatelast,aggregatepair,aggregatemixed:string;aggregateword:int;cursortext:string;cursorword:int;pointout:Point;aggregatecontext:ref Context;aggregateimage:ref Image;connection,connectionagain:ref Wmcontext;run(n:int){sys=load Sys Sys->PATH;clienttest(n);(ac,ap):=pairresult(n);aggregatecontext=ac;aggregatepair=ap;(am,an):=mixedresult(n);aggregatemixed=am;aggregateword=an;(af,ai,al):=tripleresult(n);aggregatefirst=af;aggregateimage=ai;aggregatelast=al;ctxt:=ref Context(nil,nil,nil);wm:=connect(ctxt);connection=wm;connectionagain=wm;snarfbuf="fallback";out=snarfget();}'''+CLIENT+'''clienttest(n:int){if(sys->pctl(Sys->NEWFD,1::2::nil)!=127)raise "pctl-list";contextchannel=chan of(ref Context,string);c:=ref zclient;c.req=chan of (array of byte,Sys->Rwrite);c.ptrpid=n;c.kbdpid=-1;c.ctlpid=2147483647;c.dir="private";if((c.ctlfd=sys->open(c.dir+"/pointer",Sys->OREAD))==nil)c.ptrpid=-99;c.winfd=sys->open(c.dir+"/pointer",Sys->OREAD);buf:=array[3] of byte;buf[0]=byte 65;buf[1]=byte 66;buf[2]=byte 67;rc:=chan of (int,string);cursorc:=chan[n&1]of(string,int,chan of string);replyc:=chan of string;pointc:=chan of Point;pidc:=chan of int;spawn requestworker(c,buf,rc,c.dir+"/pointer",c.ctlfd,pidc,pointc,cursorc,replyc);alt{pt:=<-pointc=>pointout=pt;}alt{(ct,cn,cr):=<-cursorc=>cursortext=ct;cursorword=cn;cr<-="cursor-ok";}(ct2,cn2,cr2):=<-cursorc;if(ct2!=cursortext||cn2!=cursorword)raise "cursor-mismatch";cr2<-="cursor-ok";(cx,ctext):=contextresult();receivedcontext=cx;receivedtext=ctext;c.ctlpid=<-pidc;(data,reply):=<-c.req;(count,err):=<-reply;clientcount=count;clientbytes=data;clienterr=err;text:=sys->sprint("dir=%s",c.dir+"/pointer");clientcount+=len text;clientformatted=text;if(n!=5){clientcount+=sys->fprint(c.ctlfd,"probe");clientcount+=sys->fprint(c.winfd,"probe");wm:=ref Wmcontext;wm.connfd=c.ctlfd;clientcount+=sys->fprint(wm.connfd,"probe");}if(n!=5){context:=ref Context(nil,nil,nil);wm:=connect(context);wptr=chan of Point;cswitch=chan of(string,int,chan of string);spawn wptrproc(c.ctlfd,c.winfd);(first,im,err):=wmctl(wm,"ptr 2147483647 -1");if(first!=nil||im!=nil||err!=nil)raise "wmctl-pointer";(first,im,err)=wmctl(wm,"cursor 0 0 8 1 0f");if(first!=nil||im!=nil||err!=nil)raise "wmctl-cursor";(first,im,err)=wmctl(wm,"cursor 0 0 8 1 0");if(first!=nil||im!=nil||err!="inconsistent cursor image data")raise "wmctl-bad-cursor";devs:="unknown"::nil;if(startinput(wm,devs)!=nil)raise "startinput";}registry:=array[2]of ref Client;sc:=ref Wmsrv->Client;sc.id=1;registry[sc.id]=c;clientregistry=registry;client=registry[sc.id];d:=ref Client;d.dir="allocated-client";allocated=d;}requestworker(c:ref Client,data:array of byte,reply:Sys->Rwrite,path:string,fd:ref Sys->FD,pidc:chan of int,pointc:chan of Point,cursorc:chan of(string,int,chan of string),replyc:chan of string){p:Point=(2147483647,(-2147483647-1));pointc<-=p;v:=(path,2147483647,replyc);v2:=v;alt{cursorc<-=v2=>}if(<-replyc!="cursor-ok")raise "cursor-reply";cursorc<-=v;if(<-replyc!="cursor-ok")raise "cursor-reply";ct:=ref Context(nil,nil,nil);contextchannel<-=(ct,"received");sys->open(path,Sys->OREAD);pidc<-=(len data+124);if(fd!=nil)sys->read(fd,data,0);c.req<-=(data,reply);reply<-=(len data,nil);}'''+re.search(r'(?m)^snarfget\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]
SOURCE+=re.search(r'(?m)^connect\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]
SOURCE+='pairresult(n:int):(ref Context,string){return(nil,sys->sprint("pair:%d",n));}mixedresult(n:int):(string,int){return(sys->sprint("mixed:%d",n),n);}tripleresult(n:int):(string,ref Image,string){if(n<0)return(sys->sprint("first:%d",n),nil,"last");return("first",nil,sys->sprint("last:%d",n));}'
for name in('wptrproc','hexc','bplong','qword','splitqword','qslice','wmctl','kill','sendreq','kbdproc','ptrproc','bytes2ptr','startinput'):
 SOURCE+=re.search(r'(?m)^'+name+r'\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]
SOURCE+='contextresult():(ref Context,string){return<-contextchannel;}'
def generate(limbo,w):
 m=w/'interfaces.m';m.write_text('include "sys.m";Wmlib:module{run:fn(n:int);};');frames={};sigs={}
 for name in('Sys','Wmlib'):
  h=subprocess.check_output([str(limbo),'-H','-M',name,'-I',str(ROOT/'module'),str(m)],text=True)
  for fn,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+r'_(\w+) (0x[0-9a-f]+)U',h,re.M):
   size=re.search(r'^#define F_'+name+'_'+fn+r'_size (\d+)',h,re.M);mp=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M)
   if size and mp:frames[(fn,int(sig,16))]=(int(size[1]),[int(x,0)for x in mp[1].split(',')if x])
   if name=='Wmlib':sigs[fn]=int(sig,16)
 rows=G['generate'](limbo,w,SOURCE,'Wmlib',(22,1,20,'~0U'),import_frames=frames,export_signatures=sigs,source_label='original Wmlib snarf/connect/control/cursor fixture')
 p=w/'full.h';s=p.read_text();gl={x[0]:x[1]for x in rows[0]['globals']};p.write_text(s+'\nstatic const uint aggregate_out[]={'+','.join(str(gl[n])for n in('aggregatefirst','aggregatelast','aggregatepair','aggregatemixed','aggregateword','aggregatecontext','aggregateimage','pointout','cursortext','cursorword','receivedcontext','receivedtext','clientregistry'))+'};\nstatic const uint snarf_out='+str(gl['out'])+';\nstatic const uint client_out='+str(gl['client'])+', allocated_out='+str(gl['allocated'])+';\nstatic const uint connect_out[]={'+str(gl['connection'])+','+str(gl['connectionagain'])+'};\nstatic const uint message_out[]={'+','.join(str(gl[n])for n in('clientbytes','clienterr','clientcount','clientformatted'))+'};\n')
 B['unit'].__globals__['OP_NAMES']={{'send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase'}.get(n[1:].lower(),n[1:].lower())for n,op in E['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];count=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(153,154,155,156,159,130,68,92,107,60,109,57,53,54,55,78,152,190,50,52,183,44,45,46,47,112,157,158,111,255):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,tables,rows=records)
    except(AssertionError,KeyError,IndexError,ValueError,StopIteration):count+=1
    else:raise AssertionError(('unchecked byte call setup',row['name'],pc,k))
 print('PASS Wmlib snarf source oracle:',count,'instruction corruptions detected',flush=True)
 probe='implement Wmlib;include "sys.m";Wmlib:module{clienttest:fn(n:int);};'+CLIENT+'clienttest(n:int){c:=ref Client;c.ptrpid=n;}'
 _,good=E['compile_source'](limbo,w,probe,interface='',loads=True);assert good[0]['accepted'],good
 for badsource in(probe.replace('Client','Other'),probe.replace('Wmlib','Foreign'),probe.replace('ptrpid','otherpid'),probe.replace('Sys->Rwrite','chan of (int,int)')):
  _,badrows=E['compile_source'](limbo,w,badsource,interface='',loads=True);assert not next(r for r in badrows if r['name']=='clienttest')['accepted'],badrows
 print('PASS private Client boundary: foreign names/modules, fields and request payload refused',flush=True)
 for kind,extra in(('Draw->Rect',''),('Other','Other:adt{x,y:int;};'),('(int,int)','')):
  source='implement Closed;include "sys.m";include "draw.m";'+extra+'Closed:module{run:fn();};run(){c:=chan of '+kind+';}'
  _,badrows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not badrows[0]['accepted'],badrows
 print('PASS Point channel boundary: Rect, foreign record and raw tuple refused',flush=True)

def unit(w):
 A=runpy.run_path(str(ROOT/'utils/tests/native64-aprint-alt.py'));src=A['unit'](w).split('\n#line 1 "native64-aprint-alt.c"')[0];extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
 if 'void\nfreestring('not in src:src+='\n'+extract((ROOT/'libinterp/heap.c').read_text(),'void\nfreestring(')+'\n'
 for sig in('static uchar*\narrayaddress(','static void\narrayindex(','static void\nnewarray('):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 if 'static void\nbranchstring('not in src:src+='\n'+extract(vm,'static void\nbranchstring(')+'\n'
 for sig in('String*\nslicer(',):
  if sig not in src:src+='\n'+extract(strings,sig)+'\n'
 for sig in('static Inst*\ncasepc(','static uvlong\ncaseccell('):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 for n in sorted(B['unit'].__globals__['OP_NAMES']):
  n={'load':'iload','goto':'igoto'}.get(n,n)
  if 'OP('+n+')'not in src:src+='\n'+extract(strings if 'OP('+n+')'in strings else vm,'OP('+n+')')+'\n'
 handlers=''.join('optab['+name+']=actual_'+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+';'for name in E['OPCODES']if 'OP('+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+')'in src)
 return src.replace('OP(newcl)','\n#define movl actual_movl\nOP(newcl)').replace('OP(newcw)','\n#define movw actual_movw\nOP(newcw)').replace('OP(newcp)','\n#define movp actual_movp\nOP(newcp)')+'\n#line 1 "native64-wmlib-snarf.c"\n'+(ROOT/'utils/tests/native64-wmlib-snarf.c').read_text().replace(' SNARF_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='wmlib-snarf-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=240)
  headers=('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-library-code.h','native64-library-layout.h','native64-sys-frame.h','native64-local-bind.h')
  for name,before,after,predicate in(
   ('native64-typed-multi.h','else p->s.imm=s->s;p->reg=native64librarymiddle(s);','else p->s.imm=0;p->reg=native64librarymiddle(s);','original snarf completion'),
   ('native64-library-layout.h','id==12?0x3c','id==12?0x38','constructor baseline'),
   ('native64-typed-code.h','p->d.i.s=s->s==2?16:64;','p->d.i.s=s->s==2?48:64;','original snarf completion'),
   ('native64-typed-multi.h','p->s.i.s=s->m>=256?s->m-256:s->m;','p->s.i.s=s->m==288?40:s->m>=256?s->m-256:s->m;','Client FD variadic argument'),
   ('native64-typed-code.h','p->d.i.s=s->m==4?0:(s->m>>8)&255;','p->d.i.s=s->m==4?0:4;','private Client raw PID fields'),
   ('native64-typed-code.h','if(s->returned==55){p->d.ind=s->d;continue;}','if(s->returned==55){p->op=INEWCW;p->d.ind=s->d;continue;}','Point channel and ALT raw values'),
   ('native64-sys-frame.h','result=base+bias;','result=base;','Sys aggregate String field result'),
   ('native64-typed-multi.h','else if(s->returned==46){if(!native64typedcode(p,1,s,1,fpbytes,mpbytes))return 0;}','else if(s->returned==46){if(!native64typedcode(p,1,s,1,fpbytes,mpbytes))return 0;p->s.imm=0;}','original cursor bytes and hexc calls'),
   ('native64-typed-code.h','p->d.i.s=s->returned==45?16:0;','p->d.i.s=0;','original snarf completion'),
   ('native64-typed-multi.h','else if(s->returned==Native64TypedListPrep){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','else if(s->returned==Native64TypedListPrep){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);if(s->op==ICONSW)p->s.imm=0;}','pctl Word list order')):
   for h in headers:(w/h).write_text((ROOT/'include'/h).read_text())
   header=w/name;text=header.read_text();assert text.count(before)==1;header.write_text(text.replace(before,after));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=240)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr[-1500:])
   for h in headers:(w/h).unlink()
   print('PASS Wmlib snarf active control:',predicate,flush=True)
if __name__=='__main__':main()
