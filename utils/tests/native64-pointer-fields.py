#!/usr/bin/env python3
"""Execute exact Draw.Pointer Word32 reads and zero tests."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
FIELDS=('buttons','xy.x','xy.y','msec')
NAMES=tuple('f'+str(i)for i in range(32))
DECL='Pointerfields:module{'+''.join(n+':fn(p:ref Draw->Pointer);'for n in NAMES)+'};'
SOURCE='implement Pointerfields;'+PREFIX+DECL+'stored:ref Draw->Pointer;out,after:int;'
for i,n in enumerate(NAMES):
 owner='stored'if (12<=i<24 or i>=28)else'p';field=FIELDS[i%12//3 if i<24 else(i-24)%4];action=i%3;head='stored=p;'if owner=='stored'else''
 body='n:='+owner+'.buttons;guard:=n;n='+owner+'.'+field+';out=n;after=guard;'if action==0 else'if('+owner+'.'+field+('==0'if action==1 else'!=0')+')out=1;else out=2;'
 if i>=24:body='out='+owner+'.'+field+';'
 SOURCE+=n+'(p:ref Draw->Pointer){'+head+body+'}'
def generate(limbo,w):
 m=w/'pointerfields.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Pointerfields','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Pointerfields_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Pointerfields',(32,0,0,'~0U'),export_signatures=signatures)
 read=next(r for r in rows if r['name']=='f0');loads=[c for c in read['code']if c[5]==228];assert len(loads)==2 and loads[0][4]==loads[1][4]and any(c[0]==E['OPCODES']['IMOVW']and c[2]==loads[0][4]and c[4]==loads[0][4]+4 for c in read['code']),read
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint field_globals[]={'+','.join(str(gl[n])for n in('stored','out','after'))+'};\n')
 (w/'pointer-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(228,229):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Pointer field oracle missed',row['name'],k))
 assert roles=={228,229},roles
 print('PASS Pointer field oracle:',controls,'corruptions detected; exact classic Word branches and full root width',flush=True)
 for owner,extra in(('Other','Other:adt{buttons:int;xy:Draw->Point;msec:int;};'),('Fake->Pointer','Fake:module{Pointer:adt{buttons:int;xy:Draw->Point;msec:int;};};')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(p:ref '+owner+');};out:int;run(p:ref '+owner+'){n:=p.buttons;out=n;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 for body in('p.buttons=3;','if(p.buttons<0)out=1;','if(p.buttons==3)out=1;'):
  _,closed=E['compile_source'](limbo,w,'implement Closed;'+PREFIX+'Closed:module{run:fn(p:ref Draw->Pointer);};out:int;run(p:ref Draw->Pointer){'+body+'}',interface='',loads=True);assert not closed[0]['accepted'],closed
 original=(ROOT/'module/draw.m').read_text()
 for old,new in(('buttons:\tint;','buttons:\tbig;'),('msec:\tint;','msec:\tbig;'),('buttons:\tint;','renamed:\tint;')):
  assert old in original;(w/'baddraw.m').write_text(original.replace(old,new,1))
  field='buttons'if old.startswith('msec:')else'msec'
  source='implement Closed;'+PREFIX.replace('include "draw.m";','include "baddraw.m";')+'Closed:module{run:fn(p:ref Draw->Pointer);};out:int;run(p:ref Draw->Pointer){n:=p.'+field+';out=n;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in records if r['name']=='init');assert init['accepted']or init['pc']>200,init;assert next(r for r in records if r['name']=='reshaped')['accepted'];print('AWM init frontier:',init.get('pc'),init.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-pointer-fields.c"\n'+(ROOT/'utils/tests/native64-pointer-fields.c').read_text().replace(' PARSE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='pointer-fields-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wide field read','native64-typed-code.h','if(s->returned==228 || s->returned==229){p->reg=0;p->s.i.f=s->s;', 'if(s->returned==228 || s->returned==229){p->op=s->returned==228?IMOVL:s->op;p->reg=0;p->s.i.f=s->s;','adjacent local Word32 preserved'),
   ('missing raw destination','native64-typed-module.h','if(step->returned==228){map=UDST(step->mode)==AFP?f->map:s->datamap;for(at=0;at<4;at++){bit=(step->d+at)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}}','if(step->returned==228){map=UDST(step->mode)==AFP?f->map:s->datamap;for(at=0;at<4;at++){bit=(step->d+at)/8;if(0)return 0;}}','malformed Pointer field metadata refused'),
   ('wrong selected field','native64-typed-code.h','p->s.i.s=s->m;if(s->returned==229)', 'p->s.i.s=s->m==12?0:s->m;if(s->returned==229)','exact selected Word32 field or zero branch'),
   ('wide zero comparison','native64-typed-code.h','if(s->returned==229)p->d.ins=out+s->d;else p->d.ind=s->d;', 'if(s->returned==229){p->op=s->op==IBEQW?IBEQL:IBNEL;p->d.ins=out+s->d;}else p->d.ind=s->d;','exact selected Word32 field or zero branch'),
   ('missing source root','native64-typed-module.h','map=(USRC(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;', 'map=(USRC(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(0)return 0;','malformed Pointer field metadata refused'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Pointer field active control:',name,flush=True)
if __name__=='__main__':main()
