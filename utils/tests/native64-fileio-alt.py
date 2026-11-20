#!/usr/bin/env python3
"""Execute checked Sys.FileIO fields and mixed read/write ALT payloads."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'))
G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";'
ARGS='f:ref Sys->FileIO,a:array of byte,r:Sys->Rread,w:Sys->Rwrite'
NAMES=('write','read','writemp','readmp','writereverse','readreverse','writedup','readdup')
DECL='Filealt:module{'+''.join(n+':fn('+ARGS+');'for n in NAMES)+'clear:fn();inspect:fn(f:ref Sys->FileIO);};'
SOURCE='implement Filealt;'+PREFIX+DECL+'stored:ref Sys->FileIO;data,aout:array of byte;rr,rout:Sys->Rread;wr,wout:Sys->Rwrite;off,count,fid,offsetout,countout,fidout,received,sent,choice:int;readtuple:(int,int,int,Sys->Rread);writetuple:(int,array of byte,int,Sys->Rwrite);'
for i,name in enumerate(NAMES):
 write=i%2==0;kind='write'if write else'read';mp=i in(2,3);reverse=i in(4,5);dup=i in(6,7)
 receiver='recv'+kind+('mp'if mp else'dup'if dup else'')+('()'if mp else'(f)')
 sender='send'+kind+('mp'if mp else'')+'()'
 SOURCE+=name+'('+ARGS+'){prepare(f,a,r,w);'+(('writetuple=(off,data,fid,wr);'if write else'readtuple=(off,count,fid,rr);')if mp else'')+'spawn '+(sender if reverse else receiver)+';spawn '+(receiver if reverse else sender)+';}'
SOURCE+='prepare('+ARGS+'){stored=f;data=a;rr=r;wr=w;}clear(){stored=nil;data=nil;rr=nil;wr=nil;aout=nil;rout=nil;wout=nil;}'
for kind in('write','read'):
 for suffix in('','mp','dup'):
  owner='stored'if suffix=='mp'else'f';channels=(kind,kind)if suffix=='dup'else('write','read')
  SOURCE+='recv'+kind+suffix+('(){'if suffix=='mp'else'(f:ref Sys->FileIO){')+'alt{'
  for ordinal,field in enumerate(channels):
   SOURCE+=('(o,a,id,w):= <-'if field=='write'else'(o,n,id,r):= <-')+owner+'.'+field+' => offsetout=o;fidout=id;'+('aout=a;wout=w;'if field=='write'else'countout=n;rout=r;')+'choice='+str(ordinal+1)+';received++;'
  SOURCE+='}}'
 for mp in(0,1):
  SOURCE+='send'+kind+('mp'if mp else'')+'(){'+(''if mp else'v:='+('(off,data,fid,wr)'if kind=='write'else'(off,count,fid,rr)')+';')+'alt{stored.'+kind+' <-='+ (kind+'tuple'if mp else'v')+' => sent++;}}'
SOURCE+='inspect(f:ref Sys->FileIO){recvwrite(f);}'
def generate(limbo,w):
 interface=w/'filealt.m';interface.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Filealt','-I',str(ROOT/'module'),str(interface)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Filealt_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Filealt',(21,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 globals={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:out.write('static const uint file_globals[]={'+','.join(str(globals[n])for n in('stored','data','rr','wr','aout','rout','wout','off','count','fid','offsetout','countout','fidout','received','sent','choice'))+'};\n')
 types=w/'sys-types.m';types.write_text(PREFIX);(w/'fileio-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Sys','-I',str(ROOT/'module'),str(types)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True)
 imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(198,199,203,201,202):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('FileIO oracle missed',row['name'],pc,k))
 print('PASS FileIO source:',controls,'corruptions detected; independent classic maps/operands/descriptors/tables/hash and unchanged bytes',flush=True)
def boundaries(limbo,w):
 for ty in('(int,array of int,int,Sys->Rwrite)','(int,array of byte,big,Sys->Rwrite)','(int,int,int,Sys->Rwrite)','(int,int,int,chan of (string,array of byte))'):
  source='implement Closed;'+PREFIX+'Closed:module{run:fn(c:chan of '+ty+');};run(c:chan of '+ty+'){alt{(a,b,d,r):= <-c => ;}}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 for decl in('Other:adt{read:chan of (int,int,int,Sys->Rread);write:chan of (int,array of byte,int,Sys->Rwrite);};','Other:adt{read:chan of (int,int,int,Sys->Rread);write:chan of (int,array of int,int,Sys->Rwrite);};'):
  source='implement Closed;'+PREFIX+decl+'Closed:module{run:fn(f:ref Other);};run(f:ref Other){alt{(a,b,d,r):= <-f.read => ;}}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted']and rows[0]['reason']=='alt-field-channel',rows
 for second in('write:chan of (int,array of int,int,Rwrite);','wrong:chan of (int,array of byte,int,Rwrite);'):
  fake='Sys:module{Rread:type chan of (array of byte,string);Rwrite:type chan of (int,string);FileIO:adt{read:chan of (int,int,int,Rread);'+second+'};};'
  source='implement Closed;'+fake+'Closed:module{run:fn(f:ref Sys->FileIO);};run(f:ref Sys->FileIO){alt{(a,b,d,r):= <-f.read => ;}}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted']and rows[0]['reason']=='alt-field-channel',rows
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 init=next(r for r in rows if r['name']=='init');assert init['accepted']or init['pc']>158,init
 assert next(r for r in rows if r['name']=='command')['accepted']
 print('PASS FileIO boundaries; real AWM init:',init.get('pc','complete'),init.get('reason','accepted'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0]
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-fileio-alt.c"\n'+(ROOT/'utils/tests/native64-fileio-alt.c').read_text().replace(' FILE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent FileIO oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='fileio-alt-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit())
  arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  original=unit();extract=CMD['NS']['S']['FILES']['FILES']['function'];body=extract(original,'OP(movmp)');assert body.count('size = mappedcopytype(t);')==1
  controls=[('truncated mixed projection',None,None,original.replace(body,body.replace('size = mappedcopytype(t);','size = mappedcopytype(t);if(size==32)size=24;'),1),'exact mixed payload roots and untouched outputs'),('wrong FileIO field','native64-typed-code.h','p->s.i.s=s->returned==192?64:s->m;', 'p->s.i.s=s->returned==192?64:s->m;if(s->returned==198 && s->m==8 && USRC(s->mode)==(AFP|AIND))p->s.i.s^=8;','all mixed ALT peers resumed and retired'),('missing nonroot copy proof','native64-typed-module.h','for(at=0;at<width;at++){map=modes[k]', 'for(at=0;at<width;at++)if(native64fileioroot(contract,at)){map=modes[k]','malformed FileIO metadata refused')]
  for name,header,before,after,predicate in controls:
   if header:
    for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
    text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));source=original
   else:source=after
   (w/'unit.c').write_text(source);subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   if header:
    for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS FileIO active control:',name,flush=True)
if __name__=='__main__':main()
