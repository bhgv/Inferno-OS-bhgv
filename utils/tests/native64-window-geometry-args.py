#!/usr/bin/env python3
"""Check imported Window Rect/Point arguments against canonical Wmsrv layout."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
SERVICE='Sink:module{rect:fn(r:Draw->Rect);point:fn(p:Draw->Point);};'
NAMES=('rect','min','max','rectmp','minmp','maxmp','rectlocal','minlocal','maxlocal')
DECL='Windowargs:module{'+''.join(n+':fn(w:ref Wmsrv->Window);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Windowargs;'+PREFIX+SERVICE+DECL+'sink:Sink;stored:ref Wmsrv->Window;'
for i,n in enumerate(NAMES):
 owner='stored'if i>=3 else'w';part=('r','r.min','r.max')[i%3]
 expression=owner+'.'+part
 prefix='stored=w;'
 if i>=6:prefix+='local:=w.r;';expression='local'+(''if i%3==0 else'.min'if i%3==1 else'.max')
 SOURCE+=n+'(w:ref Wmsrv->Window){'+prefix+'sink->'+('rect'if i%3==0 else'point')+'('+expression+');}'
SOURCE+='open(path:string){sink=load Sink path;}'
def generate(limbo,w):
 m=w/'windowargs.m';m.write_text(PREFIX+SERVICE+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Windowargs','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Windowargs_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Sink','-I',str(ROOT/'module'),str(m)],text=True);frames={}
 for n in('rect','point'):
  size=int(re.search(r'^#define F_Sink_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Sink_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n,sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Windowargs',(10,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint sink_offset='+str(gl['sink'])+';\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);(w/'window-native.h').write_text(h)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(217,218,219):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Window argument oracle missed',row['name'],pc,k))
 assert roles=={217,218,219},roles
 print('PASS Window argument oracle:',controls,'corruptions detected; canonical signatures/layouts and unchanged classic bytes',flush=True)
 for owner,extra in(('Other','Other:adt{tag:string;r:Draw->Rect;img:ref Draw->Image;};'),('Fake->Window','Fake:module{Window:adt{tag:string;r:Draw->Rect;img:ref Draw->Image;};};')):
  source='implement Closed;'+PREFIX+SERVICE+extra+'Closed:module{run:fn(w:ref '+owner+');};sink:Sink;run(w:ref '+owner+'){sink->rect(w.r);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(closed)==1 and not closed[0]['accepted'],closed
 print('PASS Window argument boundary: foreign owner and namespace refused',flush=True)
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 r=next(r for r in records if r['name']=='reshaped');assert r['accepted']or r['pc']>=783,r;print('AWM reshaped frontier:',r.get('pc'),r.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-window-geometry-args.c"\n'+(ROOT/'utils/tests/native64-window-geometry-args.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='window-args-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong Window Rect offset','native64-typed-multi.h','tag==217 || tag==218?8:tag==219?16:', 'tag==217?0:tag==218?8:tag==219?16:','ordered exact Word32 geometry'),
   ('missing Window owner map','native64-typed-module.h','if(native64imagegeometryoffset(step->returned)!=~0U){bit=step->s/8;if(0==(map[bit/8]&(128>>(bit%8))))return 0;continue;}', 'if(native64imagegeometryoffset(step->returned)!=~0U){continue;}','malformed Window metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS Window argument active control:',name,flush=True)
if __name__=='__main__':main()
