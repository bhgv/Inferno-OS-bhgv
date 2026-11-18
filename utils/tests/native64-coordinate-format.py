#!/usr/bin/env python3
"""Execute unchanged WM rectangle formatters and mixed typed tails in the real VM."""
import argparse,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD']
def generate(limbo,w):
 extract=CMD['NS']['S']['FILES']['FILES']['function']
 wm=extract((ROOT/'appl/lib/wmclient.b').read_text(),'r2s(r: Rect): string').replace('r2s(', 'wmrect(',1)
 awm=extract((ROOT/'appl/wm/awm.b').read_text(),'r2s(r: Rect): string').replace('r2s(', 'awmrect(',1)
 source='implement Coordinates;include "sys.m";sys:Sys;include "draw.m";draw:Draw;Rect:import draw;Coordinates:module{init:fn();wmrect:fn(r:Rect):string;awmrect:fn(r:Rect):string;mix:fn(n:int,b:big,r:real,s:string):string;fresh:fn(n:int,b:big,r:real,s:string):string;};init(){sys=load Sys Sys->PATH;}'+wm+awm+'mix(n:int,b:big,r:real,s:string):string{return sys->sprint("%d %bd %.2f %s",n,b,r,s);}fresh(n:int,b:big,r:real,s:string):string{value:=sys->sprint("%d %bd %.2f %s",n,b,r,s);return value+"!";}'
 rows=G['generate'](limbo,w,source,'Coordinates',(5,1,3,'~0U'))
 with(w/'full.h').open('a')as out:
  out.write('static const uint mixed_sys_offset='+str(next(g[1]for g in rows[0]['globals']if g[0]=='sys'))+';\n')
 print('PASS unchanged WM helper sources: wmclient.r2s and awm.r2s bodies, full classic relocation and mixed/inherited slot oracles',flush=True)

def refusals(limbo,w):
 E=G['E']
 for kind in('array of int','chan of string','list of string','ref Opaque'):
  source='implement Closed;include "sys.m";Opaque:adt{n:int;};Closed:module{run:fn(sys:Sys,fmt:string,x:'+kind+'):string;};run(sys:Sys,fmt:string,x:'+kind+'):string{return sys->sprint("%H",x);}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted']and rows[0]['reason']=='variadic-argument-kind',rows
 source='implement Closed;include "sys.m";Closed:module{run:fn(sys:Sys,fmt:string):string;};run(sys:Sys,fmt:string):string{return sys->sprint("'+' '.join(['%d']*257)+'",'+','.join(['1']*257)+');}'
 _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
 assert len(rows)==1 and not rows[0]['accepted']and rows[0]['reason']=='variadic-arguments',rows
 print('PASS numeric variadic compiler refusals: four opaque managed tails and 257-slot input; ordinary bytes preserved',flush=True)

def unit(w):
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
 src+='\n#define LIMBO_NATIVE64_PREFIXES\n#include "sys-native64.h"\n'
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src=src.replace(extract(src,'int\nsnprint('),'',1);runtime=(ROOT/'libinterp/runt.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
 src+='\nchar*syserr(char*a,char*b,Prog*p){USED(b);USED(p);strcpy(a,"fixture");return a;}\nvoid rerrstr(char*p,uint n){if(n)snprintf(p,n,"fixture");}\n'
 a=runtime.index('typedef struct XprintString');b=runtime.index('/* Validate borrowed string',a);core=runtime[a:b]
 a=runtime.index('/* Private reader categories');b=runtime.index('/* Preserve packed legacy strides',a);core+=runtime[a:b]
 for sig in('static int\nxsnprint(','static void\nxprintstring(','static uchar*\nxprintargument(','static void\nxprinttake(','static int\nxprintwithargs(','Array*\nmem2array('):core+='\n'+extract(runtime,sig)+'\n'
 src+='\n'+core+'\n#include "native64-format.h"\n#include "native64-sys-format.h"\n#include "native64-sys-frame.h"\n#include "native64-sys-sites.h"\n#include "native64-sys-program.h"\n#include "native64-sys-module.h"\n'
 for sig in('int\nSys_sprint64(','int\nSys_setprogram64(','void\nSys_programcall64(','int\nSys_clearframes64('):src+='\n'+extract(runtime,sig).replace('Sys_sprint64(', 'productionSys_sprint64(')+'\n'
 src+='\n'+extract((ROOT/'emu/port/errstr.c').read_text(),'void\nkgerrstr(')+'\n'
 for sig in('String*\nstringdup(','void\ncvtup(','String*\naddstring(','OP(addc)','OP(cvtwc)'):src+='\n'+extract(strings,sig)+'\n'
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 src+='\n#line 1 "native64-coordinate-format.c"\n'+(ROOT/'utils/tests/native64-coordinate-format.c').read_text().replace(' MIX_HANDLERS',src[start:end])
 return src

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='coordinate-format-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);refusals(a.limbo.resolve(),w)
  (w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  extra=[str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]
  binary=w/'unit';command=cc+[str(w/'unit.c')]+extra+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after in (
   ('borrowed-result','native64-sys-frame.h','if(call->borrowed && address!=call->borrowed)return nil;','if(0)return nil;'),
   ('sign-extension','native64-sys-module.h','out->op=step->op;out->add=step->mode;','out->op=step->op==ICVTWL?IMOVW:step->op;out->add=step->mode;')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1;header=w/path;header.write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and 'source execution completed'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   header.unlink();print('PASS coordinate active control:',name,flush=True)
if __name__=='__main__':main()
