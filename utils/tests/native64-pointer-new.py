#!/usr/bin/env python3
"""Execute checked Draw.Pointer allocation, scalar initialization and replacement."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
NAMES=('empty','filled','words','stores','globalstore','replace','parseglobal','parsefp','context')
DECL='Pointernew:module{'+''.join(n+':fn(p:ref Draw->Pointer);'for n in NAMES)+'};'
SOURCE='implement Pointernew;'+PREFIX+DECL+'stored:ref Draw->Pointer;ctx:ref Draw->Context;seed:int=123;text:string=" -123tail";'
SOURCE+='empty(p:ref Draw->Pointer){q:=ref Draw->Pointer;stored=q;}filled(p:ref Draw->Pointer){q:=ref Draw->Pointer(3,(5,7),11);stored=q;}'
SOURCE+='words(p:ref Draw->Pointer){a:=p.buttons;b:=p.xy.x;c:=p.xy.y;d:=p.msec;q:=ref Draw->Pointer(a,(b,c),d);stored=q;}'
SOURCE+='stores(p:ref Draw->Pointer){p.buttons=3;p.xy.x=5;p.xy.y=7;p.msec=11;stored=p;}globalstore(p:ref Draw->Pointer){stored=p;stored.buttons=seed;stored.xy.x=seed;stored.xy.y=seed;stored.msec=seed;}'
SOURCE+='replace(p:ref Draw->Pointer){q:=ref Draw->Pointer(1,(2,3),4);q=ref Draw->Pointer(3,(5,7),11);stored=q;}'
SOURCE+='parseglobal(p:ref Draw->Pointer){q:=ref Draw->Pointer(3,(5,7),11);q.xy.x=int text;stored=q;}parsefp(p:ref Draw->Pointer){stored=p;s:=text;stored.xy.y=int s;}'
SOURCE+='context(p:ref Draw->Pointer){c:=ref Draw->Context;ctx=c;}'
def generate(limbo,w):
 m=w/'pointernew.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Pointernew','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Pointernew_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Pointernew',(9,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint field_globals[]={'+str(gl['stored'])+','+str(gl['text'])+'};\n')
 (w/'pointer-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(242,243,244):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Pointer new oracle missed',row['name'],k))
 assert roles=={242,243,244},roles
 print('PASS Pointer allocation/store oracle:',controls,'corruptions detected; unchanged classic Dis',flush=True)
 for declaration,owner in(('Other:adt{buttons:int;xy:Draw->Point;msec:int;};','Other'),('Fake:module{Pointer:adt{buttons:int;xy:Draw->Point;msec:int;};};','Fake->Pointer')):
  source='implement Closed;'+PREFIX+declaration+'Closed:module{run:fn();};saved:ref '+owner+';run(){p:=ref '+owner+';saved=p;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest','setfocus','controlevent','reshaped'):
  row=next(r for r in records if r['name']==name);print('AWM',name,':',row['accepted'],row.get('pc'),row.get('reason'),flush=True)
 assert next(r for r in records if r['name']=='handlerequest')['pc']>436
def unit():
 CT=runpy.run_path(str(ROOT/'utils/tests/native64-context-new.py'))
 src=CT['unit']().split('\n#line 1 "native64-context-new.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\n'+extract((ROOT/'libinterp/heap.c').read_text(),'void\nfreestring(')+'\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(cvtcw)\n')+'\n'
 return src+'\n#line 1 "native64-pointer-new.c"\n'+(ROOT/'utils/tests/native64-pointer-new.c').read_text().replace(' PARSE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='pointer-new-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wrong store field','native64-typed-code.h','if(s->returned==243 || s->returned==244){p->reg=0;p->d.i.f=s->d;p->d.i.s=s->m;continue;}', 'if(s->returned==243 || s->returned==244){p->reg=0;p->d.i.f=s->d;p->d.i.s=s->m==12?8:s->m;continue;}','four exact Word32 fields'),
   ('wrong Pointer descriptor size','native64-typed-module.h','if(pointers){pointertype=ordinal;m->type[ordinal]=dtype(freeheap,16,nil,0);','if(pointers){pointertype=ordinal;m->type[ordinal]=dtype(freeheap,32,nil,0);','raw Pointer allocation descriptor'),
   ('missing Pointer destination root','native64-typed-module.h','map=(UDST(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->d/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;', 'map=(UDST(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->d/8;if(0)return 0;','malformed Pointer new metadata refused'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==(2 if name=='missing Pointer destination root'else 1);(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Pointer allocation/store active control:',name,flush=True)
  source=unit();before='memmove(R.d, &value, sizeof(value));';after='do{LONG wide=value;memmove(R.d,&wide,sizeof(wide));}while(0);';assert source.count(before)==1;(w/'unit.c').write_text(source.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'four exact Word32 fields'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS Pointer parser active control: wide store corrupts adjacent coordinate',flush=True)
if __name__=='__main__':main()
