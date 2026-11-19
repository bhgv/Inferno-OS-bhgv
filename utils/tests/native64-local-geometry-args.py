#!/usr/bin/env python3
"""Execute exact Point/Rect arguments through real local Limbo calls and AWM r2s."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
NAMES=('globalrect','localrect','windowrect','mpwindow','pointfmt','mixed','chain','imagerect','screenrect','localscreen','screenmin','screenmax')
DECL='Localgeometry:module{'+''.join(n+':fn(w:ref Wmsrv->Window);'for n in NAMES)+'};'
SOURCE='implement Localgeometry;'+PREFIX+'Point,Rect:import Draw;'+DECL+'rect,seenrect:Rect;point,seenpoint:Point;stored:ref Wmsrv->Window;image:ref Draw->Image;screen:ref Draw->Screen;out,label:string;lead,trail:int;bign:big;'
SOURCE+='globalrect(w:ref Wmsrv->Window){out=r2s(rect);}localrect(w:ref Wmsrv->Window){local:=rect;out=r2s(local);}windowrect(w:ref Wmsrv->Window){out=r2s(w.r);}mpwindow(w:ref Wmsrv->Window){stored=w;out=r2s(stored.r);}pointfmt(w:ref Wmsrv->Window){out=p2s(rect.max);}mixed(w:ref Wmsrv->Window){out=mix(17,rect,-31,point,16r123456789abcdef0,"mix");}chain(w:ref Wmsrv->Window){local:=w.r;out=wrap(local);}'
SOURCE+='imagerect(w:ref Wmsrv->Window){out=r2s(image.r);}'
SOURCE+='screenrect(w:ref Wmsrv->Window){out=r2s(screen.image.r);}localscreen(w:ref Wmsrv->Window){local:=screen;out=r2s(local.image.r);}screenmin(w:ref Wmsrv->Window){out=p2s(screen.image.r.min);}screenmax(w:ref Wmsrv->Window){out=p2s(screen.image.clipr.max);}'
SOURCE+=re.search(r'r2s\(r: Rect\): string\s*\{.*?\n\}',(ROOT/'appl/wm/awm.b').read_text(),re.S)[0]
SOURCE+='p2s(p:Point):string{return string p.x+" "+string p.y;}wrap(r:Rect):string{return r2s(r);}mix(a:int,r:Rect,b:int,p:Point,n:big,s:string):string{lead=a;trail=b;seenrect=r;seenpoint=p;bign=n;label=s;return r2s(r);}'
def generate(limbo,w):
 m=w/'localgeometry.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Localgeometry','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Localgeometry_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Localgeometry',(16,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint geometry_globals[]={'+','.join(str(gl[n])for n in('rect','point','stored','out','label','lead','trail','bign','seenrect','seenpoint','image','screen'))+'};\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);draw=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'window-native.h').write_text(h+'\n'+'\n'.join(l for l in draw.splitlines()if l.startswith(('#define Draw_Image_map ','#define Draw_Screen_map ')))+'\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for first,end,target,*_ in row.get('calls',[]):
   for pc in range(first+1,end):
    if row['code'][pc][0]!=E['OPCODES']['IMOVM']and row['code'][pc][5]!=235:continue
    for k in range(6):
     bad=copy.deepcopy(row);bad['code'][pc][k]+=1
     try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
     except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
     else:raise AssertionError(('local geometry oracle missed',row['name'],k))
 print('PASS local geometry oracle:',controls,'corruptions detected; classic source/target leaves, canonical signatures and unchanged bytes',flush=True)
 for ty,extra,body in(('Other','Other:adt{x:int;y:int;};','take(x);'),('Draw->Rect','','spawn take(x);'),('Draw->Rect','','take(((1,2),(3,4)));')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(x:'+ty+');};run(x:'+ty+'){'+body+'}take(x:'+ty+'){}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 print('PASS local geometry boundary: other ADT, spawn and scalar literal aggregate remain closed',flush=True)
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 r=next(r for r in records if r['name']=='reshaped');assert r['accepted']or r['pc']>795,r;print('AWM reshaped frontier:',r.get('pc'),r.get('reason'),'; init:',[(x.get('pc'),x.get('reason'))for x in records if x['name']=='init'],flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 handlers=src[start:end]
 src+='\nvoid rerrstr(char*p,uint n){if(n)snprintf(p,n,\"fixture\");}\n'
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src=src.replace(extract(src,'int\nsnprint('),'',1);src+='\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(cvtwc)')+'\n'
 return src+'\n#line 1 "native64-local-geometry-args.c"\n'+(ROOT/'utils/tests/native64-local-geometry-args.c').read_text().replace(' WINDOW_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='local-geometry-args-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong Screen image field','native64-typed-multi.h','else if(s->returned==166 || s->returned==235){checkstep=*s;checkstep.returned=Native64TypedDraw;', 'else if(s->returned==166 || s->returned==235){checkstep=*s;if(s->returned==235)checkstep.m=16;checkstep.returned=Native64TypedDraw;','exact completion/error'),
   ('short local Rect copy','native64-typed-multi.h','else if(s->op==IMOVM)p->reg=s->m;', 'else if(s->op==IMOVM)p->reg=(!c->external && s->m==16)?8:s->m;','exact real r2s/p2s signed decimal String'),
   ('missing callee raw map','native64-typed-multi.h','for(i=0;i<(unsigned long)s->m;i+=4){bit=(s->d+i)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}', 'for(i=0;i<(unsigned long)s->m;i+=4){bit=(s->d+i)/8;if(0)return 0;}','malformed local geometry metadata refused'),
   ('missing caller raw map','native64-typed-module.h','for(j=0;j<(unsigned long)step->m;j+=4){bit=(step->s+j)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}', 'for(j=0;j<(unsigned long)step->m;j+=4){bit=(step->s+j)/8;if(0)return 0;}','malformed local geometry metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS local geometry active control:',name,flush=True)
if __name__=='__main__':main()
