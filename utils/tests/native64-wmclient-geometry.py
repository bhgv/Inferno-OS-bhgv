#!/usr/bin/env python3
"""Execute unchanged Wmclient geometry/input and complete Window scalar/Point/Rect fields."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-wmsrv-windows.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E'];EXTRACT=BASE['EXTRACT']
ORIGINAL=(ROOT/'appl/lib/wmclient.b').read_text();SELECTED=('Window.screenr','Window.imager','Window.pointer','sizetb')
FIELDS=('r.min.x','r.min.y','r.max.x','r.max.y','displayr.min.x','displayr.min.y','displayr.max.x','displayr.max.y','bd','focused','tbsize.x','tbsize.y','tbrect.min.x','tbrect.min.y','tbrect.max.x','tbrect.max.y','buttons','ptrfocus','saved.x','saved.y')
GEOMETRIES=('r','displayr','tbrect','r.min','r.max','displayr.min','displayr.max','tbsize','tbrect.min','tbrect.max','saved')
DECL=''.join('get'+str(i)+':fn(w:ref Window):int;set'+str(i)+':fn(w:ref Window,v:int);'for i in range(20))+'getmp:fn(w:ref Window):int;setmp:fn(w:ref Window,v:int);'+''.join('image'+n+':fn(i:ref Draw->Image):int;'for n in('depth','chans','repl'))
DECL+=''.join('gcap'+str(i)+':fn(w:ref Window,v:int);gpub'+str(i)+':fn(w:ref Window,v:int);'for i in range(11))+'testsize:fn(w:ref Window,v:int);resr:fn(w:ref Window,r:Draw->Rect):Draw->Rect;resp:fn(w:ref Window,p:Draw->Point):Draw->Point;locresr:fn(w:ref Window,r:Draw->Rect):Draw->Rect;'
INTERFACE=(ROOT/'module/wmclient.m').read_text();INTERFACE=INTERFACE[:INTERFACE.rindex('};')]+DECL+INTERFACE[INTERFACE.rindex('};'):]
DECL+='qrepeat:fn(w:ref Window,v:int);qparse:fn(w:ref Window,v:int);rparse:fn(w:ref Window,v:int);ctlparse:fn(w:ref Window,v:int);'
INTERFACE=(ROOT/'module/wmclient.m').read_text();INTERFACE=INTERFACE[:INTERFACE.rindex('};')]+DECL+INTERFACE[INTERFACE.rindex('};'):]
SOURCE=ORIGINAL
for m in reversed(list(re.finditer(r'(?m)^([A-Za-z_]\w*(?:\.\w+)?)\(',ORIGINAL))):
 if m[1]in SELECTED:continue
 body=BASE['limbo_body'](ORIGINAL[m.start():]);SOURCE=SOURCE[:m.start()]+body[:body.index('{')]+'{raise "unreached fixture body";}'+SOURCE[m.start()+len(body):]
SOURCE=SOURCE.replace('include "wmclient.m";',INTERFACE)+'stored:ref Window;'
SOURCE=re.sub(r'(?m)^init\(\)\s*\{raise "unreached fixture body";\}', 'init(){draw=load Draw Draw->PATH;tk=load Tk Tk->PATH;wmlib=load Wmlib Wmlib->PATH;}',SOURCE)
for i,f in enumerate(FIELDS):SOURCE+='get'+str(i)+'(w:ref Window):int{v:=w.'+f+';if(v==0)return 0;return v;}set'+str(i)+'(w:ref Window,v:int){w.'+f+'=v;}'
SOURCE+='getmp(w:ref Window):int{stored=w;v:=stored.bd;if(v==0)return 0;return v;}setmp(w:ref Window,v:int){stored=w;stored.bd=v;}'
for n in('depth','chans','repl'):SOURCE+='image'+n+'(i:ref Draw->Image):int{v:=i.'+('chans.desc'if n=='chans'else n)+';if(v==0)return 0;return v;}'
for i,g in enumerate(GEOMETRIES):
 SOURCE+='gvalue'+str(i)+':'+('Rect'if i<3 else'Point')+';gcap'+str(i)+'(w:ref Window,v:int){gvalue'+str(i)+'=w.'+g+';}gpub'+str(i)+'(w:ref Window,v:int){w.'+g+'=gvalue'+str(i)+';}'
SOURCE+='testsize(w:ref Window,v:int){sizetb(w);}resr(w:ref Window,r:Rect):Rect{w.r=r.inset(1);return r;}resp(w:ref Window,p:Point):Point{w.tbsize=p.add((3,4));return p;}locresr(w:ref Window,r:Rect):Rect{w.r=localr(r);return r;}localr(r:Rect):Rect{return r.inset(2);}'
SOURCE+='parsed:string;parsedimage:ref Image;parsederror:string;parsedindex,tupleguard:int;qparse(w:ref Window,v:int){guard:=v;(s,n):=qword("quoted word",v);parsed=s;parsedindex=n;tupleguard=guard;}rparse(w:ref Window,v:int){(r,n):=s2r("1 2 3 4",v);w.displayr=r;parsedindex=n;}ctlparse(w:ref Window,v:int){(s,i,e):=wmlib->wmctl(nil,"request");parsed=s;parsedimage=i;parsederror=e;parsedindex=v;}'
SOURCE+='qrepeat(w:ref Window,v:int){(s,n):=qword("quoted word",v);parsed=s;(s,n)=qword("quoted word",n);parsed=s;parsedindex=n;tupleguard=v;}'
def generate(limbo,w):
 interface=w/'wmclient-geometry.m';interface.write_text('include "sys.m";include "draw.m";include "tk.m";include "wmlib.m";'+INTERFACE)
 h=subprocess.check_output([str(limbo),'-H','-M','Wmclient','-I',str(ROOT/'module'),str(interface)],text=True);signatures={n.replace('_','.',1)if n.startswith('Window_')else n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Wmclient_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 (w/'window-native.h').write_text(h)
 dh=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(interface)],text=True);frames={}
 th=subprocess.check_output([str(limbo),'-H','-M','Tk','-I',str(ROOT/'module'),str(interface)],text=True)
 wh=subprocess.check_output([str(limbo),'-H','-M','Wmlib','-I',str(ROOT/'module'),str(interface)],text=True)
 for namespace,text in(('Draw',dh),('Tk',th),('Wmlib',wh)):
  for n,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+namespace+r'_(\w+) (0x[0-9a-f]+)U',text,re.M):
   size=re.search(r'^#define F_'+namespace+'_'+n+r'_size (\d+)',text,re.M);mapping=re.search(r'^#define F_'+namespace+'_'+n+r'_map \{([^}]*)\}',text,re.M)
   if size and mapping:frames[(n.replace('_','.',1),int(sig,16))]=(int(size[1]),[int(x,0)for x in mapping[1].split(',')if x])
 rows=G['generate'](limbo,w,SOURCE,'Wmclient',(91,3,0,'~0U'),import_frames=frames,export_signatures=signatures,source_label='four unchanged Wmclient geometry/input bodies with scalar/geometry helpers; fixture init loads Draw/Tk/Wmlib')
 (w/'full.h').write_text((w/'full.h').read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 gl={x[0]:x[1]for x in rows[0]['globals']};(w/'full.h').write_text((w/'full.h').read_text()+'\nstatic const uint draw_offset='+str(gl['draw'])+';\nstatic const uint tk_offset='+str(gl['tk'])+';\nstatic const uint wmlib_offset='+str(gl['wmlib'])+';\nstatic const uint tuple_offsets[]={'+','.join(str(gl[n])for n in('parsed','parsedimage','parsederror','parsedindex','tupleguard'))+'};\nstatic const uint geometry_offsets[]={'+','.join(str(gl['gvalue'+str(i)])for i in range(11))+'};\n')
 types=('F_Draw_Rect_dx','F_Draw_Rect_dy','F_Draw_Rect_inset','F_Draw_Rect_size','F_Draw_Point_add','F_Draw_Point_in');(w/'geometry-layout.h').write_text(''.join('typedef struct '+n+' '+n+';\n'+re.search(r'struct '+n+r' \{.*?\n};',dh,re.S)[0]+'\n'for n in types))
 data,rows=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in rows:
  for pc,c in enumerate(row['code']):
   if c[5]not in(85,81,82,83,84,86,88,89):continue
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
 for shape,body in (('(string,big)','(a,b):=svc->get();'),('(int,string,int)','(a,b,c):=svc->get();'),('(string,ref Other,string)','(a,b,c):=svc->get();')):
  source='implement Closed;Other:adt{x:int;};Service:module{get:fn():'+shape+';};Closed:module{run:fn();};svc:Service;run(){'+body+'}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 source='implement Closed;Service:module{get:fn():(string,int);};Closed:module{run:fn():(string,int);};svc:Service;run():(string,int){return svc->get();}'
 _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 print('PASS Wmlib tuple boundary: wrong numeric/root shapes and forwarded tuple results refused',flush=True)


def unit():
 src=CMD['unit']().replace('#define RuntimeBlocks 256','#define RuntimeBlocks 1024').replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start);handlers=src[start:end]
 src+='\n#include "window-native.h"\n#include "geometry-layout.h"\n'
 if 'OP(movm)'not in src:src+='\n'+EXTRACT((ROOT/'libinterp/xec.c').read_text(),'OP(movm)')+'\n'
 for n in('Rect_dx','Rect_dy','Rect_inset','Rect_size','Point_add','Point_in'):src+='\n#define F_'+n+' F_Draw_'+n+'\n'+EXTRACT((ROOT/'libinterp/geom.c').read_text(),'void\n'+n+'(')+'\n'
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
   ('wrong Window geometry result field','native64-typed-multi.h','p->s.i.s=s->m;p->d.i.f=40;p->d.i.s=32;','p->s.i.s=s->m==8?32:s->m;p->d.i.f=40;p->d.i.s=32;','external/local Rect result into Window with independent caller result'),
   ('wrong Wmlib tuple result offset','native64-typed-multi.h','else if(s->returned==85){p->reg=c->contract;p->s.ind=s->s;','else if(s->returned==85){p->reg=c->contract;p->s.ind=s->s+8;','exact method completion/error'),
   ('missing caller geometry proof','native64-typed-multi.h','!c->external || c->callerresultcontract!=c->contract','!c->external','unproved caller geometry return refused')):
   parents=('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h')
   for n in parents:(w/n).write_text((ROOT/'include'/n).read_text())
   text=(ROOT/'include'/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for n in parents:(w/n).unlink()
   print('PASS Wmclient geometry active control:',name,flush=True)

if __name__=='__main__':main()
