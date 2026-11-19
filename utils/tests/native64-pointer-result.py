#!/usr/bin/env python3
"""Execute real Sys.millisec into rooted Pointer.msec and deliver via a channel."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
NAMES=('direct','local','twice','made','deliver','madedeliver')
DECL='Pointerresult:module{'+''.join(n+':fn(p:ref Draw->Pointer);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Pointerresult;'+PREFIX+DECL+'sys:Sys;stored:ref Draw->Pointer;'
SOURCE+='direct(p:ref Draw->Pointer){p.msec=sys->millisec();}local(p:ref Draw->Pointer){a:=p;a.msec=sys->millisec();}twice(p:ref Draw->Pointer){p.msec=sys->millisec();p.msec=sys->millisec();}'
SOURCE+='made(p:ref Draw->Pointer){q:=ref Draw->Pointer(3,(5,7),11);q.msec=sys->millisec();stored=q;}'
SOURCE+='deliver(p:ref Draw->Pointer){ch:=chan[1]of ref Draw->Pointer;p.msec=sys->millisec();ch<-=p;stored=<-ch;}'
SOURCE+='madedeliver(p:ref Draw->Pointer){q:=ref Draw->Pointer(3,(5,7),11);ch:=chan[1]of ref Draw->Pointer;q.msec=sys->millisec();ch<-=q;stored=<-ch;}'
SOURCE+='open(path:string){sys=load Sys path;}'
def generate(limbo,w):
 m=w/'pointerresult.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Pointerresult','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Pointerresult_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 sysheader=subprocess.check_output([str(limbo),'-H','-M','Sys','-I',str(ROOT/'module'),str(m)],text=True);(w/'sys-native.h').write_text(sysheader)
 drawheader=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'draw-native.h').write_text('\n'.join('typedef struct Draw_'+n+' Draw_'+n+';\n'+re.search(r'struct Draw_'+n+r' \{.*?\n\};',drawheader,re.S)[0]+'\n'+next(l for l in drawheader.splitlines()if l.startswith('#define Draw_'+n+'_map '))for n in('Point','Pointer')))
 size=int(re.search(r'^#define F_Sys_millisec_size (\d+)',sysheader,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Sys_millisec_map \{([^}]*)\}',sysheader,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sys_millisec (0x[0-9a-f]+)U',sysheader,re.M)[1],16)
 rows=G['generate'](limbo,w,SOURCE,'Pointerresult',(7,1,0,'~0U'),import_frames={('millisec',sig):(size,mapping)},export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint result_globals[]={'+','.join(str(gl[n])for n in('sys','stored'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for e in row.get('externals',[]):
   pc=e['end']-1;assert row['code'][pc][5]==245 and e['contract']==4
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Pointer result oracle missed',row['name'],k))
 print('PASS Pointer result oracle:',controls,'corruptions detected; canonical Sys/Pointer geometry and unchanged classic bytes',flush=True)
 for extra,owner,destination in(('', 'Draw->Pointer','saved'),('Other:adt{buttons:int;xy:Draw->Point;msec:int;};','Other','p'),('Fake:module{Pointer:adt{buttons:int;xy:Draw->Point;msec:int;};};','Fake->Pointer','p')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(p:ref '+owner+');};sys:Sys;saved:ref '+owner+';run(p:ref '+owner+'){saved=p;'+destination+'.msec=sys->millisec();}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(closed)==1 and not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest','setfocus','controlevent','reshaped'):
  r=next(r for r in records if r['name']==name);print('AWM',name,':',r['accepted'],r.get('pc'),r.get('reason'),flush=True)
 assert next(r for r in records if r['name']=='handlerequest')['pc']>441
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();port=(ROOT/'emu/port/inferno.c').read_text()
 src+='\n#include "sys-native.h"\nstatic WORD clockvalue;static int clockcalls,clockfault;long osmillisec(void){clockcalls++;if(clockfault&&clockcalls==clockfault)error("fail:clock");return clockvalue;}\n'
 for sig in('static void\nsysstatusframe(','void\nSys_millisec('):src+='\n'+extract(port,sig)+'\n'
 for sig in('static void\nnewrecord(','OP(new)','OP(newz)'):src+='\n'+extract(vm,sig)+'\n'
 src+='\n#define movp actual_movp\n'+extract(vm,'OP(newcp)')+'\n#undef movp\n'
 return src+'\n#line 1 "native64-pointer-result.c"\n'+(ROOT/'utils/tests/native64-pointer-result.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='pointer-result-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wrong field displacement','native64-typed-multi.h','else if(s->returned==245){p->s.i.f=s->s;p->s.i.s=12;', 'else if(s->returned==245){p->s.i.f=s->s;p->s.i.s=8;','exact completion/error'),
   ('missing raw heap geometry','native64-local-bind.h',' || (ownertype->np && ownertype->map[0])','', 'exact completion/error'),
   ('missing owner GC root','native64-typed-module.h','bit=step->s/8;if(!(f->map[bit/8]&(0x80>>(bit%8))))return 0;', 'bit=step->s/8;if(0)return 0;','malformed Pointer result metadata refused'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS Pointer result active control:',name,flush=True)
  source=unit();before='memmove(f->ret, &result, sizeof(result));';after='do{LONG wide=result;memmove(f->ret,&wide,sizeof(wide));}while(0);';assert source.count(before)==1;(w/'unit.c').write_text(source.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'timestamp width preserves heap guard'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS real millisec active control: wide result store detected by heap guard',flush=True)
if __name__=='__main__':main()
