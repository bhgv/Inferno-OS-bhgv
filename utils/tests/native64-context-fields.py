#!/usr/bin/env python3
"""Execute checked Draw.Context FP/MP field transfers in the real C/JIT VM."""
import argparse,copy,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD']
SOURCE='''implement Contexts;include "sys.m";include "draw.m";draw:Draw;
Contexts:module{capture:fn(ctxt:ref Draw->Context);publish:fn(ctxt:ref Draw->Context);captureglobal:fn();publishglobal:fn();forget:fn(ctxt:ref Draw->Context);forgetglobal:fn();roundtrip:fn(ctxt:ref Draw->Context);};
display:ref Draw->Display;screen:ref Draw->Screen;wm:chan of (string,chan of (string,ref Draw->Wmcontext));saved:ref Draw->Context;
capture(ctxt:ref Draw->Context){if(ctxt.display==nil)display=nil;else display=ctxt.display;if(ctxt.screen!=nil)screen=ctxt.screen;else screen=nil;if(ctxt.wm==nil)wm=nil;else wm=ctxt.wm;}
publish(ctxt:ref Draw->Context){ctxt.display=display;ctxt.screen=screen;ctxt.wm=wm;}
captureglobal(){if(saved.display==nil)display=nil;else display=saved.display;if(saved.screen!=nil)screen=saved.screen;else screen=nil;if(saved.wm==nil)wm=nil;else wm=saved.wm;}
publishglobal(){saved.display=display;saved.screen=screen;saved.wm=wm;}
forget(ctxt:ref Draw->Context){ctxt.display=nil;ctxt.screen=nil;ctxt.wm=nil;}
forgetglobal(){saved.display=nil;saved.screen=nil;saved.wm=nil;}
roundtrip(ctxt:ref Draw->Context){d:=ctxt.display;s:=ctxt.screen;w:=ctxt.wm;ctxt.display=d;ctxt.screen=s;ctxt.wm=w;display=d;screen=s;wm=w;}
'''
def generate(limbo,w):
 rows=G['generate'](limbo,w,SOURCE,'Contexts',(7,0,0,'~0U'))
 (w/'full.h').write_text((w/'full.h').read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 globals={g[0]:g[1]for g in rows[0]['globals']}
 with(w/'full.h').open('a')as out:
  for n in('display','screen','wm','saved'):out.write(f'static const uint context_{n}={globals[n]};\n')
 data,reports=G['E']['compile_source'](limbo,w,SOURCE,interface='',loads=True)
 controls=0
 for row in reports:
  for pc,step in enumerate(row['code']):
   if step[5]not in(96,97,98,99):continue
   for field in(0,1,2,3,4,5):
    bad=copy.deepcopy(row);bad['code'][pc][field]+=1
    try:G['E']['verify'](data,bad,[],loads=True,rows=reports)
    except(AssertionError,KeyError,IndexError,StopIteration):controls+=1
    else:raise AssertionError(('field oracle missed mutation',row['name'],pc,field))
 print('PASS Context source oracle:',controls,'field record mutations detected, classic bytes unchanged',flush=True)
 return rows

def refusals(limbo,w):
 for owner,declaration in(('Other','Other:adt{display:ref Draw->Display;screen:ref Draw->Screen;wm:chan of (string,chan of (string,ref Draw->Wmcontext));};'),('Fake->Context','Fake:module{Context:adt{display:ref Draw->Display;screen:ref Draw->Screen;wm:chan of (string,chan of (string,ref Draw->Wmcontext));};};')):
  source='implement Closed;include "sys.m";include "draw.m";'+declaration+'Closed:module{run:fn(c:ref '+owner+');};display:ref Draw->Display;run(c:ref '+owner+'){display=c.display;}'
  _,rows=G['E']['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted']and rows[0]['reason']=='context-field-roles',rows
 print('PASS Context compiler boundary: identical-layout ADT and wrong namespace refused; ordinary bytes preserved',flush=True)

def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-context-fields.c"\n'+(ROOT/'utils/tests/native64-context-fields.c').read_text().replace(' CONTEXT_HANDLERS',src[start:end])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='context-fields-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);refusals(a.limbo.resolve(),w)
  (w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('field-displacement','native64-typed-code.h','p->s.i.s=s->m;','p->s.i.s=0;','exact MP field value'),
   ('branch-displacement','native64-typed-code.h','p->s.i.s=8*(s->returned-Native64TypedContextNil);','p->s.i.s=0;','exact MP field value'),
   ('mapped-roots','native64-typed-module.h','if((map[bit/8] & (128>>(bit%8)))==0)return 0;','if(0)return 0;','malformed Context metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   header=w/path;header.write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Context active control:',name,flush=True)

if __name__=='__main__':main()
