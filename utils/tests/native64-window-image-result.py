#!/usr/bin/env python3
"""Execute checked Image result publication into a rooted local Window owner."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
NAMES=('direct','local','twice')
DECL='Windowimage:module{'+''.join(n+':fn(w:ref Wmsrv->Window);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Windowimage;'+PREFIX+DECL+'draw:Draw;Screen:import draw;screen:ref Draw->Screen;box:Draw->Rect=((1,2),(3,4));'
SOURCE+='direct(w:ref Wmsrv->Window){w.img=screen.newwindow(w.r,1,0);}'
SOURCE+='local(w:ref Wmsrv->Window){a:=w;a.img=screen.newwindow(box,1,0);}'
SOURCE+='twice(w:ref Wmsrv->Window){w.img=screen.newwindow(box,1,0);w.img=screen.newwindow(box,1,0);}'
SOURCE+='open(path:string){draw=load Draw path;}'
def generate(limbo,w):
 m=w/'windowimage.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Windowimage','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Windowimage_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'draw-native.h').write_text(h);frames={}
 for n in('Screen_newwindow',):
  size=int(re.search(r'^#define F_Draw_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Draw_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Draw_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n.replace('_','.'),sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Windowimage',(4,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint result_globals[]={'+','.join(str(gl[n])for n in('draw','screen','box'))+'};\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);(w/'window-native.h').write_text('typedef struct Wmsrv_Window Wmsrv_Window;\n'+re.search(r'struct Wmsrv_Window \{.*?\n\};',h,re.S)[0]+'\n'+next(l for l in h.splitlines()if l.startswith('#define Wmsrv_Window_map '))+'\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for j,e in enumerate(row.get('externals',[])):
   pc=e['end']-1;assert row['code'][pc][5]==220 and e['contract']==24
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Window result oracle missed',row['name'],k))
 print('PASS Window result oracle:',controls,'corruptions detected; canonical signatures/layouts and unchanged classic bytes',flush=True)
 for extra,owner,destination in(('', 'Wmsrv->Window','saved'),('Other:adt{tag:string;r:Draw->Rect;img:ref Draw->Image;};','Other','w'),('Fake:module{Window:adt{tag:string;r:Draw->Rect;img:ref Draw->Image;};};','Fake->Window','w')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(w:ref '+owner+');};draw:Draw;Screen:import draw;screen:ref Draw->Screen;saved:ref '+owner+';run(w:ref '+owner+'){saved=w;'+destination+'.img=screen.newwindow(((1,2),(3,4)),1,0);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(closed)==1 and not closed[0]['accepted'],closed
 print('PASS Window result boundary: MP owner, foreign layout and namespace refused',flush=True)
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 r=next(r for r in records if r['name']=='reshaped');assert r['accepted']or r['pc']>783,r;print('AWM reshaped frontier:',r.get('pc'),r.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-window-image-result.c"\n'+(ROOT/'utils/tests/native64-window-image-result.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='window-image-result-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong field displacement','native64-typed-multi.h','else if(s->returned==220){p->s.i.f=s->s;p->s.i.s=24;', 'else if(s->returned==220){p->s.i.f=s->s;p->s.i.s=0;','exact completion/error'),
   ('missing heap img root','native64-local-bind.h',' || ownertype->map[0]!=0x90','', 'exact completion/error'),
   ('missing owner slot root proof','native64-typed-module.h','bit=step->s/8;if(!(f->map[bit/8]&(128>>(bit%8))))return 0;', 'bit=step->s/8;if(0)return 0;','malformed Window result metadata refused')):
   for parent in('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS Window result active control:',name,flush=True)
if __name__=='__main__':main()
