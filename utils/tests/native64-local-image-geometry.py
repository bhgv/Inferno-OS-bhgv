#!/usr/bin/env python3
"""Execute ordinary Image/FP/MP Rect/Point copies with exact Word32 layouts."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
PARTS=('r','clipr','r.min','r.max','clipr.min','clipr.max');OFFSETS=(0,16,0,8,16,24)
NAMES=tuple(action+str(i)for action in('capture','publish','capturemp','publishmp')for i in range(6))+('swap','shift','alias')
DECL='Geometry:module{'+''.join(n+':fn(image:ref Draw->Image);'for n in NAMES)+'};'
SOURCE='implement Geometry;'+PREFIX+DECL+'stored:ref Draw->Image;pad:int;rectin,rectout,rectmirror:Draw->Rect;pointin,pointout,pointmirror:Draw->Point;count,guard:int;'
for index,name in enumerate(NAMES[:24]):
 part=PARTS[index%6];rect=index%6<2;capture=index<6 or 12<=index<18;mp=index>=12;owner='stored'if mp else'image';kind='rect'if rect else'point'
 body='stored=image;'
 if capture:
  body+=kind+'out='+owner+'.'+part+';'+kind+'mirror='+kind+'out;'if mp else'local:='+owner+'.'+part+';'+kind+'out=local;'+kind+'mirror=local;'
 else:
  body+=kind+'mirror='+kind+'in;'+owner+'.'+part+'='+kind+'mirror;'if mp else'local:='+kind+'in;'+kind+'mirror=local;'+owner+'.'+part+'=local;'
 SOURCE+=name+'(image:ref Draw->Image){'+body+'count++;if(guard== -99)count=pad;}'
for name,body in(('swap','a:=image.r;b:=image.clipr;image.r=b;image.clipr=a;'),('shift','p:=image.r.min;image.r.max=p;'),('alias','r:=image.r;image.r=r;')):SOURCE+=name+'(image:ref Draw->Image){stored=image;'+body+'count++;if(guard== -99)count=pad;}'
def generate(limbo,w):
 interface=w/'geometry.m';interface.write_text(PREFIX+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Geometry','-I',str(ROOT/'module'),str(interface)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Geometry_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Geometry',(27,0,0,'~0U'),export_signatures=signatures);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:
  out.write('static const uint geometry_globals[]={'+','.join(str(gl[n])for n in('stored','rectin','pointin','rectout','rectmirror','pointout','pointmirror','count','guard','pad'))+'};\n')
 (w/'draw-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(interface)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in range(208,213):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('geometry oracle missed',row['name'],pc,k))
 assert roles==set(range(208,213)),roles
 print('PASS local Image geometry source:',controls,'corruptions detected; five copy roles, canonical hashes, classic operands/maps and unchanged bytes',flush=True)
def boundaries(limbo,w):
 for ty,body in(('Other','r:=image.r;'),('Draw->Image','n:=image.depth;')):
  extra='Other:adt{r:Draw->Rect;};'if ty=='Other'else''
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(image:ref '+ty+');};run(image:ref '+ty+'){'+body+'}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);reshaped=next(r for r in rows if r['name']=='reshaped');assert reshaped['accepted']or reshaped['pc']>716,reshaped;init=next(r for r in rows if r['name']=='init')
 print('PASS local geometry boundaries; real reshaped:',reshaped.get('pc'),reshaped.get('reason'),'; AWM init:',init.get('pc'),init.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-local-image-geometry.c"\n'+(ROOT/'utils/tests/native64-local-image-geometry.c').read_text().replace(' GEOMETRY_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent geometry oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='local-image-geometry-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong Image Rect offset','native64-typed-code.h','if(s->returned==208 || s->returned==209){p->s.i.f=s->s;p->s.i.s=s->m>>8;', 'if(s->returned==208 || s->returned==209){p->s.i.f=s->s;p->s.i.s=s->m>>8;if(s->returned==208 && !(s->m>>8))p->s.i.s=16;','exact geometry outputs and all adjacent MP bytes'),
   ('overwide Point store','native64-typed-code.h','if(s->returned>=208 && s->returned<=212){p->reg=s->m&255;', 'if(s->returned>=208 && s->returned<=212){p->reg=s->m&255;if(s->returned==211 && (s->m>>8)==24)p->reg=16;','exact Image geometry and preserved roots/raw guard'),
   ('missing trailing raw-map proof','native64-typed-module.h','else for(at=0;at<width;at++){bit=(offsets[k]+at)/8;', 'else for(at=0;at<width && at<8;at++){bit=(offsets[k]+at)/8;','malformed local geometry metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS local geometry active control:',name,flush=True)
if __name__=='__main__':main()
