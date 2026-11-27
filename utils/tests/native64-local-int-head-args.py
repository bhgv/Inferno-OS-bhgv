#!/usr/bin/env python3
"""Execute parsed Point/Rect/int literals through real local Limbo calls."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
NAMES=('point','scalar','global','deep','mixed','repeat','rect','plain')
SERVICE='Audit:module{check:fn();};'
DECL='Localheads:module{'+''.join(n+':fn(xs:list of string);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Localheads;'+PREFIX+SERVICE+DECL+'Point,Rect:import Draw;audit:Audit;hold:list of string;pointout:Point;rectout:Rect;factor,count,fail:int;'
SOURCE+='point(xs:list of string){takepoint(xs,xs,xs,Point(int hd xs,int hd tl xs),-17);}scalar(xs:list of string){takepoint(xs,xs,xs,(10,-11),int hd tl tl xs);}global(xs:list of string){hold=xs;takepoint(xs,xs,xs,Point(int hd hold,int hd tl hold),-17);}'
SOURCE+='deep(xs:list of string){takepoint(xs,xs,xs,Point(int hd tl tl xs,int hd tl tl tl xs),-17);}mixed(xs:list of string){takerect(((-55,int hd tl xs),(int hd xs,97)),int hd tl tl xs);}repeat(xs:list of string){takepoint(xs,xs,xs,Point(int hd xs,int hd tl xs),-17);takepoint(xs,xs,xs,Point(int hd xs,int hd tl xs),int hd tl tl xs);}'
SOURCE+='rect(xs:list of string){takerect(((int hd xs,int hd tl xs),(int hd tl tl xs,int hd tl tl tl xs)),-17);}plain(xs:list of string){takerect(((1,2),(3,4)),-17);}'
SOURCE+='takepoint(a,b,c:list of string,p:Point,n:int){audit->check();if(fail==count+1)raise "fail:local";pointout=p;factor=n;count++;}takerect(r:Rect,n:int){audit->check();if(fail==count+1)raise "fail:local";rectout=r;factor=n;count++;}open(path:string){audit=load Audit path;}'
def generate(limbo,w):
 m=w/'localheads.m';m.write_text(PREFIX+SERVICE+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Localheads','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Localheads_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Audit','-I',str(ROOT/'module'),str(m)],text=True);(w/'audit-native.h').write_text(h)
 size=int(re.search(r'^#define F_Audit_check_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Audit_check_map \{([^}]*)\}',h,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Audit_check (0x[0-9a-f]+)U',h,re.M)[1],16)
 rows=G['generate'](limbo,w,SOURCE,'Localheads',(11,1,0,'~0U'),import_frames={('check',sig):(size,mapping)},export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint local_globals[]={'+','.join(str(gl[n])for n in('audit','hold','pointout','rectout','factor','count','fail'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]!=253:continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('local int head oracle missed',row['name'],pc,k))
 print('PASS local int head oracle:',controls,'corruptions detected; unchanged classic Dis',flush=True)
 for n in(32,33):
  source='implement Closed;'+PREFIX+'Closed:module{run:fn(xs:list of string);};Point:import Draw;out:Point;run(xs:list of string){take(Point(int hd '+'tl '*n+'xs,1));}take(p:Point){out=p;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert next(r for r in closed if r['name']=='run')['accepted']==(n==32),closed
 for tail in('spawn take(Point(int hd xs,1));','spawn take((1,2));'):
  source='implement Closed;'+PREFIX+'Closed:module{run:fn(xs:list of string);};Point:import Draw;out:Point;run(xs:list of string){'+tail+'}take(p:Point){out=p;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest','sizewin'):
  row=next(r for r in records if r['name']==name);print('AWM',name,':',row['accepted'],row.get('pc'),row.get('reason'),flush=True)
 row=next(r for r in records if r['name']=='handlerequest');assert row['accepted'] or row['pc']>618 or row['pc']==618 and row['reason']=='multi-callee'
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\n'+extract((ROOT/'libinterp/heap.c').read_text(),'void\nfreestring(')+'\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(cvtcw)\n')+'\n'
 return src+'\n#line 1 "native64-local-int-head-args.c"\n'+(ROOT/'utils/tests/native64-local-int-head-args.c').read_text().replace(' LOCAL_HEAD_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='local-int-head-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wrong coordinate destination','native64-typed-multi.h','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d==88?92:s->d==92?88:s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}','exact local Point Rect and scalar arguments'),
   ('missing String temporary clear','native64-typed-multi.h','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}','else if(s->returned==253){if(s->op==ICVTCW){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else{native64typedcode(p,1,s,1,fpbytes,mpbytes);if(s->op==IMOVP){p->add=SRC(AFP)|DST(AFP);p->s.ind=s->d;}}}','String temporary cleared before local dispatch'),
   ('wide numeric conversion',None,'memmove(R.d, &value, sizeof(value));','{ LONG wide=value; memmove(R.d,&wide,sizeof(wide)); }','local Word32 conversion preserves adjacent guard'))
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
   print('PASS local numeric head active control:',name,flush=True)

if __name__=='__main__':main()
