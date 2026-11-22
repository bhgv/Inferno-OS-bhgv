#!/usr/bin/env python3
"""Execute flattened mixed, nested Word and mapped results through real local calls."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
B=runpy.run_path(str(ROOT/'utils/tests/native64-local-geometry-args.py'));G,CMD,E=B['G'],B['CMD'],B['E']
DECL='Winplace:module{run:fn(n:int);};'
SOURCE='''implement Winplace;include "sys.m";include "draw.m";include "tk.m";include "sh.m";Point,Rect:import Draw;Listnode,Var,Localenv,Builtins,Node,Redir:import Sh;'''+DECL+'''EW,NS:con iota;Lay:adt{d:int;mkr:fn(l:self Lay,r:Rect):Rect;};
syntaxtext,syntaxassign,syntaxword,syntaxnest:string;syntaxkind,syntaxtype:int;builtintable:ref Builtins;hit,miss:list of Shellbuiltin;before,between,after,empty:int;envtext,envvalue,envfirst,envfirstvalue:string;builtinmarker,builtinmatch,builtinmissing:Shellbuiltin;lookupchild,lookupparent,lookuppos,lookupmiss:list of ref Sh->Listnode;envvar,hashfirst,hashmiss:ref Sh->Var;envcount:int;shellctx:ref Sh->Context;shellflags:int;catchvalue:int;nodes,converted,empty_nodes:list of ref Sh->Listnode;menutext:string;topout:ref Tk->Toplevel;eventout:chan of string;bytebuf:array of byte;pointout:Point;left,middle,right:list of Rect;quoted:string;nextq:int;parsed:Rect;position:int;out:string;word,a,b,c,d,e:int;ctx:ref Draw->Context;image:ref Draw->Image;last:string;
run(n:int){rest:=tl nodes;entry:=hd rest;added:=ref Sh->Listnode(nil,entry.word);nodes=added::nodes;front:=hd nodes;menutext=front.word;inputwords:="first"::"second"::nil;converted=stringlist2list(inputwords);no_words:list of string;empty_nodes=stringlist2list(no_words);shellrecords(n);shelllookup();builtinlookup(n);if(n>=0)removebuiltinmod(builtintable,builtinmarker);envpairs(n);buf:=array[12] of byte;bplong(buf,4,n);bytebuf=buf;catchprobe(n);(top,event):=tkpair(n);topout=top;eventout=event;(s,v):=mixed(n);out=s;word=v;(v1,v2,v3,v4,v5):=five(n);a=v1;b=v2;c=v3;d=v4;e=v5;(range,z):=nested(n);i:=firstword(range);a=i;b=range.t1+i-i;c=z;(ct,tail):=roots(n);ctx=ct;last=tail;(first,im,tail2):=forward(n);out=first;image=im;last=tail2;(r,end):=s2r(" -2147483648 2147483647 -1 0",0);p:=swappt((n,1),n);pointout=p;lay:=Lay(n>=0);mapped:=lay.mkr(r);parsed=mapped;position=end;(q,stop):=qword(" 'a''b' rest",0);quoted=q;nextq=stop;input:list of Rect;if(n>=0){(lb,seed,rb):=select(nil,nil,0,1,3);input=seed;left=lb;right=rb;}(ls,ms,rs):=select(input,nil,1,2,3);left=ls;middle=ms;right=rs;}
builtinprobe(){
 table:=builtintable.ba;
 value_hit:=builtinproviders(builtintable,"beta");hit=value_hit;
 value_miss:=builtinproviders(builtintable,"betaz");miss=value_miss;
 value_before:=builtinindex(table,3,"0");before=value_before;
 value_between:=builtinindex(table,3,"betaz");between=value_between;
 value_after:=builtinindex(table,3,"z");after=value_after;
 value_empty:=builtinindex(table,0,"beta");empty=value_empty;
}
rendernested(tree:ref Node):string {
 if(tree==nil)return "?";
 left:=tree.left;
 right:=tree.right;
 word:=tree.word;
 if(left==nil && right==nil)return word;
 prefix:=rendernested(left);
 suffix:=rendernested(right);
 return "["+prefix+":"+suffix+"]";
}
syntaxprobe(n:int){
 redirect:=ref Redir(3,n,7);
 child:=ref Node(11,nil,nil,"child",nil);
 tree:=ref Node(12,child,nil,nil,redirect);
 first:=tree.left;
 syntaxkind=first.ntype;
 syntaxword=first.word;
 target:=tree.redir;
 syntaxtype=target.rtype;
 plain:=fdassignstr(0,target);
 syntaxtext=plain;
 assign:=fdassignstr(1,target);
 syntaxassign=assign;
 nested:=rendernested(tree);
 syntaxnest=nested;
}
builtinlookup(n:int){
 syntaxprobe(n);
 builtinprobe();
 modules:list of (string, Shellbuiltin);
 if(n>=0){
  entry:=("available",builtinmarker);
  modules=entry::modules;
  entry=("missing",nil);
  modules=entry::modules;
 }
 for(;modules!=nil;modules=tl modules){
  (name,builtin):=hd modules;
  if(name=="available"){
   providers:list of Shellbuiltin;
   providers=builtin::providers;
   providers=builtinmissing::providers;
   if(hd providers==builtinmissing)providers=tl providers;
   if(providers!=nil && hd providers!=builtinmissing)builtinmatch=hd providers;
  }
  else builtinmissing=builtin;
 }
}
shelllookup(){
 outer:=array[7] of list of ref Var;
 inner:=array[7] of list of ref Var;
 pwords:="parent"::nil;
 cwords:="child"::nil;
 args:="first"::"second"::nil;
 pv:=stringlist2list(pwords);
 cv:=stringlist2list(cwords);
 av:=stringlist2list(args);
 entry:=ref Var("autoload",pv,0);
 idx:=hashfn("autoload",7);
 hashadd(outer,idx,entry);
 entry=ref Var("autoload",cv,0);
 hashadd(inner,idx,entry);
 idx=hashfn("*",7);
 entry=ref Var("*",av,0);
 hashadd(outer,idx,entry);
 base:=ref Localenv(outer,nil,0);
 local:=ref Localenv(inner,base,0);
 e:=ref Sh->Environment(nil,nil,nil,local);
 c:=ref Sh->Context(e,nil,nil,nil);
 lookupchild=shellget(c,"autoload");
 lookuppos=shellget(c,"2");
 lookupmiss=shellget(c,"missing");
 e.localenv=base;
 lookupparent=shellget(c,"autoload");
}
envpairs(n:int){pairs:list of (string,string);if(n>=0){first:=("A","1");second:=("B","2");pairs=second::pairs;pairs=first::pairs;}for(;pairs!=nil;pairs=tl pairs){(key,value):=hd pairs;if(envcount==0){envfirst=key;envfirstvalue=value;}envtext=key;envvalue=value;envvar=ref Sh->Var(key,nil,envcount);envcount++;}}
shellrecords(n:int){b:=ref Sh->Builtins(nil,n);vars:=array[7] of list of ref Sh->Var;prior:=ref Var("old",nil,n);hashadd(vars,3,prior);fresh:=ref Var("new",nil,n);hashadd(vars,3,fresh);hashfirst=hashfind(vars,3,"old");hashmiss=hashfind(vars,4,"old");l:=ref Sh->Localenv(vars,nil,n);e:=ref Sh->Environment(b,b,nil,l);c:=ref Sh->Context(e,nil,nil,nil);shellctx=c;env:=c.env;builtins:=env.builtins;localenv:=env.localenv;shellflags=builtins.n^localenv.flags;}
catchprobe(n:int){{if(n<0)raise "fail:fixture";}exception{"fail:*"=>catchvalue=17;}}
swappt(p:Point,n:int):Point{if(n<0)return p;return(p.y,p.x);}
mixed(n:int):(string,int){if(n<0)return (text(n),n);return ("positive",n);}
text(n:int):string{if(n<0)return "negative";return "positive";}firstword(r:(int,int)):int{return r.t0;}
five(n:int):(int,int,int,int,int){return(n,2,3,4,5);}
nested(n:int):((int,int),int){return((n,2),3);}
roots(n:int):(ref Draw->Context,string){if(n<0)return(nil,"negative");return(nil,"positive");}
tkpair(n:int):(ref Tk->Toplevel,chan of string){p:(ref Tk->Toplevel,chan of string);if(n<0)return(nil,nil);p=(nil,nil);return p;}
triple(n:int):(string,ref Draw->Image,string){v:(string,ref Draw->Image,string);if(n<0)v=("negative",nil,"end");else v=("positive",nil,"end");return v;}forward(n:int):(string,ref Draw->Image,string){return triple(n);}
'''
SOURCE+=''.join(re.search(r'(?m)^'+n+r'\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]for n in('splitqword','qslice','qword','s2r','bplong'))
SOURCE+=re.search(r'(?m)^select\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/winplace.b').read_text(),re.S)[0]
SOURCE+=re.search(r'(?m)^Lay\.mkr\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/winplace.b').read_text(),re.S)[0]
SOURCE+=''.join(re.search(r'(?m)^'+n+r'\([^\n]*\n\{.*?\n\}',(ROOT/'appl/cmd/sh/sh.b').read_text(),re.S)[0]for n in ('stringlist2list','revlist','hashadd','hashfind','hashfn','varfind','index','removebuiltinmod','builtinindex','builtinproviders','fdassignstr'))

SOURCE+=re.search(r'(?ms)^Context\.get\([^\n]*\n\{.*?^\}',(ROOT/'appl/cmd/sh/sh.b').read_text())[0].replace('Context.get(ctxt: self ref Context','shellget(ctxt: ref Sh->Context',1)

def generate(limbo,w):
 m=w/'aggregate.m';m.write_text(DECL);h=subprocess.check_output([str(limbo),'-H','-M','Winplace',str(m)],text=True);sigs={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Winplace_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Winplace',(38,0,0,'~0U'),export_signatures=sigs,source_label='aggregate fixture with original Wmlib and Winplace bodies')
 p=w/'full.h';gl={x[0]:x[1]for x in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint aggregate_globals[]={'+','.join(str(gl[n])for n in('out','word','a','b','c','d','e','ctx','image','last','parsed','position','quoted','nextq','left','middle','right','pointout','bytebuf','topout','eventout','nodes','menutext','catchvalue','converted','empty_nodes','shellctx','shellflags','envtext','envcount','envvalue','envfirst','envfirstvalue','envvar','hashfirst','hashmiss','lookupchild','lookupparent','lookuppos','lookupmiss','builtinmarker','builtinmatch','builtinmissing','builtintable','hit','miss','before','between','after','empty','syntaxtext','syntaxassign','syntaxword','syntaxkind','syntaxtype','syntaxnest'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);count=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(77,157,158,255):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,[],rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):count+=1
    else:raise AssertionError(('unchecked aggregate field',row['name'],pc,k))
 print('PASS aggregate source oracle:',count,'instruction corruptions detected',flush=True)
 for payload,extra in(('Draw->Point',''),('Other','Other:adt{a,b,c,d:int;};')):
  ty='list of '+payload;source='implement Closed;include "sys.m";include "draw.m";'+extra+'Closed:module{run:fn():('+ty+','+ty+','+ty+');};run():('+ty+','+ty+','+ty+'){return(nil,nil,nil);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 print('PASS aggregate boundary: Point and foreign record list triples refused',flush=True)
 for result in ('ref Tk->Toplevel,chan of int','ref Draw->Context,chan of string'):
  source='implement Closed;include "sys.m";include "draw.m";include "tk.m";Closed:module{run:fn():('+result+');};run():('+result+'){return(nil,nil);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert not closed[0]['accepted'],closed
 print('PASS Tk tuple boundary: wrong channel payload and wrong record refused',flush=True)
 source=(ROOT/'appl/wm/atoolbar.b').read_text()
 data,records=E['compile_source'](limbo,w,source,interface='',loads=True)
 imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 toolbar=next(r for r in records if r['name']=='toolbar')
 assert toolbar['accepted'] and any(e['name']=='toplevel' and e['contract']==116 for e in toolbar['externals']),toolbar
 CMD['verify_nilbranches'](data,toolbar,imports,rows=records)
 assert {'consoleproc','conout','initbuiltin','builtin_delmenu','delmenu','word','runbuiltin','builtin_menu'} <= {r['name'] for r in records if r['accepted']}
 for row in records:
  if row['name'] in ('initbuiltin','builtin_delmenu','delmenu','word','runbuiltin','builtin_menu'):
   CMD['verify_nilbranches'](data,row,imports,rows=records)
 print('PASS original atoolbar: checked Tk tuple import, destination map, console and menu removal',flush=True)
 init=next(r for r in records if r['name']=='init')
 assert init['accepted'] and any(e['name']=='Context.run' and e['contract']==24 for e in init['externals']),init
 [(_,catch)]=G['checked_catches'](data,records)
 assert catch['pattern']=='fail:*' and any(catch['cleanup'])
 print('PASS original atoolbar init: independent Dis exception range, slot, label and cleanup map',flush=True)
 data,records=E['compile_source'](limbo,w,(ROOT/'appl/lib/tkclient.b').read_text(),interface='',loads=True)
 assert len(records)==16 and all(r['accepted'] for r in records),[(r['name'],r.get('reason')) for r in records]
 top=next(r for r in records if r['name']=='toplevel')
 assert top['result_aggregate']==116
 print('PASS original Tkclient: all 16 bodies checked, including the Tk tuple provider',flush=True)
 shell=(ROOT/'appl/cmd/sh/sh.b').read_text()
 data,records=E['compile_source'](limbo,w,shell,interface='',loads=True)
 imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 for name in ('stringlist2list','revlist','hashadd','hashfind','newlocalenv','varfind','get','addmodule','getself','removebuiltinmod','builtinindex','builtinproviders','fdassignstr'):
  row=next(r for r in records if r['name']==name and (name!='get' or r['export_name']=='Context.get'))
  assert row['accepted'],row
  CMD['verify_nilbranches'](data,row,imports,rows=records)
 loaded=next(r for r in records if r['name']=='builtin_loaded')
 assert loaded['accepted'] and [v['operation'] for v in loaded['variadics']]==[2,2],loaded
 CMD['verify_nilbranches'](data,loaded,imports,rows=records)
 renderer=next(r for r in records if r['name']=='cmd2string')
 assert renderer['accepted'] and any(call[2]==renderer['first'] and not call[-1] for call in renderer['calls']),renderer
 CMD['verify_nilbranches'](data,renderer,imports,rows=records)
 quotation=next(r for r in records if r['name']=='quoted')
 assert quotation['accepted'] and any(call[2]==renderer['first'] for call in quotation['calls']),quotation
 substitution=next(r for r in records if r['name']=='sbuiltin_builtin')
 assert substitution['accepted'] and len(substitution['externals'])==1,substitution
 chain=('loadmodule','unloadmodule','builtin_load','builtin_unload')
 for name in chain:
  row=next(r for r in records if r['name']==name)
  assert row['accepted'],row
  assert row.get('calls'),row
  if name in ('loadmodule','unloadmodule'):
   CMD['verify_nilbranches'](data,row,imports,rows=records)
 context=next(r for r in records if r.get('export_name')=='Context.new')
 assert context['accepted'],context
 print('PASS original Sh: checked load, unload, command callers and Context.new autoload',flush=True)
 print('PASS original Sh: checked shell arguments, environment, builtin registration and removal',flush=True)
 for name in('Other','Lay'):
  source='implement Closed;'+name+':adt{d:int;};Closed:module{run:fn(x:'+name+');};run(x:'+name+'){take(x);}take(x:'+name+'){}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 print('PASS Lay argument boundary: foreign single Word ADT refused',flush=True)


 for name,expected in(('wmlib',25),('winplace',21)):
  source=(ROOT/('appl/lib/'+name+'.b')).read_text();data,records=E['compile_source'](limbo,w,source,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
  assert sum(r['accepted']for r in records)==expected,[(r['name'],r.get('reason'))for r in records]
  for row in records:
   if row['accepted']:CMD['verify_nilbranches'](data,row,imports,rows=records)
  old,_=E['compile_source'](limbo,w,source,interface='',loads=False);assert old==data
  print('PASS original '+name+':',expected,'/',len(records),'bodies accepted; independent operands and unchanged classic bytes',flush=True)

def unit():
 src=B['BASE']['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0]
 src=src.replace('WORD modstatus(REG*r,char*s,int n){USED(r);USED(s);USED(n);require(0);return 0;}',
                 'WORD modstatus(REG*r,char*s,int n){USED(r);if(n)s[0]=0;return 0;}')
 src=src.replace('static void*commandexresize(void*p,size_t n){USED(p);USED(n);require(0);return nil;}',
                 'static void*commandexresize(void*p,size_t n){USED(p);USED(n);return nil;}')
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start);handlers=src[start:end]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src=src.replace(extract(src,'int\nsnprint('),'',1);src+='\nvoid rerrstr(char*p,uint n){if(n)snprintf(p,n,"fixture");}\n'
 src=src.replace(extract(src,'void\ncvtup('),'void cvtup(Rune*,String*);',1);strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('void\ncvtup(','String*\nslicer(','OP(indc)','OP(insc)','OP(slicec)','OP(cvtcw)','OP(cvtwc)'):src+='\n'+extract(strings,sig)+'\n'
 vm=(ROOT/'libinterp/xec.c').read_text()
 src='#define TEST_REAL_MAPPED_CONS 1\n'+src
 src+='\n'+vm[vm.index('typedef struct ConsMapped'):vm.index('/* Preserve the destination, tail')]
 for sig in ('Type*\ncconsmappedtype(', 'OP(consmp)', 'OP(headmp)'):src+='\n'+extract(vm,sig)+'\n'
 for sig in ('static void\nbranchstring(', 'OP(bnec)', 'OP(blec)', 'OP(bgec)'):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 for sig in ('static void\nnewrecord(', 'OP(newz)'):src+='\n'+extract(vm,sig)+'\n'
 for sig in('static uchar*\narrayaddress(','static void\narrayindex(','static void\nnewarray(','OP(newaz)','OP(lena)','OP(indl)','OP(indx)','OP(indb)','OP(cvtwb)','OP(shrw)','OP(consm)','OP(headm)','OP(consp)','OP(headp)','OP(iraise)'):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 return src+'\n#line 1 "native64-aggregate-result.c"\n'+(ROOT/'utils/tests/native64-aggregate-result.c').read_text().replace(' AGGREGATE_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='aggregate-result-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for header,before,after,predicate in(
   ('native64-typed-module.h','if(flow!=complete)return 0;','if(0)return 0;','malformed aggregate or undefined path refused'),
   ('native64-typed-code.h','p->reg=(s->op==IMOVM)?(s->m>>8)&255:0;','p->reg=(s->op==IMOVM)?8:0;','original Wmlib s2r rectangle'),
   ('native64-typed-code.h','p->s.imm=s->s;p->d.i.f=48;p->d.i.s=0;','p->s.imm=s->s;p->d.i.f=48;p->d.i.s=1;','original Wmlib bplong bytes and guards')):
   for name in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/name).write_text((ROOT/'include'/name).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(header,result.returncode,result.stderr[-1000:])
   for name in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/name).unlink()
   print('PASS aggregate active control:',predicate,flush=True)
if __name__=='__main__':main()
