#!/usr/bin/env python3
"""Execute flattened mixed, nested Word and mapped results through real local calls."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
B=runpy.run_path(str(ROOT/'utils/tests/native64-local-geometry-args.py'));G,CMD,E=B['G'],B['CMD'],B['E']
DECL='Winplace:module{run:fn(n:int);};'
SOURCE='''implement Winplace;include "sys.m";include "draw.m";Point,Rect:import Draw;'''+DECL+'''EW,NS:con iota;Lay:adt{d:int;mkr:fn(l:self Lay,r:Rect):Rect;};
pointout:Point;left,middle,right:list of Rect;quoted:string;nextq:int;parsed:Rect;position:int;out:string;word,a,b,c,d,e:int;ctx:ref Draw->Context;image:ref Draw->Image;last:string;
run(n:int){(s,v):=mixed(n);out=s;word=v;(v1,v2,v3,v4,v5):=five(n);a=v1;b=v2;c=v3;d=v4;e=v5;(range,z):=nested(n);i:=firstword(range);a=i;b=range.t1+i-i;c=z;(ct,tail):=roots(n);ctx=ct;last=tail;(first,im,tail2):=forward(n);out=first;image=im;last=tail2;(r,end):=s2r(" -2147483648 2147483647 -1 0",0);p:=swappt((n,1),n);pointout=p;lay:=Lay(n>=0);mapped:=lay.mkr(r);parsed=mapped;position=end;(q,stop):=qword(" 'a''b' rest",0);quoted=q;nextq=stop;input:list of Rect;if(n>=0){(lb,seed,rb):=select(nil,nil,0,1,3);input=seed;left=lb;right=rb;}(ls,ms,rs):=select(input,nil,1,2,3);left=ls;middle=ms;right=rs;}
swappt(p:Point,n:int):Point{if(n<0)return p;return(p.y,p.x);}
mixed(n:int):(string,int){if(n<0)return (text(n),n);return ("positive",n);}
text(n:int):string{if(n<0)return "negative";return "positive";}firstword(r:(int,int)):int{return r.t0;}
five(n:int):(int,int,int,int,int){return(n,2,3,4,5);}
nested(n:int):((int,int),int){return((n,2),3);}
roots(n:int):(ref Draw->Context,string){if(n<0)return(nil,"negative");return(nil,"positive");}
triple(n:int):(string,ref Draw->Image,string){v:(string,ref Draw->Image,string);if(n<0)v=("negative",nil,"end");else v=("positive",nil,"end");return v;}forward(n:int):(string,ref Draw->Image,string){return triple(n);}
'''
SOURCE+=''.join(re.search(r'(?m)^'+n+r'\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]for n in('splitqword','qslice','qword','s2r'))
SOURCE+=re.search(r'(?m)^select\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/winplace.b').read_text(),re.S)[0]
SOURCE+=re.search(r'(?m)^Lay\.mkr\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/winplace.b').read_text(),re.S)[0]
def generate(limbo,w):
 m=w/'aggregate.m';m.write_text(DECL);h=subprocess.check_output([str(limbo),'-H','-M','Winplace',str(m)],text=True);sigs={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Winplace_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Winplace',(16,0,0,'~0U'),export_signatures=sigs,source_label='aggregate fixture with original Wmlib and Winplace bodies')
 p=w/'full.h';gl={x[0]:x[1]for x in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint aggregate_globals[]={'+','.join(str(gl[n])for n in('out','word','a','b','c','d','e','ctx','image','last','parsed','position','quoted','nextq','left','middle','right','pointout'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);count=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]!=77:continue
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
 for name in('Other','Lay'):
  source='implement Closed;'+name+':adt{d:int;};Closed:module{run:fn(x:'+name+');};run(x:'+name+'){take(x);}take(x:'+name+'){}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 print('PASS Lay argument boundary: foreign single Word ADT refused',flush=True)


 for name,expected in(('wmlib',14),('winplace',12)):
  source=(ROOT/('appl/lib/'+name+'.b')).read_text();data,records=E['compile_source'](limbo,w,source,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
  assert sum(r['accepted']for r in records)==expected,[(r['name'],r.get('reason'))for r in records]
  for row in records:
   if row['accepted']:CMD['verify_nilbranches'](data,row,imports,rows=records)
  old,_=E['compile_source'](limbo,w,source,interface='',loads=False);assert old==data
  print('PASS original '+name+':',expected,'/',len(records),'bodies accepted; independent operands and unchanged classic bytes',flush=True)

def unit():
 src=B['BASE']['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0]
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start);handlers=src[start:end]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src=src.replace(extract(src,'int\nsnprint('),'',1);src+='\nvoid rerrstr(char*p,uint n){if(n)snprintf(p,n,"fixture");}\n'
 src=src.replace(extract(src,'void\ncvtup('),'void cvtup(Rune*,String*);',1);strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('void\ncvtup(','String*\nslicer(','OP(indc)','OP(insc)','OP(slicec)','OP(cvtcw)'):src+='\n'+extract(strings,sig)+'\n'
 vm=(ROOT/'libinterp/xec.c').read_text()
 for sig in('OP(consm)','OP(headm)'):src+='\n'+extract(vm,sig)+'\n'
 return src+'\n#line 1 "native64-aggregate-result.c"\n'+(ROOT/'utils/tests/native64-aggregate-result.c').read_text().replace(' AGGREGATE_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='aggregate-result-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for header,before,after,predicate in(
   ('native64-typed-module.h','if(flow!=complete)return 0;','if(0)return 0;','malformed aggregate or undefined path refused'),
   ('native64-typed-code.h','p->reg=(s->op==IMOVM)?(s->m>>8)&255:0;','p->reg=(s->op==IMOVM)?8:0;','original Wmlib s2r rectangle')):
   for name in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/name).write_text((ROOT/'include'/name).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(header,result.returncode,result.stderr[-1000:])
   for name in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/name).unlink()
   print('PASS aggregate active control:',predicate,flush=True)
if __name__=='__main__':main()
