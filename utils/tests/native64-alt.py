#!/usr/bin/env python3
"""Execute checked borrowed blocking ALT guards with the real VM scheduler."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
TR=runpy.run_path(str(ROOT/'utils/tests/native64-channel-transfer.py'));G=TR['G'];CMD=TR['CMD'];E=TR['E']
NAMES=('run','reverse','send','stringrun','stringreverse','stringsend','mp')
def args(kind):return 'c:chan of '+kind+',v:'+kind+',other:chan of '+kind
SOURCE='implement Select;include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";wmclient:Wmclient;Window:import wmclient;Select:module{'+''.join(n+':fn('+args('string'if 3<=i<=5 else'int')+');'for i,n in enumerate(NAMES))+'window:fn(win:ref Window,v:string,other:chan of string);duplicate:fn('+args('int')+');clear:fn();};count,choice,wordout,value:int;saved:string;stored,otherstored:chan of int;'
for i,n in enumerate(NAMES[:6]):
 k='string'if i>=3 else'int';s='s'if i>=3 else'w';a=args(k)
 body='spawn pick'+s+'(c,other);spawn sender'+s+'(c,v);'if i%3==0 else'spawn sender'+s+'(c,v);spawn pick'+s+'(other,c);'if i%3==1 else'spawn choose'+s+'(c,other,v);spawn receiver'+s+'(c);'
 SOURCE+=n+'('+a+'){'+body+'}'
SOURCE+='mp('+args('int')+'){stored=c;otherstored=other;value=v;spawn pickmp();spawn sendermp();}window(win:ref Window,v:string,other:chan of string){c:=win.ctl;spawn pickwin(win,other);spawn senders(c,v);}duplicate('+args('int')+'){spawn pickw(c,c);spawn senderw(c,v);}clear(){saved=nil;stored=nil;otherstored=nil;}'
for k,s,out in(('int','w','wordout'),('string','s','saved')):
 SOURCE+='pick'+s+'(c:chan of '+k+',other:chan of '+k+'){alt{v:= <-c => '+out+'=v;choice=1;count++;v:= <-other => '+out+'=v;choice=2;count++;}}'
 SOURCE+='sender'+s+'(c:chan of '+k+',v:'+k+'){c <-=v;}receiver'+s+'(c:chan of '+k+'){'+out+'= <-c;}'
 SOURCE+='choose'+s+'(c:chan of '+k+',other:chan of '+k+',v:'+k+'){alt{c <-=v => choice=1;count++;other <-=v => choice=2;count++;}}'
SOURCE+='pickwin(win:ref Window,other:chan of string){alt{v:= <-win.ctl => saved=v;choice=1;count++;v:= <-other => saved=v;choice=2;count++;}}'
SOURCE+='pickmp(){alt{v:= <-stored => wordout=v;choice=1;count++;v:= <-otherstored => wordout=v;choice=2;count++;}}sendermp(){stored <-=value;}'
def generate(limbo,w):
 interface=w/'select.m';interface.write_text(SOURCE[:SOURCE.index('};')+2].replace('implement Select;','',1));header=subprocess.check_output([str(limbo),'-H','-M','Select','-I',str(ROOT/'module'),str(interface)],text=True);canonical={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Select_(\w+) (0x[0-9a-f]+)U',header,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Select',(21,0,0,'~0U'),export_signatures=canonical);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as o:o.write('static const uint alt_globals[]={'+','.join(str(gl[n])for n in('count','choice','wordout','saved'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in(189,190,191,192):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('ALT oracle missed',r['name'],pc,k))
 print('PASS blocking ALT source:',controls,'corruptions detected; classic operands/tables/maps and unchanged bytes',flush=True)
def boundaries(limbo,w):
 for source in(
  'implement A;A:module{run:fn(c:chan of int);};run(c:chan of int){alt{v:= <-c => ; * => ;}}',
  'implement A;A:module{run:fn(c:array of chan of int);};run(c:array of chan of int){alt{(i,v):= <-c => ;}}',
  'implement A;A:module{run:fn(c:chan of (int,int));};run(c:chan of (int,int)){alt{(v,w):= <-c => ;}}',
  'implement A;A:module{run:fn(c:chan of int);};get(c:chan of int):chan of int{return c;}run(c:chan of int){alt{v:= <-get(c) => ;}}'):
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in rows if r['name']=='run')['accepted'],rows
 for kind in('byte','big','real'):
  source='implement A;A:module{run:fn(c:chan of '+kind+');};run(c:chan of '+kind+'){alt{v:= <-c => ;}}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 for n in(32,33):
  source='implement A;A:module{run:fn(c:chan of int);};out:int;run(c:chan of int){alt{'+''.join('v:= <-c => out=v;'for _ in range(n))+'}}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert next(r for r in rows if r['name']=='run')['accepted']==(n==32),rows
 print('PASS ALT boundary: NBALT, arrays, other payload classes and computed channel closed; 32 guards accepted, 33 refused',flush=True)
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in rows if r['name']=='init');assert not init['accepted']and init['pc']==142 and init['reason']=='alt-field-channel',init;assert next(r for r in rows if r['name']=='command')['accepted']
 print('PASS real AWM frontier: Window.ctl PC140 passed; Wmcontext.ctl PC142 is the next closed field',flush=True)
def unit():
 src=TR['unit']().split('\n#line 1 "native64-channel-transfer.c"')[0];stub='void altdone(Alt*a,Prog*p,Channel*c,int r){USED(a);USED(p);USED(c);USED(r);require(0);}';assert src.count(stub)==1;src=src.replace(stub,'')
 alt=(ROOT/'libinterp/alt.c').read_text();src+='\nchar exAlt[]="alt send/recv on same chan";\n#define isend actual_isend\n#define irecv actual_irecv\n'+ '\n'.join(l for l in alt.splitlines()if not l.startswith('#include')and not l.startswith('#define OP'))+'\n#undef isend\n#undef irecv\n';extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();src+='\n'+extract(vm,'OP(alt)')+'\n'+extract(vm,'OP(nbalt)')+'\n'+extract(vm,'OP(igoto)')+'\n'
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-alt.c"\n'+(ROOT/'utils/tests/native64-alt.c').read_text().replace(' ALT_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent ALT oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='checked-alt-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicates in (
   ('window-field','native64-typed-code.h','p->s.i.s=64;','p->s.i.s=0;',('exact selected guard and count','ALT and peer complete without errors')),
   ('borrowed-table-map','native64-typed-alt.h','if(bit/8<f->mapbytes && (f->map[bit/8]&(128>>(bit%8))))return 0;}','if(0)return 0;}',('malformed ALT metadata refused',))):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and any(p in r.stderr for p in predicates)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS ALT active control:',name,flush=True)
if __name__=='__main__':main()
