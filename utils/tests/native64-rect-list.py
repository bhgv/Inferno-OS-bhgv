#!/usr/bin/env python3
"""Execute raw Rect lists and unchanged AWM newrect with controlled placement."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
NAMES=('prepend','local','window','mpwindow','head','headglobal','tail','place')
DECL='Rectlist:module{'+''.join(n+':fn(w:ref Wmsrv->Window);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Rectlist;'+PREFIX+'include "wmsrv.m";include "winplace.m";'+DECL+'draw:Draw;wmsrv:Wmsrv;winplace:Winplace;Point,Rect:import draw;rect,screen,out,lastrect:Rect;saved:list of Rect;hold:ref Wmsrv->Window;'
SOURCE+='prepend(w:ref Wmsrv->Window){saved=rect::saved;}local(w:ref Wmsrv->Window){r:=rect;xs:=saved;xs=r::xs;saved=xs;}window(w:ref Wmsrv->Window){saved=w.r::saved;}mpwindow(w:ref Wmsrv->Window){hold=w;saved=hold.r::saved;}'
SOURCE+='head(w:ref Wmsrv->Window){r:=hd saved;out=r;}headglobal(w:ref Wmsrv->Window){out=hd saved;}tail(w:ref Wmsrv->Window){saved=tl saved;}place(w:ref Wmsrv->Window){r:=newrect(rect,screen);out=r;}'
SOURCE+='open(path:string){draw=load Draw path;wmsrv=load Wmsrv path;winplace=load Winplace path;}'
SOURCE+=re.search(r'newrect\(w, r: Rect\): Rect\s*\{.*?\n\}',(ROOT/'appl/wm/awm.b').read_text(),re.S)[0].replace('\nlastrect: Rect;','')
def generate(limbo,w):
 m=w/'rectlist.m';m.write_text(PREFIX+'include "wmsrv.m";include "winplace.m";'+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Rectlist','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Rectlist_(\w+) (0x[0-9a-f]+)U',h,re.M)};frames={}
 for module,methods in(('Draw',('Rect_size',)),('Wmsrv',('top',)),('Winplace',('place',))):
  header=subprocess.check_output([str(limbo),'-H','-M',module,'-I',str(ROOT/'module'),str(m)],text=True)
  if module=='Draw':(w/'draw-native.h').write_text(header)
  else:
   names=('Wmsrv_Window','Wmsrv_Client','F_Wmsrv_top')if module=='Wmsrv'else('F_Winplace_place',)
   text='\n'.join('typedef struct '+n+' '+n+';'for n in names)+'\n'
   for n in names:text+=re.search(r'struct '+n+r' \{.*?\n\};',header,re.S)[0]+'\n'+('\n'.join(l for l in header.splitlines()if l.startswith('#define '+n+'_map ')))+'\n'
   (w/(module.lower()+'-native.h')).write_text(text)
  for n in methods:
   size=int(re.search(r'^#define F_'+module+'_'+n+r'_size (\d+)',header,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_'+module+'_'+n+r'_map \{([^}]*)\}',header,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_'+module+'_'+n+r' (0x[0-9a-f]+)U',header,re.M)[1],16);frames[(n.replace('_','.'),sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Rectlist',(10,3,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint rect_globals[]={'+','.join(str(gl[n])for n in('draw','wmsrv','winplace','rect','screen','out','lastrect','saved','hold'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(248,249,250):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Rect list oracle missed',row['name'],k))
 assert roles=={248,249,250},roles
 print('PASS Rect list oracle:',controls,'corruptions detected; unchanged classic Dis',flush=True)
 for ty,extra in(('Draw->Point',''),('Other','Other:adt{a,b,c,d:int;};'),('Fake->Rect','Fake:module{Rect:adt{a,b,c,d:int;};};')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(r:'+ty+');};xs:list of '+ty+';run(r:'+ty+'){xs=r::xs;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest','newrect'):
  row=next(r for r in records if r['name']==name);print('AWM',name,':',row['accepted'],row.get('pc'),row.get('reason'),flush=True)
 assert next(r for r in records if r['name']=='newrect')['accepted']
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text()
 src=src.replace('cons(ulong size, void *slot)\n{','cons(ulong size, void *slot)\n{\n if(rectconsfail && ++rectconscount==rectconsfail)failat=allocations+1;')
 at=src.index('List*\ncons(');src=src[:at]+'static int rectconsfail,rectconscount;\n'+src[at:]
 for sig in('OP(consm)','OP(headm)'):src+='\n'+extract(vm,sig)+'\n'
 src+='\n#include "draw-native.h"\n#define F_Rect_size F_Draw_Rect_size\nvoid\n'+extract((ROOT/'libinterp/geom.c').read_text(),'Rect_size(void *fp)')+'\n'
 return src+'\n#line 1 "native64-rect-list.c"\n'+(ROOT/'utils/tests/native64-rect-list.c').read_text().replace(' RECT_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='rect-list-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wrong Window rectangle field','native64-typed-code.h','if(s->returned==248 && (USRC(s->mode)&AIND)){p->s.i.f=s->s;p->s.i.s=8;', 'if(s->returned==248 && (USRC(s->mode)&AIND)){p->s.i.f=s->s;p->s.i.s=16;','exact Rect payload and field'),
   ('short raw head','native64-typed-code.h','if(s->returned>=248 && s->returned<=250){p->d.ind=s->d;', 'if(s->returned>=248 && s->returned<=250){if(s->returned==249)p->reg=4;p->d.ind=s->d;','exact MP output and adjacent bytes'),
   ('missing List destination root','native64-typed-module.h','if(!!(map[bit/8]&(128>>(bit%8)))!=(width==8))return 0;', 'if(!(k==1&&width==8) && !!(map[bit/8]&(128>>(bit%8)))!=(width==8))return 0;','malformed Rect list metadata refused'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS Rect list active control:',name,flush=True)
if __name__=='__main__':main()
