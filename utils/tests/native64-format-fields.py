#!/usr/bin/env python3
"""Execute Window.tag variadic tails and consumed String temporary retirement."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-coordinate-format.py'));G,CMD=BASE['G'],BASE['CMD'];E=G['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
NAMES=('direct','nested','mp','forward','quoted')
DECL='Fieldformat:module{'+''.join(n+':fn(w:ref Wmsrv->Window);'for n in NAMES)+'init:fn();};'
SOURCE='implement Fieldformat;'+PREFIX+'Rect:import Draw;'+DECL+'sys:Sys;stored:ref Wmsrv->Window;out:string;'
SOURCE+='direct(w:ref Wmsrv->Window){s:=sys->sprint("%s|%s",w.tag,"tail");out=s;}nested(w:ref Wmsrv->Window){s:=sys->sprint("!reshape %q -1 %s",w.tag,r2s(w.r));out=s;}mp(w:ref Wmsrv->Window){stored=w;s:=sys->sprint("%s|%s",stored.tag,"tail");out=s;}forward(w:ref Wmsrv->Window){s:=relay(w);out=s;}quoted(w:ref Wmsrv->Window){s:=sys->sprint("%q",w.tag);out=s;}'
SOURCE+=re.search(r'r2s\(r: Rect\): string\s*\{.*?\n\}',(ROOT/'appl/wm/awm.b').read_text(),re.S)[0]
SOURCE+='relay(w:ref Wmsrv->Window):string{return sys->sprint("%s|%s",w.tag,"tail");}init(){sys=load Sys Sys->PATH;}'
def generate(limbo,w):
 m=w/'fieldformat.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Fieldformat','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Fieldformat_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Fieldformat',(8,1,5,'~0U'),export_signatures=signatures)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text()+'\nstatic const uint format_globals[]={'+','.join(str(gl[n])for n in('sys','stored','out'))+'};\nstatic const uint mixed_sys_offset='+str(gl['sys'])+';\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);(w/'window-native.h').write_text(re.sub(r'struct Sys_FD \{.*?\n\};','',h,flags=re.S))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(222,223):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('formatter field oracle missed',row['name'],pc,k))
 assert roles=={222,223},roles
 print('PASS formatter field oracle:',controls,'corruptions detected; canonical signatures, classic field/clear operands and unchanged bytes',flush=True)
 for extra,owner,expr in(('Other:adt{tag:string;};','Other','w.tag'),('Fake:module{Window:adt{tag:string;r:Draw->Rect;img:ref Draw->Image;};};','Fake->Window','w.tag'),('','Wmsrv->Window','w.img')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(sys:Sys,w:ref '+owner+'):string;};run(sys:Sys,w:ref '+owner+'):string{return sys->sprint("'+('%H'if expr=='w.img'else'%s')+'",'+expr+');}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 r=next(r for r in records if r['name']=='reshaped');assert r['accepted']or r['pc']>795,r;print('AWM reshaped frontier:',r.get('pc'),r.get('reason'),'; init:',[(x.get('pc'),x.get('reason'))for x in records if x['name']=='init'],flush=True)
def unit(w):
 src=BASE['unit'](w).split('\n#line 1 "native64-coordinate-format.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\n'+extract((ROOT/'libinterp/xec.c').read_text(),'OP(movm)\n')+'\n'
 return src+'\n#line 1 "native64-format-fields.c"\n'+(ROOT/'utils/tests/native64-format-fields.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='format-fields-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','fmtquote','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong tag field','native64-sys-module.h','else if(step->returned==222){out->s.i.f=step->s;out->s.i.s=0;', 'else if(step->returned==222){out->s.i.f=step->s;out->s.i.s=24;','retained Window tag argument'),
   ('temporary not cleared','native64-sys-module.h','else if(step->returned==223){Native64TypedStep copy=*step;copy.returned=0;', 'else if(step->returned==223){Native64TypedStep copy=*step;copy.returned=0;copy.mode=SRC(AFP)|DST(AFP);copy.s=copy.d;','temporary String retired before formatter'),
   ('missing owner slot root','native64-sys-module.h','map=(sm&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;', 'map=(sm&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(0)return 0;','malformed formatter field metadata refused')):
   text=(ROOT/'include'/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   (w/header).unlink();print('PASS formatter field active control:',name,flush=True)
if __name__=='__main__':main()
