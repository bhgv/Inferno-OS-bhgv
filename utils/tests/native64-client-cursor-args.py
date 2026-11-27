#!/usr/bin/env python3
"""Check exact Client.cursor String arguments into imported rooted frames."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
SERVICE='Sink:module{take:fn(s:string);echo:fn(w:ref Wmsrv->Window,s:string):string;};'
NAMES=('direct','mp','method','methodmp')
DECL='Cursorargs:module{'+''.join(n+':fn(c:ref Wmsrv->Client);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Cursorargs;'+PREFIX+SERVICE+DECL+'sink:Sink;stored:ref Wmsrv->Client;out:string;'
SOURCE+='direct(c:ref Wmsrv->Client){sink->take(c.cursor);}mp(c:ref Wmsrv->Client){stored=c;sink->take(stored.cursor);}method(c:ref Wmsrv->Client){s:=sink->echo(nil,c.cursor);out=s;}methodmp(c:ref Wmsrv->Client){stored=c;s:=sink->echo(nil,stored.cursor);out=s;}open(path:string){sink=load Sink path;}'
def generate(limbo,w):
 m=w/'cursorargs.m';m.write_text(PREFIX+SERVICE+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Cursorargs','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Cursorargs_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Sink','-I',str(ROOT/'module'),str(m)],text=True);frames={}
 for n in('take','echo'):
  size=int(re.search(r'^#define F_Sink_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Sink_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n,sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Cursorargs',(5,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint sink_offset='+str(gl['sink'])+';\nstatic const uint out_offset='+str(gl['out'])+';\n')
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True);(w/'client-native.h').write_text(h)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]!=233:continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Pointer argument oracle missed',row['name'],pc,k))
 assert roles=={233},roles
 print('PASS Client cursor argument oracle:',controls,'corruptions detected; full owner and imported String root, canonical signatures and unchanged bytes',flush=True)
 for ty,extra in(('Other','Other:adt{cursor:string;};'),('Fake->Client','Fake:module{Client:adt{cursor:string;};};')):
  source='implement Closed;'+PREFIX+SERVICE+extra+'Closed:module{run:fn(c:ref '+ty+');};sink:Sink;run(c:ref '+ty+'){sink->take(c.cursor);}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 source='implement Closed;'+PREFIX+'Closed:module{run:fn(c:ref Wmsrv->Client);};out:string;run(c:ref Wmsrv->Client){local(c.cursor);}local(s:string){out=s;}'
 _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in closed if r['name']=='run')['accepted'],closed
 original=(ROOT/'module/wmsrv.m').read_text();(w/'badwmsrv.m').write_text(original.replace('flags:\tint;','flags:\tbig;',1))
 _,closed=E['compile_source'](limbo,w,SOURCE.replace('include "wmsrv.m";','include "badwmsrv.m";'),interface='',loads=True);assert not any(r['accepted']for r in closed if r['name']in NAMES),closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in records if r['name']=='init');focus=next(r for r in records if r['name']=='setfocus');assert focus['accepted']or focus['pc']>1203,focus;assert next(r for r in records if r['name']=='reshaped')['accepted'];print('AWM init:',init.get('pc'),init.get('reason'),'; setfocus:',focus.get('pc'),focus.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-client-cursor-args.c"\n'+(ROOT/'utils/tests/native64-client-cursor-args.c').read_text().replace(' POINTER_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='cursor-args-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong cursor offset','native64-typed-multi.h','p->s.i.f=s->s;p->s.i.s=s->m;p->d.i.f=40;p->d.i.s=s->d;', 'p->s.i.f=s->s;p->s.i.s=s->returned==233?0:s->m;p->d.i.f=40;p->d.i.s=s->d;','exact borrowed cursor String'),
   ('missing owner map','native64-typed-module.h','if(!found)return 0;mode=USRC(step->mode)&~AIND;map=mode==AFP?f->map:s->datamap;bit=step->s/8;if((map[bit/8]&(128 >> (bit%8)))==0)return 0;continue;', 'if(!found)return 0;continue;','malformed cursor argument metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS Client cursor argument active control:',name,flush=True)
if __name__=='__main__':main()
