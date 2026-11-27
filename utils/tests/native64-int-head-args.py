#!/usr/bin/env python3
"""Execute parsed String heads directly in Point fields and Word arguments."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
NAMES=('sub','mul','global','deep','mixed','repeat')
DECL='Inthead:module{'+''.join(n+':fn(xs:list of string);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Inthead;'+PREFIX+DECL+'draw:Draw;Point:import draw;hold:list of string;step,out:Point;'
SOURCE+='sub(xs:list of string){v:=Point(int hd xs,int hd tl xs).sub(step);out=v;}mul(xs:list of string){v:=Point(int hd xs,int hd tl xs).mul(int hd tl tl xs);out=v;}global(xs:list of string){hold=xs;v:=Point(int hd hold,int hd tl hold).sub(step);out=v;}'
SOURCE+='deep(xs:list of string){v:=Point(int hd tl tl xs,int hd tl tl tl xs).sub(step);out=v;}mixed(xs:list of string){v:=Point(-55,int hd tl xs).sub(step);out=v;}repeat(xs:list of string){a:=Point(int hd xs,int hd tl xs).sub(step);b:=Point(int hd xs,int hd tl xs).mul(int hd tl tl xs);v:=a.add(b);out=v;}open(path:string){draw=load Draw path;}'
def generate(limbo,w):
 m=w/'inthead.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Inthead','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Inthead_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 header=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'draw-native.h').write_text(header);frames={}
 for n in('Point_sub','Point_mul','Point_add'):
  size=int(re.search(r'^#define F_Draw_'+n+r'_size (\d+)',header,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Draw_'+n+r'_map \{([^}]*)\}',header,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Draw_'+n+r' (0x[0-9a-f]+)U',header,re.M)[1],16);frames[(n.replace('_','.'),sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Inthead',(7,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint int_globals[]={'+','.join(str(gl[n])for n in('draw','hold','step','out'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]!=253:continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('int head argument oracle missed',row['name'],k))
 print('PASS int head argument oracle:',controls,'corruptions detected; unchanged classic Dis',flush=True)
 for n in(32,33):
  source='implement Closed;'+PREFIX+'Closed:module{run:fn(xs:list of string);};draw:Draw;Point:import draw;out:Point;run(xs:list of string){v:=Point(int hd '+'tl '*n+'xs,1).mul(3);out=v;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert closed[0]['accepted']==(n==32),closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest','dragwin','sizewin'):
  row=next(r for r in records if r['name']==name);print('AWM',name,':',row['accepted'],row.get('pc'),row.get('reason'),flush=True)
 row=next(r for r in records if r['name']=='handlerequest');assert row['accepted'] or row['pc']>606
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\n#include "draw-native.h"\n'+extract((ROOT/'libinterp/heap.c').read_text(),'void\nfreestring(')+'\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(cvtcw)\n')+'\n'
 for name in('Point_sub','Point_mul','Point_add'):src+='\n#define F_'+name+' F_Draw_'+name+'\nvoid\n'+extract((ROOT/'libinterp/geom.c').read_text(),name+'(void *fp)')+'\n'
 return src+'\n#line 1 "native64-int-head-args.c"\n'+(ROOT/'utils/tests/native64-int-head-args.c').read_text().replace(' INT_HEAD_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='int-head-args-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wrong coordinate destination','native64-typed-multi.h','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d==64?68:s->d==68?64:s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}','exact Point coordinates and sub argument'),
   ('missing String temporary clear','native64-typed-multi.h','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else{native64typedcode(p,1,s,1,fpbytes,mpbytes);if(s->op==IMOVP){p->add=SRC(AFP)|DST(AFP);p->s.ind=s->d;}}}','String temporary cleared before geometry dispatch'),
   ('wide numeric conversion',None,'memmove(R.d, &value, sizeof(value));','{ LONG wide=value; memmove(R.d,&wide,sizeof(wide)); }','Word32 conversion preserves adjacent argument guard'))
  for name,header,before,after,predicate in controls:
   if header:
    for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
    text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1))
   else:
    text=unit();assert text.count(before)==1;(w/'unit.c').write_text(text.replace(before,after,1))
   subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):
    if(w/parent).exists():(w/parent).unlink()
   (w/'unit.c').write_text(unit())
   print('PASS numeric head argument active control:',name,flush=True)

if __name__=='__main__':main()
