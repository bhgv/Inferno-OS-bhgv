#!/usr/bin/env python3
"""Execute checked mapped tuple channel allocation with production owners and C/JIT."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
TR=runpy.run_path(str(ROOT/'utils/tests/native64-channel-triples.py'));G=TR['G'];CMD=TR['CMD'];E=TR['E']
SOURCE='implement Factory;include "sys.m";include "draw.m";include "wmsrv.m";Service:module{run:fn();};Item:adt{n:int;};Factory:module{b:fn(n:int):chan of (string,chan of (string,ref Draw->Wmcontext));w:fn(n:int):chan of (ref Wmsrv->Client,chan of string);l:fn(n:int):chan of (ref Wmsrv->Client,array of byte,Sys->Rwrite);f:fn(n:int):chan of (list of string,array of byte);p:fn(n:int):chan of (Service,ref Item,chan of int);fresh:fn(n:int):chan of (string,chan of string);empty:fn():chan of (string,chan of string);constant:fn():chan of (string,chan of string);store:fn(n:int);renew:fn(n:int):chan of (string,chan of string);global:fn(n:int):chan of (string,chan of string);amp:fn(n:int):chan of (string,chan of string);clear:fn();bytes:fn(n:int):array of byte;context:fn():ref Draw->Context;triple:fn(a,b,c:chan of int):(chan of int,chan of int,chan of int);copytriple:fn(a,b,c:chan of int):(chan of int,chan of int,chan of int);};saved:chan of (string,chan of string);capacity:int;b(n:int):chan of (string,chan of (string,ref Draw->Wmcontext)){return chan[n]of (string,chan of (string,ref Draw->Wmcontext));}w(n:int):chan of (ref Wmsrv->Client,chan of string){return chan[n]of (ref Wmsrv->Client,chan of string);}l(n:int):chan of (ref Wmsrv->Client,array of byte,Sys->Rwrite){return chan[n]of (ref Wmsrv->Client,array of byte,Sys->Rwrite);}f(n:int):chan of (list of string,array of byte){return chan[n]of (list of string,array of byte);}p(n:int):chan of (Service,ref Item,chan of int){return chan[n]of (Service,ref Item,chan of int);}fresh(n:int):chan of (string,chan of string){c:=chan[n]of (string,chan of string);return c;}empty():chan of (string,chan of string){return chan of (string,chan of string);}constant():chan of (string,chan of string){return chan[3]of (string,chan of string);}store(n:int){saved=chan[n]of (string,chan of string);}renew(n:int):chan of (string,chan of string){c:=chan[3]of (string,chan of string);c=chan[n]of (string,chan of string);return c;}global(n:int):chan of (string,chan of string){saved=chan[n]of (string,chan of string);return saved;}amp(n:int):chan of (string,chan of string){capacity=n;saved=chan[capacity]of (string,chan of string);return saved;}clear(){saved=nil;}bytes(n:int):array of byte{a:=array[n]of byte;return a;}context():ref Draw->Context{return ref Draw->Context;}triple(a,b,c:chan of int):(chan of int,chan of int,chan of int){return(a,b,c);}copytriple(a,b,c:chan of int):(chan of int,chan of int,chan of int){(x,y,z):=triple(a,b,c);return(x,y,z);}'
def generate(limbo,w):
 rows=G['generate'](limbo,w,SOURCE,'Factory',(17,0,0,'~0U'));p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));globals={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:out.write('static const uint channel_globals[]={'+str(globals['saved'])+','+str(globals['capacity'])+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;ops={E['OPCODES']['INEWCMP']}
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[0]not in ops:continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,IndexError,KeyError,ValueError,StopIteration):controls+=1
    else:raise AssertionError(('channel oracle missed',r['name'],pc,k))
 print('PASS mapped channel source:',controls,'corruptions detected, independent classic operands/maps and identical old-profile bytes',flush=True)
def boundaries(limbo,w):
 for payload in('(int,int)','(string,int)','(string,string,string,string)','((string,string),string)'):
  source='implement Closed;Closed:module{make:fn():chan of '+payload+';};make():chan of '+payload+'{return chan of '+payload+';}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(rows)==1 and not rows[0]['accepted'],rows
 source='implement Closed;Payload:adt{a:string;b:chan of string;};Closed:module{make:fn():chan of Payload;};make():chan of Payload{return chan of Payload;}'
 _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(rows)==1 and not rows[0]['accepted'],rows
 print('PASS mapped channel boundary: raw, mixed, four-root, nested and ADT payloads remain closed',flush=True)
def unit():
 src=TR['unit']().split('\n#line 1 "native64-channel-triples.c"')[0];src=src.replace('#include "tests/leaf-cons-stub.h"','#define cnewcscalar unused_channel_allocator\n#define cnewcmapped unused_mapped_allocator\n#include "tests/leaf-cons-stub.h"\n#undef cnewcscalar\n#undef cnewcmapped');extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();heap=(ROOT/'libinterp/heap.c').read_text()
 for signature in('Heap*\nheaparray(','void\ninitarray('):src=src.replace(extract(src,signature),extract(heap,signature),1)
 src+='\n'+extract(heap,'void\nfreearray(')+'\nType Tword={1,0,0,4},Tlong={1,0,0,8},Treal={1,0,0,8};\n'
 src=src.replace('static int\nchannelbuild(void *arg)\n{','static int channelbuilds,channelfailure,channelfailbuild;\nstatic int\nchannelbuild(void *arg)\n{\n channelbuilds++;if(channelfailure&&channelbuilds==channelfailbuild)failat=allocations+channelfailure;')
 src+='\n'+extract(vm,'OP(movb)')+'\n'
 src+='\n#define movp actual_movp\n#define movw actual_movw\n#define movl actual_movl\n#define movf actual_movf\n#define movb actual_movb\n'
 for signature in('static WORD\nchannelargs(','Channel*\nnewc(','OP(newcb)','OP(newcw)','OP(newcl)','OP(newcf)','OP(newcp)','Channel*\ncnewcscalar(','OP(newcmp)','Channel*\ncnewcmapped('):src+='\n'+extract(vm,signature)+'\n'
 src+='\n#undef movp\n#undef movw\n#undef movl\n#undef movf\n#undef movb\n'
 start=src.index(' optab[IMOVW]=countmovw;');stop=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-mapped-channels.c"\n'+(ROOT/'utils/tests/native64-mapped-channels.c').read_text().replace(' CHANNEL_HANDLERS',src[start:stop])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='mapped-channels-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('descriptor-relocation','native64-typed-module.h','(f->code[j].s==3?mapped2:0);','0;','five separate mapped/Context/copy/byte descriptors'),
   ('capacity-root-map','native64-typed-module.h','if(mode!=AIMM){map=mode==AFP?f->map:s->datamap;bit=step->m/8;if((map[bit/8]&(128>>(bit%8)))!=0)return 0;}','if(0)return 0;','malformed channel metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS mapped channel active control:',name,flush=True)
if __name__=='__main__':main()
