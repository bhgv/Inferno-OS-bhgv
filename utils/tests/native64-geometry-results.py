#!/usr/bin/env python3
"""Execute imported Draw Point/Rect results through the actual geom.c bodies."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
METHODS=('Rect_subpt','Rect_addpt','Rect_inset','Point_add','Point_sub','Point_mul')
NAMES=('rectsub','rectadd','rectinset','pointadd','pointsub','pointmul','windowrect','windowmin','windowmax','chain')+tuple('mp'+n for n in('rectsub','rectadd','rectinset','pointadd','pointsub','pointmul','windowrect','windowmin','windowmax'))
DECL='Results:module{'+''.join(n+':fn(w:ref Wmsrv->Window);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Results;'+PREFIX+DECL+'draw:Draw;Point,Rect:import draw;pad:int;rectin,rectout:Draw->Rect;pointin,step,pointout:Draw->Point;guard:int;'
EXPR=('rectin.subpt(pointin)','rectin.addpt(pointin)','rectin.inset(3)','pointin.add(step)','pointin.sub(step)','pointin.mul(3)','w.r.subpt(pointin)','w.r.min.add(step)','w.r.max.sub(step)')
for i,n in enumerate(NAMES[:9]):
 SOURCE+=n+'(w:ref Wmsrv->Window){local:='+EXPR[i]+';'+('rectout'if i in(0,1,2,6)else'pointout')+'=local;if(guard== -99)guard=pad;}'
SOURCE+='chain(w:ref Wmsrv->Window){a:=w.r.subpt(pointin);local:=a.addpt(step);rectout=local;if(guard== -99)guard=pad;}'
for i,n in enumerate(NAMES[10:]):SOURCE+=n+'(w:ref Wmsrv->Window){'+('rectout'if i in(0,1,2,6)else'pointout')+'='+EXPR[i]+';if(guard== -99)guard=pad;}'
SOURCE+='open(path:string){draw=load Draw path;}'
def generate(limbo,w):
 m=w/'results.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Results','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Results_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'draw-native.h').write_text(h);frames={}
 for n in METHODS:
  size=int(re.search(r'^#define F_Draw_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Draw_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Draw_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n.replace('_','.'),sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Results',(20,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint result_globals[]={'+','.join(str(gl[n])for n in('draw','pad','rectin','rectout','pointin','step','pointout','guard'))+'};\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);(w/'window-native.h').write_text('typedef struct Wmsrv_Window Wmsrv_Window;\n'+re.search(r'struct Wmsrv_Window \{.*?\n\};',h,re.S)[0]+'\n'+next(l for l in h.splitlines()if l.startswith('#define Wmsrv_Window_map '))+'\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for j,e in enumerate(row.get('externals',[])):
   assert e['contract']in(96,112)and e['resultoffset']%4==0;roles.add(e['contract'])
   for key in('contract','result_geometry','resultoffset','inherited'):
    bad=copy.deepcopy(row);bad['externals'][j][key]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('geometry result oracle missed',row['name'],key))
   pc=e['end']-1
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('geometry binding oracle missed',row['name'],k))
 assert roles=={96,112},roles
 print('PASS geometry result oracle:',controls,'corruptions detected; canonical signatures/layouts and unchanged classic bytes',flush=True)
 for extra,result,body in(('Other:adt{x:int;y:int;};','Other','v:=draw->take();'),('','Draw->Rect','return draw->take();')):
  tail=':Draw->Rect'if body.startswith('return')else''
  source='implement Closed;'+PREFIX+extra+'Fake:module{take:fn():'+result+';};Closed:module{run:fn()'+tail+';};draw:Fake;out:Draw->Rect;run()'+tail+'{'+body+'}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(closed)==1 and not closed[0]['accepted'],closed
 print('PASS geometry result boundary: foreign ADT and inherited results refused; direct MP results exercised',flush=True)
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 r=next(r for r in records if r['name']=='reshaped');assert r['accepted']or r['pc']>783,r;print('AWM reshaped frontier:',r.get('pc'),r.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\nstatic unsigned int calls;\n#include \"draw-native.h\"\n'
 for name in METHODS:
  src+='\n#define F_'+name+' F_Draw_'+name+'\nvoid\n'+extract((ROOT/'libinterp/geom.c').read_text(),name+'(void *fp)').replace('{','{\n calls++;',1)+'\n'
 return src+'\n#line 1 "native64-geometry-results.c"\n'+(ROOT/'utils/tests/native64-geometry-results.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='geometry-results-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('Rect treated as reference','native64-local-bind.h','reference=!native64geometryresultwidth(contract) && (contract==72 || !!(contract&16));','reference=contract==72 || !!(contract&16);','exact completion/error'),
   ('truncated raw result proof','native64-geometry-result.h','for(i=0;i<width;i++){bit=', 'for(i=0;i<4;i++){bit=','malformed geometry result metadata refused'),
   ('wrong result address','native64-typed-multi.h','if(c->contract && !c->inherited && j==c->end-1)p->reg=c->contract;', 'if(c->contract && !c->inherited && j==c->end-1){p->reg=c->contract;if(c->contract==112)p->s.ind+=4;}','exact geometry result and adjacent MP bytes')):
   for parent in('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h','native64-geometry-result.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h','native64-geometry-result.h'):(w/parent).unlink()
   print('PASS geometry result active control:',name,flush=True)
if __name__=='__main__':main()
