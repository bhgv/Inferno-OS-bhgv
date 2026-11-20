#!/usr/bin/env python3
"""Execute unchanged Wmclient screenr/imager and the complete Window scalar layout."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-wmsrv-windows.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E'];EXTRACT=BASE['EXTRACT']
ORIGINAL=(ROOT/'appl/lib/wmclient.b').read_text();SELECTED=('Window.screenr','Window.imager')
FIELDS=('r.min.x','r.min.y','r.max.x','r.max.y','displayr.min.x','displayr.min.y','displayr.max.x','displayr.max.y','bd','focused','tbsize.x','tbsize.y','tbrect.min.x','tbrect.min.y','tbrect.max.x','tbrect.max.y','buttons','ptrfocus','saved.x','saved.y')
DECL=''.join('get'+str(i)+':fn(w:ref Window):int;set'+str(i)+':fn(w:ref Window,v:int);'for i in range(20))+'getmp:fn(w:ref Window):int;setmp:fn(w:ref Window,v:int);'+''.join('image'+n+':fn(i:ref Draw->Image):int;'for n in('depth','chans','repl'))
INTERFACE=(ROOT/'module/wmclient.m').read_text();INTERFACE=INTERFACE[:INTERFACE.rindex('};')]+DECL+INTERFACE[INTERFACE.rindex('};'):]
SOURCE=ORIGINAL
for m in reversed(list(re.finditer(r'(?m)^([A-Za-z_]\w*(?:\.\w+)?)\(',ORIGINAL))):
 if m[1]in SELECTED:continue
 body=BASE['limbo_body'](ORIGINAL[m.start():]);SOURCE=SOURCE[:m.start()]+body[:body.index('{')]+'{raise "unreached fixture body";}'+SOURCE[m.start()+len(body):]
SOURCE=SOURCE.replace('include "wmclient.m";',INTERFACE)+'stored:ref Window;'
SOURCE=re.sub(r'(?m)^init\(\)\s*\{raise "unreached fixture body";\}', 'init(){draw=load Draw Draw->PATH;}',SOURCE)
for i,f in enumerate(FIELDS):SOURCE+='get'+str(i)+'(w:ref Window):int{v:=w.'+f+';if(v==0)return 0;return v;}set'+str(i)+'(w:ref Window,v:int){w.'+f+'=v;}'
SOURCE+='getmp(w:ref Window):int{stored=w;v:=stored.bd;if(v==0)return 0;return v;}setmp(w:ref Window,v:int){stored=w;stored.bd=v;}'
for n in('depth','chans','repl'):SOURCE+='image'+n+'(i:ref Draw->Image):int{v:=i.'+('chans.desc'if n=='chans'else n)+';if(v==0)return 0;return v;}'
def generate(limbo,w):
 interface=w/'wmclient-geometry.m';interface.write_text('include "sys.m";include "draw.m";include "tk.m";'+INTERFACE)
 h=subprocess.check_output([str(limbo),'-H','-M','Wmclient','-I',str(ROOT/'module'),str(interface)],text=True);signatures={n.replace('_','.',1)if n.startswith('Window_')else n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Wmclient_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 (w/'window-native.h').write_text(h)
 dh=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(interface)],text=True);frames={}
 for n,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Draw_(\w+) (0x[0-9a-f]+)U',dh,re.M):
  size=re.search(r'^#define F_Draw_'+n+r'_size (\d+)',dh,re.M);mapping=re.search(r'^#define F_Draw_'+n+r'_map \{([^}]*)\}',dh,re.M)
  if size and mapping:frames[(n.replace('_','.',1),int(sig,16))]=(int(size[1]),[int(x,0)for x in mapping[1].split(',')if x])
 rows=G['generate'](limbo,w,SOURCE,'Wmclient',(59,1,0,'~0U'),import_frames=frames,export_signatures=signatures,source_label='unchanged Wmclient screenr/imager plus 42 Window and 3 Image helpers; fixture init loads Draw, other bodies raise')
 (w/'full.h').write_text((w/'full.h').read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 gl={x[0]:x[1]for x in rows[0]['globals']};(w/'full.h').write_text((w/'full.h').read_text()+'\nstatic const uint draw_offset='+str(gl['draw'])+';\n')
 types=('F_Draw_Rect_dx','F_Draw_Rect_dy','F_Draw_Rect_inset');(w/'geometry-layout.h').write_text(''.join('typedef struct '+n+' '+n+';\n'+re.search(r'struct '+n+r' \{.*?\n};',dh,re.S)[0]+'\n'for n in types))
 data,rows=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in rows:
  for pc,c in enumerate(row['code']):
   if c[5]not in(86,88,89):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
    except(AssertionError,KeyError,IndexError,ValueError,StopIteration):controls+=1
    else:raise AssertionError(('Window scalar oracle missed',row['name'],pc,k))
 print('PASS Wmclient Window scalar oracle:',controls,'independent mutations, unchanged classic bytes',flush=True)
 data,rows=E['compile_source'](limbo,w,ORIGINAL,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 for name in SELECTED:
  r=next(r for r in rows if r.get('export_name')==name);assert r['accepted'],r;CMD['verify_nilbranches'](data,r,imports,rows=rows)
 print('Wmclient frontier:',sum(r['accepted']for r in rows),'/',len(rows),[(r['name'],r.get('pc'),r.get('reason'))for r in rows if not r['accepted']],flush=True)
 for decl,owner in(('Other:adt{bd:int;};','Other'),('Fake:module{Window:adt{bd:int;};};','Fake->Window')):
  closed='implement Closed;include "sys.m";'+decl+'Closed:module{run:fn(w:ref '+owner+'):int;};run(w:ref '+owner+'):int{v:=w.bd;if(v==0)return 0;return v;}'
  _,r=E['compile_source'](limbo,w,closed,interface='',loads=True);assert len(r)==1 and not r[0]['accepted'],r
 print('PASS Window scalar boundary: foreign record and namespace refused',flush=True)

def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start);handlers=src[start:end]
 src+='\n#include "window-native.h"\n#include "geometry-layout.h"\n'
 if 'OP(movm)'not in src:src+='\n'+EXTRACT((ROOT/'libinterp/xec.c').read_text(),'OP(movm)')+'\n'
 for n in('Rect_dx','Rect_dy','Rect_inset'):src+='\n#define F_'+n+' F_Draw_'+n+'\n'+EXTRACT((ROOT/'libinterp/geom.c').read_text(),'void\n'+n+'(')+'\n'
 return src+'\n#line 1 "native64-wmclient-geometry.c"\n'+(ROOT/'utils/tests/native64-wmclient-geometry.c').read_text().replace(' GEOMETRY_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='wmclient-geometry-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(w/'unit')];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong Window scalar offset','native64-typed-code.h','if(s->returned==89||s->returned==133){p->s.i.f=s->s&65535;p->s.i.s=(unsigned long)s->s>>16;','if(s->returned==89||s->returned==133){p->s.i.f=s->s&65535;p->s.i.s=s->returned==89?56:(unsigned long)s->s>>16;','exact signed scalar read'),
   ('missing caller geometry proof','native64-typed-multi.h','!c->external || c->callerresultcontract!=c->contract','!c->external','unproved caller geometry return refused')):
   parents=('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h')
   for n in parents:(w/n).write_text((ROOT/'include'/n).read_text())
   text=(ROOT/'include'/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for n in parents:(w/n).unlink()
   print('PASS Wmclient geometry active control:',name,flush=True)

if __name__=='__main__':main()
