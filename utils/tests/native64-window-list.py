#!/usr/bin/env python3
"""Execute exact Wmsrv.Window lists, fields and local geometry in C/JIT."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
ARGS='l:list of ref Wmsrv->Window,a,b:ref Wmsrv->Window'
NAMES=('scan','scanmp','reverse','build','head','tail','selftail','consalias','fields','clearimg','cleartag')
DECL='Windowlist:module{'+''.join(n+':fn('+ARGS+');'for n in NAMES)+'};'
SOURCE='implement Windowlist;'+PREFIX+DECL+'saved,made,rest:list of ref Wmsrv->Window;last:ref Wmsrv->Window;tag:string;image:ref Draw->Image;rect,mirror:Draw->Rect;point,pointmirror:Draw->Point;count,sum,guard:int;'
VISIT='count++;if(w!=nil){last=w;tag=w.tag;image=w.img;r:=w.r;rect=r;mirror=rect;p:=w.r.min;point=p;pointmirror=point;sum+=r.min.x;w.tag=tag;w.img=image;w.r=r;w.r.min=p;}'
BODIES=['saved=l;visit(l);','saved=l;for(;saved!=nil;saved=tl saved){w:=hd saved;'+VISIT+'}', 'saved=l;rev:list of ref Wmsrv->Window;for(p:=l;p!=nil;p=tl p){w:=hd p;rev=w::rev;}made=rev;visit(rev);', 'saved=l;p:list of ref Wmsrv->Window;p=a::p;p=b::p;p=nil::p;made=p;visit(p);','saved=l;last=hd l;rest=tl l;','saved=l;rest=tl saved;','saved=l;saved=tl saved;rest=saved;','saved=l;saved=a::saved;made=saved;','saved=l;last=a;tag=a.tag;image=a.img;rect=a.r;mirror=rect;point=a.r.min;pointmirror=point;a.tag=tag;a.img=image;a.r=rect;a.r.max=point;count=1;', 'saved=l;a.img=nil;','saved=l;a.tag=nil;']
for n,b in zip(NAMES,BODIES):SOURCE+=n+'('+ARGS+'){'+b+'}'
SOURCE+='visit(l:list of ref Wmsrv->Window){for(p:=l;p!=nil;p=tl p){w:=hd p;'+VISIT+'}if(guard== -99)count=99;}'
def generate(limbo,w):
 m=w/'windowlist.m';m.write_text(PREFIX+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Windowlist','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Windowlist_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Windowlist',(12,0,0,'~0U'),export_signatures=signatures);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:out.write('static const uint window_globals[]={'+','.join(str(gl[n])for n in('saved','made','rest','last','tag','image','rect','mirror','point','pointmirror','count','sum','guard'))+'};\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);draw=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);h+='\n'+next(l for l in draw.splitlines()if l.startswith('#define Draw_Image_map '))+'\n';(w/'window-native.h').write_text(h)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(213,214,215):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('window list oracle missed',row['name'],pc,k))
 assert roles=={213,214,215},roles
 print('PASS Window list source:',controls,'corruptions detected; canonical hashes, classic list/field/geometry operands and unchanged bytes',flush=True)
def boundaries(limbo,w):
 for ty in('Draw->Image','Wmsrv->Client'):
  s='implement Closed;'+PREFIX+'Closed:module{run:fn(l:list of ref '+ty+');};out:ref '+ty+';run(l:list of ref '+ty+'){out=hd l;}'
  _,r=E['compile_source'](limbo,w,s,interface='',loads=True);assert not r[0]['accepted'],r
 original=(ROOT/'module/wmsrv.m').read_text()
 for before,after in(('img:\tref Draw->Image;', 'img:\tref Draw->Screen;'),('tag:\tstring;', 'tag:\tchan of string;')):
  assert before in original;(w/'badwmsrv.m').write_text(original.replace(before,after,1))
  s='implement Closed;include "sys.m";include "draw.m";include "badwmsrv.m";Closed:module{run:fn(l:list of ref Wmsrv->Window);};out:ref Wmsrv->Window;run(l:list of ref Wmsrv->Window){out=hd l;}'
  _,r=E['compile_source'](limbo,w,s,interface='',loads=True);assert not r[0]['accepted'],r
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);reshaped=next(r for r in rows if r['name']=='reshaped');assert reshaped['accepted']or reshaped['pc']>764,reshaped;init=next(r for r in rows if r['name']=='init');assert next(r for r in rows if r['name']=='command')['accepted']
 print('PASS Window list boundaries; real reshaped:',reshaped.get('pc'),reshaped.get('reason'),'; AWM init:',init.get('pc'),init.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];before='cons(ulong size, void *slot)\n{';assert src.count(before)==1;src='static int wlconsfail,wlconscount;\n'+src.replace(before,before+'\n if(wlconsfail && ++wlconscount==wlconsfail)failat=allocations+1;',1);start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-window-list.c"\n'+(ROOT/'utils/tests/native64-window-list.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent Window list oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='window-list-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong Window cons source','native64-typed-code.h','if(s->returned==213){p->reg=0;p->d.ind=s->d;continue;}', 'if(s->returned==213){p->reg=0;p->d.ind=s->d;if(s->op==ICONSP && USRC(s->mode)==AFP && s->s==72)p->s.ind+=8;continue;}','exact Window list traversal and captured roots'),
   ('wrong Window Rect offset','native64-typed-code.h','p->s.i.s=s->m>>8;p->d.ind=s->d;}else if(s->returned==212)', 'p->s.i.s=s->m>>8;if(s->returned==215 && (s->m&255)==16)p->s.i.s=0;p->d.ind=s->d;}else if(s->returned==212)','exact Window list traversal and captured roots'),
   ('missing list destination root proof','native64-typed-module.h','if(k && step->returned && step->returned!=213)continue;', 'if(k && step->returned)continue;','malformed Window metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS Window list active control:',name,flush=True)
if __name__=='__main__':main()
