#!/usr/bin/env python3
"""Execute exact Point/Rect arguments through real local Limbo calls and AWM r2s."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
NAMES=('direct','mp','prefix','prefixmp','suffix','suffixmp')
DECL='Clientid:module{'+''.join(n+':fn(c:ref Wmsrv->Client);'for n in NAMES)+'};'
SOURCE='implement Clientid;'+PREFIX+DECL+'stored:ref Wmsrv->Client;out:string;'
SOURCE+='direct(c:ref Wmsrv->Client){s:=string c.id;out=s;}mp(c:ref Wmsrv->Client){stored=c;s:=string stored.id;out=s;}'
for i,n in enumerate(NAMES[2:]):
 owner='stored'if i%2 else'c';head='stored=c;'if i%2 else'';expr='"newclient "+string '+owner+'.id'if i<2 else'string '+owner+'.id+" event"'
 SOURCE+=n+'(c:ref Wmsrv->Client){'+head+'consume('+expr+');}'
SOURCE+='consume(s:string){out=s;}'
def generate(limbo,w):
 m=w/'clientid.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Clientid','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Clientid_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Clientid',(7,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint id_globals[]={'+','.join(str(gl[n])for n in('stored','out'))+'};\n')
 (w/'client-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(237,238):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Client id oracle missed',row['name'],pc,k))
 assert roles=={237,238},roles
 print('PASS Client id oracle:',controls,'corruptions detected; exact field, local argument/clear maps and unchanged bytes',flush=True)
 for body in('n:=string c.flags;out=n;','spawn consume("x"+string c.id);','multi("x"+string c.id,0);'):
  source='implement Closed;'+PREFIX+'Closed:module{run:fn(c:ref Wmsrv->Client);};out:string;run(c:ref Wmsrv->Client){'+body+'}consume(s:string){out=s;}multi(s:string,n:int){out=s;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 for ty,extra in(('Other','Other:adt{id:int;};'),('Fake->Client','Fake:module{Client:adt{id:int;};};')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(c:ref '+ty+');};out:string;run(c:ref '+ty+'){s:=string c.id;out=s;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 original=(ROOT/'module/wmsrv.m').read_text();(w/'badwmsrv.m').write_text(original.replace('flags:\tint;','flags:\tbig;',1))
 _,closed=E['compile_source'](limbo,w,SOURCE.replace('include "wmsrv.m";','include "badwmsrv.m";'),interface='',loads=True);assert not any(r['accepted']for r in closed if r['name']in NAMES),closed
 source='implement Closed;'+PREFIX+'Closed:module{run:fn(c:ref Wmsrv->Client);};out:string;run(c:ref Wmsrv->Client){s:=echo("x"+string c.id);out=s;}echo(s:string):string{return s;}'
 _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in records if r['name']=='init');assert init['accepted']or init['pc']>243,init;assert next(r for r in records if r['name']=='setfocus')['accepted'];print('AWM init:',init.get('pc'),init.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 handlers=src[start:end]
 src+='\nvoid rerrstr(char*p,uint n){if(n)snprintf(p,n,\"fixture\");}\n'
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src=src.replace(extract(src,'int\nsnprint('),'',1);src+='\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(cvtwc)')+'\n'
 return src+'\n#line 1 "native64-client-id.c"\n'+(ROOT/'utils/tests/native64-client-id.c').read_text().replace(' WINDOW_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='client-id-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,before,after,predicate in (
   ('wrong id offset','if(s->returned==237){p->reg=0;p->s.i.f=s->s;p->s.i.s=72;', 'if(s->returned==237){p->reg=0;p->s.i.f=s->s;p->s.i.s=32;','exact Client id decimal String'),
   ('caller temporary not cleared','else if(s->returned==238){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}', 'else if(s->returned==238){checkstep=*s;checkstep.returned=0;checkstep.d=64;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','temporary retired before local CALL')):
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   header=w/('native64-typed-code.h'if name=='wrong id offset'else'native64-typed-multi.h');text=header.read_text();assert text.count(before)==1;header.write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Client id active control:',name,flush=True)
if __name__=='__main__':main()
