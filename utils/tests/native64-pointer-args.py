#!/usr/bin/env python3
"""Check exact Draw.Pointer by-value imported arguments from FP/MP roots."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
SERVICE='Sink:module{pointer:fn(p:Draw->Pointer);event:fn(w:ref Wmsrv->Window,p:Draw->Pointer):int;point:fn(p:Draw->Point);};'
NAMES=('direct','mp','method','methodmp','point','pointmp')
DECL='Pointerargs:module{'+''.join(n+':fn(p:ref Draw->Pointer);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Pointerargs;'+PREFIX+SERVICE+DECL+'sink:Sink;stored:ref Draw->Pointer;out:int;'
SOURCE+='direct(p:ref Draw->Pointer){sink->pointer(*p);}mp(p:ref Draw->Pointer){stored=p;sink->pointer(*stored);}method(p:ref Draw->Pointer){n:=sink->event(nil,*p);out=n;}methodmp(p:ref Draw->Pointer){stored=p;n:=sink->event(nil,*stored);out=n;}point(p:ref Draw->Pointer){sink->point(p.xy);}pointmp(p:ref Draw->Pointer){stored=p;sink->point(stored.xy);}open(path:string){sink=load Sink path;}'
def generate(limbo,w):
 m=w/'pointerargs.m';m.write_text(PREFIX+SERVICE+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Pointerargs','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Pointerargs_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Sink','-I',str(ROOT/'module'),str(m)],text=True);frames={}
 for n in('pointer','event','point'):
  size=int(re.search(r'^#define F_Sink_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Sink_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n,sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Pointerargs',(7,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint sink_offset='+str(gl['sink'])+';\nstatic const uint out_offset='+str(gl['out'])+';\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'pointer-native.h').write_text(h)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(227,230):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Pointer argument oracle missed',row['name'],pc,k))
 for row in records:
  if row['name']not in NAMES[:4]:continue
  for nominal in('Draw.Rect',None):
   bad=copy.deepcopy(row)
   if nominal is None:bad['externals'][0]['aggregate_args'][0].pop('nominal')
   else:bad['externals'][0]['aggregate_args'][0]['nominal']=nominal
   try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
   except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
   else:raise AssertionError('Pointer nominal oracle missed')
 assert roles=={227,230},roles
 print('PASS Pointer argument oracle:',controls,'corruptions detected; canonical signatures/layouts and unchanged classic bytes',flush=True)
 for owner,extra in(('Other','Other:adt{buttons:int;xy:Draw->Point;msec:int;};'),('Fake->Pointer','Fake:module{Pointer:adt{buttons:int;xy:Draw->Point;msec:int;};};')):
  source='implement Closed;'+PREFIX+extra+'Sink:module{pointer:fn(p:'+owner+');};Closed:module{run:fn(p:ref '+owner+');};sink:Sink;run(p:ref '+owner+'){sink->pointer(*p);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(closed)==1 and not closed[0]['accepted'],closed
 original=(ROOT/'module/draw.m').read_text()
 for old,new in(('buttons:\tint;','buttons:\tbig;'),('msec:\tint;','msec:\tbig;'),('xy:\t\tPoint;','xy:\t\tRect;'),('buttons:\tint;','renamed:\tint;')):
  assert old in original;(w/'baddraw.m').write_text(original.replace(old,new,1))
  source='implement Closed;'+PREFIX.replace('include "draw.m";','include "baddraw.m";')+SERVICE+'Closed:module{run:fn(p:ref Draw->Pointer);};sink:Sink;run(p:ref Draw->Pointer){sink->pointer(*p);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 for arg,body in(('p:Draw->Pointer','sink->pointer(p);'),('p:ref Draw->Pointer','q:=*p;sink->pointer(q);')):
  source='implement Closed;'+PREFIX+SERVICE+'Closed:module{run:fn('+arg+');};sink:Sink;run('+arg+'){'+body+'}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 print('PASS Pointer argument boundary: foreign ADT/namespace and changed canonical fields refused',flush=True)
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 r=next(r for r in records if r['name']=='init');assert r['accepted']or r['pc']>192,r;assert next(r for r in records if r['name']=='reshaped')['accepted'];print('AWM init frontier:',r.get('pc'),r.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-pointer-args.c"\n'+(ROOT/'utils/tests/native64-pointer-args.c').read_text().replace(' POINTER_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='pointer-args-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('short Pointer copy','native64-typed-multi.h','p->s.i.s=native64imagegeometryoffset(s->returned);p->reg=s->m;', 'p->s.i.s=native64imagegeometryoffset(s->returned);p->reg=s->returned==227?8:s->m;','ordered exact Word32 Pointer'),
   ('missing Pointer owner map','native64-typed-module.h','if(native64imagegeometryoffset(step->returned)!=~0U){bit=step->s/8;if(0==(map[bit/8]&(128>>(bit%8))))return 0;continue;}', 'if(native64imagegeometryoffset(step->returned)!=~0U){continue;}','malformed Pointer metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS Pointer argument active control:',name,flush=True)
if __name__=='__main__':main()
