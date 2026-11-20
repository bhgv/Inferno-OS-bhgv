#!/usr/bin/env python3
"""Execute checked Sys.aprint with mixed tails, Client fields and byte-array results."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD']
def generate(limbo,w):
 source='implement Coordinates;include "sys.m";sys:Sys;include "draw.m";include "wmsrv.m";Rect:import Draw;stored:ref Wmsrv->Client;Coordinates:module{init:fn();wmrect:fn(r:Rect):array of byte;awmrect:fn(r:Rect):array of byte;mix:fn(n:int,b:big,r:real,s:string):array of byte;fresh:fn(n:int,b:big,r:real,s:string):array of byte;};init(){sys=load Sys Sys->PATH;}wmrect(r:Rect):array of byte{return sys->aprint("%d %d %d %d",r.min.x,r.min.y,r.max.x,r.max.y);}awmrect(r:Rect):array of byte{v:=sys->aprint("%d %d %d %d",r.min.x,r.min.y,r.max.x,r.max.y);return v;}mix(n:int,b:big,r:real,s:string):array of byte{c:=ref Wmsrv->Client;c.token=n;return sys->aprint("%d %bd %.2f %s",c.token,b,r,s);}fresh(n:int,b:big,r:real,s:string):array of byte{stored=ref Wmsrv->Client;stored.token=n;rc:=chan[n&1]of(array of byte,string);ok:=0;alt{rc<-=(sys->aprint("%d",stored.token),nil)=>ok=1;* =>;}value:array of byte;if(ok){(v,e):=<-rc;value=v;}return value;}'
 rows=G['generate'](limbo,w,source,'Coordinates',(5,1,4,'~0U'),source_label='checked ALT aprint source fixture')
 with(w/'full.h').open('a')as out:out.write('static const uint mixed_sys_offset='+str(next(g[1]for g in rows[0]['globals']if g[0]=='sys'))+';\n')
 data,rows=G['E']['compile_source'](limbo,w,source,interface='',loads=True);imports=G['E']['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 import copy
 for row in rows:
  for pc,c in enumerate(row['code']):
   if c[5]!=95:continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
    except(AssertionError,KeyError,IndexError,ValueError,StopIteration):controls+=1
    else:raise AssertionError(('Client word oracle missed',pc,k))
 assert controls==12,controls
 print('PASS aprint Client word oracle:',controls,'mutations, FP/MP owners, unchanged classic bytes',flush=True)
 controls=0
 for row in rows:
  for var in row.get('variadics',[]):
   if var['end']+2>=len(row['code'])or row['code'][var['end']+2][5]!=195:continue
   for pc in range(var['first'],var['end']+3):
    for k in range(6):
     bad=copy.deepcopy(row);bad['code'][pc][k]+=1
     try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
     except(AssertionError,KeyError,IndexError,ValueError,StopIteration):controls+=1
     else:raise AssertionError(('ALT producer oracle missed',pc,k))
 assert controls==42,controls
 print('PASS ALT aprint source preparation:',controls,'independent mutations',flush=True)

 for decl,typ,expr in [('Other:adt{token:int;};','ref Other','c.token'),('','array of int','c'),('','chan of int','c')]:
  closed='implement Closed;include "sys.m";'+decl+'Closed:module{run:fn(sys:Sys,c:'+typ+'):array of byte;};run(sys:Sys,c:'+typ+'):array of byte{return sys->aprint("'+('%d'if expr=='c.token'else'%H')+'",'+expr+');}'
  _,records=G['E']['compile_source'](limbo,w,closed,interface='',loads=True);assert len(records)==1 and not records[0]['accepted'],records
 print('PASS aprint compiler refusals: foreign token owner and opaque array/channel tails',flush=True)
 _,records=G['E']['compile_source'](limbo,w,(ROOT/'appl/lib/wmsrv.b').read_text(),interface='',loads=True)
 wm=next(r for r in records if r['name']=='wm');assert wm['accepted']or wm['pc']>164,wm
 assert next(r for r in records if r['name']=='childminder')['accepted']
 print('Wmsrv frontier:',sum(r['accepted']for r in records),'/',len(records),[(r['name'],r.get('pc'),r.get('reason'))for r in records if not r['accepted']],flush=True)



def unit(w):
 apr=runpy.run_path(str(ROOT/'utils/tests/native64-aprint.py'));alt=runpy.run_path(str(ROOT/'utils/tests/native64-alt-replies.py'));extract=CMD['NS']['S']['FILES']['FILES']['function']
 original=apr['unit'](w);start=original.index('\n#define LIMBO_NATIVE64_PREFIXES');end=original.index('\n#line 1 "native64-aprint.c"');tail=original[start:end]
 prefix=alt['unit']().split('\n#line 1 "native64-alt-replies.c"')[0]
 for sig in('Heap*heap(','void\nfreearray(','static void\nnewrecord(','OP(new)','OP(newz)','Array*\nmem2array(','String*\nstringdup(','void\ncvtup(','String*\naddstring(','OP(addc)'):
  if sig in tail and sig in prefix:tail=tail.replace(extract(tail,sig),'')
 if 'Heap*heap('in tail:tail=tail.replace(extract(tail,'Heap*heap('),'')
 prefix=prefix.replace(extract(prefix,'int\nsnprint('),'',1)
 src=prefix+tail
 # The ALT runtime owns all channel/selection handlers and mapped copies.
 controls=original[original.index(' optab[IMOVW]=countmovw;'):original.index(' require(prepare64calls(')]
 return src+'\n#line 1 "native64-aprint-alt.c"\n'+(ROOT/'utils/tests/native64-aprint-alt.c').read_text().replace(' MIX_HANDLERS',controls)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='aprint-alt-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w)
  (w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  extra=[str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]
  binary=w/'unit';command=cc+[str(w/'unit.c')]+extra+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,before,after in (
   ('sign-extension','out->op=step->op;out->add=step->mode;','out->op=step->op==ICVTWL?IMOVW:step->op;out->add=step->mode;'),
   ('wrong Client field','else if(step->returned==95){out->s.i.f=step->s;out->s.i.s=step->m;','else if(step->returned==95){out->s.i.f=step->s;out->s.i.s=72;')):
   text=(ROOT/'include/native64-sys-module.h').read_text();assert text.count(before)==1;header=w/'native64-sys-module.h';header.write_text(text.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and ('source execution completed'if name=='sign-extension'else'exact byte formatting')in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   header.unlink();print('PASS aprint active control:',name,flush=True)
if __name__=='__main__':main()
