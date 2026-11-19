#!/usr/bin/env python3
"""Execute checked Wmsrv request ALT triples and mapped three-root copies."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ALT=runpy.run_path(str(ROOT/'utils/tests/native64-alt.py'));G=ALT['G'];CMD=ALT['CMD'];E=ALT['E']
PREFIX='include "sys.m";include "draw.m";'
PAIR='(ref Draw->Pointer,array of byte,Sys->Rwrite)'
ARGS='c:chan of '+PAIR+',p:ref Draw->Pointer,a:array of byte,r:Sys->Rwrite,other:chan of '+PAIR
DECL='Requestselect:module{'+''.join(n+':fn('+ARGS+');'for n in('run','reverse','duplicate','mp'))+'mkrequest:fn():chan of '+PAIR+';clear:fn();mkpair:fn():chan of (ref Draw->Pointer,chan of string);mkbytes:fn(n:int):array of byte;mkcontext:fn():ref Draw->Context;mktriple:fn(a,b,c:chan of int):(chan of int,chan of int,chan of int);copytriple:fn(a,b,c:chan of int):(chan of int,chan of int,chan of int);};'
SOURCE='implement Requestselect;'+PREFIX+DECL+'shared,alternate:chan of '+PAIR+';sourcep,oldp,pout:ref Draw->Pointer;sourcea,olda,aout:array of byte;sourcer,oldr,rout:Sys->Rwrite;both:'+PAIR+';received,sent,choice:int;'
SOURCE+='run('+ARGS+'){prepare(c,p,a,r,other);unused:=mkrequest();spawn reader();spawn sender();}reverse('+ARGS+'){prepare(c,p,a,r,other);spawn sender();spawn readerrev();}duplicate('+ARGS+'){prepare(c,p,a,r,other);spawn readerdup();spawn sender();}mp('+ARGS+'){prepare(c,p,a,r,other);both=(p,a,r);spawn reader();spawn sendermp();}mkrequest():chan of '+PAIR+'{return chan of '+PAIR+';}clear(){oldp=nil;olda=nil;oldr=nil;pout=nil;aout=nil;rout=nil;}'
SOURCE+='prepare('+ARGS+'){shared=c;alternate=other;sourcep=p;sourcea=a;sourcer=r;}'
for name,channels in(('reader',('shared','alternate')),('readerrev',('alternate','shared')),('readerdup',('shared','shared'))):
 SOURCE+=name+'(){alt{'+''.join('(p,a,r):= <-'+c+' => pout=p;aout=a;rout=r;choice='+str(i+1)+';received++;'for i,c in enumerate(channels))+'}}'
SOURCE+='sender(){v:=(sourcep,sourcea,sourcer);alt{shared <-=v => sent++;}}sendermp(){alt{shared <-=both => sent++;}}'
SOURCE+='mkpair():chan of (ref Draw->Pointer,chan of string){return chan of (ref Draw->Pointer,chan of string);}mkbytes(n:int):array of byte{a:=array[n]of byte;return a;}mkcontext():ref Draw->Context{return ref Draw->Context;}mktriple(a,b,c:chan of int):(chan of int,chan of int,chan of int){return(a,b,c);}'
SOURCE+='copytriple(a,b,c:chan of int):(chan of int,chan of int,chan of int){(x,y,z):=mktriple(a,b,c);return(x,y,z);}'
def generate(limbo,w):
 m=w/'requestselect.m';m.write_text(PREFIX+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Requestselect','-I',str(ROOT/'module'),str(m)],text=True);canonical={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Requestselect_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Requestselect',(17,0,0,'~0U'),export_signatures=canonical);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as o:o.write('static const uint request_globals[]={'+','.join(str(gl[n])for n in('oldp','oldr','pout','rout','received','sent','choice','olda','aout'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if not(c[5]in(196,197)):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('request oracle missed',row['name'],pc,k))
 print('PASS ALT request source:',controls,'corruptions detected; classic tuple descriptor/operands/tables/maps and unchanged bytes',flush=True)
def boundaries(limbo,w):
 for ty in('(ref Draw->Pointer,int)','(ref Draw->Pointer,chan of int)','(ref Draw->Pointer,ref Draw->Pointer)','(ref Draw->Pointer,array of int,Sys->Rwrite)','(ref Draw->Pointer,array of byte,chan of (string,int))','(ref Draw->Pointer,array of byte,chan of (int,int))','(ref Draw->Pointer,array of byte,chan of string)'):
  names=','.join('x'+str(i)for i in range(3 if 'array' in ty else 2));source='implement Closed;'+PREFIX+'Closed:module{run:fn(c:chan of '+ty+');};run(c:chan of '+ty+'){alt{('+names+'):= <-c => ;}}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 prefix=PREFIX+'include "wmsrv.m";';decl='Probe:module{run:fn(c:chan of (ref Wmsrv->Client,array of byte,Sys->Rwrite));};';source='implement Probe;'+prefix+decl+'client:ref Wmsrv->Client;bytes:array of byte;reply:Sys->Rwrite;run(c:chan of (ref Wmsrv->Client,array of byte,Sys->Rwrite)){alt{(p,a,r):= <-c => client=p;bytes=a;reply=r;}}'
 data,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert rows[0]['accepted'],rows;CMD['verify_nilbranches'](data,rows[0],[],rows=rows)
 interface=w/'join.m';interface.write_text(prefix+decl);h=subprocess.check_output([str(limbo),'-H','-M','Probe','-I',str(ROOT/'module'),str(interface)],text=True);signature=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Probe_run (0x[0-9a-f]+)U',h,re.M)[1],16);assert E['BASE']['READ']['legacy_instructions'](data,with_exports=True)[-1][0]['signature']==signature
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in rows if r['name']=='init');assert not init['accepted']and init['pc']>153,init;assert next(r for r in rows if r['name']=='command')['accepted']
 print('PASS real Wmsrv.Client req triple and canonical hash; wider/different tuples closed; AWM init:',init['pc'],init['reason'],flush=True)
def unit():
 base=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'))
 src=base['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0]
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-alt-requests.c"\n'+(ROOT/'utils/tests/native64-alt-requests.c').read_text().replace(' REQUEST_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent request oracles require assertions')
 with tempfile.TemporaryDirectory(prefix='alt-requests-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  # Each control must fail its semantic predicate, without a sanitizer fault.
  original=unit();extract=CMD['NS']['S']['FILES']['FILES']['function'];body=extract(original,'OP(movmp)');assert body.count('size = mappedcopytype(t);')==1
  controls=[('short mapped copy',None,original.replace(body,body.replace('size = mappedcopytype(t);','size = mappedcopytype(t);if(size==24)size=16;'),1),'exact three-root tuple payload'),('missing third copy root','native64-typed-module.h',None,'malformed request metadata refused')]
  for name,header,source,predicate in controls:
   if header:
    for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
    text=(w/header).read_text();before='at<offsets[k]+24';assert text.count(before)==1;(w/header).write_text(text.replace(before,'at<offsets[k]+16',1));source=original
   (w/'unit.c').write_text(source);subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   if header:
    for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS ALT request active control:',name,flush=True)
if __name__=='__main__':main()
