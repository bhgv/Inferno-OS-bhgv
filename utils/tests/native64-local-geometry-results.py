#!/usr/bin/env python3
"""Execute raw Point/Rect returns across local Limbo calls and real AWM fitrect."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
METHODS=('Rect_dx','Rect_dy','Rect_size')
NAMES=('globalrect','localrect','branch','point','globalpoint','nested','fit','windowrect')
DECL='Localresults:module{'+''.join(n+':fn(w:ref Wmsrv->Window);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Localresults;'+PREFIX+DECL+'draw:Draw;Point,Rect:import draw;rectin,other,rectout:Rect;pointin,pointout:Point;selector,after:int;'
EXPR=('fromglobal()','copyrect(rectin)','choose(rectin,other,selector)','copypoint(pointin)','frompoint()','intermediate(rectin)','fitrect(rectin,other)','copyrect(w.r)')
for i,n in enumerate(NAMES):
 SOURCE+=n+'(w:ref Wmsrv->Window){n:=selector;v:='+EXPR[i]+';'+('pointout'if i in(3,4)else'rectout')+'=v;after=n;}'
SOURCE+='open(path:string){draw=load Draw path;}fromglobal():Rect{return rectin;}copyrect(r:Rect):Rect{return r;}choose(r,s:Rect,n:int):Rect{if(n==99)raise "fail:rectangle";if(n>0)return r;return s;}copypoint(p:Point):Point{return p;}frompoint():Point{return pointin;}intermediate(r:Rect):Rect{s:=copyrect(r);return s;}'
SOURCE+=re.search(r'fitrect\(w, r: Rect\): Rect\s*\{.*?\n\}',(ROOT/'appl/wm/awm.b').read_text(),re.S)[0]
def generate(limbo,w):
 m=w/'localresults.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Localresults','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Localresults_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'draw-native.h').write_text(h);frames={}
 for n in METHODS:
  size=int(re.search(r'^#define F_Draw_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Draw_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Draw_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n.replace('_','.'),sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Localresults',(16,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint result_globals[]={'+','.join(str(gl[n])for n in('draw','rectin','other','rectout','pointin','pointout','selector','after'))+'};\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);(w/'window-native.h').write_text('typedef struct Wmsrv_Window Wmsrv_Window;\n'+re.search(r'struct Wmsrv_Window \{.*?\n\};',h,re.S)[0]+'\n'+next(l for l in h.splitlines()if l.startswith('#define Wmsrv_Window_map '))+'\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set();alignment=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(246,247):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('local geometry return oracle missed',row['name'],k))
  for j,site in enumerate(row.get('calls',[])):
   if site[5]not in(96,112):continue
   pc=site[1]-1;alignment.add(row['code'][pc][2]%8)
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('local geometry binding oracle missed',row['name'],k))
 assert roles=={246,247}and 4 in alignment,(roles,alignment)
 print('PASS local geometry result oracle:',controls,'corruptions detected; source/caller raw leaves and unchanged classic Dis',flush=True)
 for result,extra in(('Other','Other:adt{a,b,c,d:int;};'),('Fake->Rect','Fake:module{Rect:adt{a,b,c,d:int;};};'),('Draw->Pointer','')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn();};saved:'+result+';run(){v:=take();}take():'+result+'{return saved;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='take')['accepted'],closed
 for body in('out=copy(r);','v:=relay(r);out=v;'):
  source='implement Closed;'+PREFIX+'Closed:module{run:fn(r:Draw->Rect);};out:Draw->Rect;run(r:Draw->Rect){'+body+'}copy(r:Draw->Rect):Draw->Rect{return r;}relay(r:Draw->Rect):Draw->Rect{return copy(r);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest','fitrect','newrect'):
  row=next(r for r in records if r['name']==name);print('AWM',name,':',row['accepted'],row.get('pc'),row.get('reason'),flush=True)
 assert next(r for r in records if r['name']=='fitrect')['accepted']
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\nstatic unsigned int calls;\n#include \"draw-native.h\"\n'
 for name in METHODS:
  src+='\n#define F_'+name+' F_Draw_'+name+'\nvoid\n'+extract((ROOT/'libinterp/geom.c').read_text(),name+'(void *fp)').replace('{','{\n calls++;',1)+'\n'
 return src+'\n#line 1 "native64-local-geometry-results.c"\n'+(ROOT/'utils/tests/native64-local-geometry-results.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='local-geometry-results-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('truncated whole result','native64-typed-code.h','if(s->returned==246 || s->returned==247){p->s.ind=s->s;p->d.i.f=32;p->d.i.s=0;continue;}', 'if(s->returned==246 || s->returned==247){p->reg=4;p->s.ind=s->s;p->d.i.f=32;p->d.i.s=0;continue;}','exact raw result and adjacent MP bytes'),
   ('truncated source GC proof','native64-typed-module.h','map=USRC(step->mode)==AFP?f->map:s->datamap;for(at=0;at<width;at++){bit=(step->s+at)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}', 'map=USRC(step->mode)==AFP?f->map:s->datamap;for(at=0;at<4;at++){bit=(step->s+at)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}','malformed local result metadata refused'),
   ('wrong caller result address','native64-typed-multi.h','if(c->contract && !c->inherited && j==c->end-1)p->reg=c->contract;', 'if(c->contract && !c->inherited && j==c->end-1){p->reg=c->contract;if(c->contract==112)p->s.ind-=4;}','exact raw result and adjacent MP bytes'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS local geometry result active control:',name,flush=True)
if __name__=='__main__':main()
