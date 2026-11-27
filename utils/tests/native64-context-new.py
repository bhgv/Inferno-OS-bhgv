#!/usr/bin/env python3
"""Create and copy bounded Draw.Context objects in the checked C/JIT VM."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
PREFIX='include "sys.m";include "draw.m";draw:Draw;Context:import draw;'
WM='chan of (string,chan of (string,ref Draw->Wmcontext))';ARGS='d:ref Draw->Display,s:ref Draw->Screen,w:'+WM
DECL='Factory:module{make:fn('+ARGS+'):ref Context;empty:fn():ref Context;clone:fn(c:ref Context):ref Context;renew:fn('+ARGS+'):ref Context;store:fn('+ARGS+');copy:fn(src,dst:ref Context);copyglobals:fn();copyto:fn(src:ref Context);copyfrom:fn(dst:ref Context);same:fn(c:ref Context);bytes:fn(n:int):array of byte;direct:fn(a,b,c:chan of int):(chan of int,chan of int,chan of int);tuple:fn(a,b,c:chan of int):(chan of int,chan of int,chan of int);};'
SOURCE='implement Factory;'+PREFIX+DECL+'saved,other:ref Context;make('+ARGS+'):ref Context{return ref Context(d,s,w);}empty():ref Context{return ref Context;}clone(c:ref Context):ref Context{return ref Context(c.display,c.screen,c.wm);}renew('+ARGS+'):ref Context{c:=ref Context(d,s,w);c=ref Context(d,s,w);return c;}store('+ARGS+'){saved=make(d,s,w);}copy(src,dst:ref Context){dst.display=src.display;dst.screen=src.screen;dst.wm=src.wm;}copyglobals(){saved.display=other.display;saved.screen=other.screen;saved.wm=other.wm;}copyto(src:ref Context){saved.display=src.display;saved.screen=src.screen;saved.wm=src.wm;}copyfrom(dst:ref Context){dst.display=other.display;dst.screen=other.screen;dst.wm=other.wm;}same(c:ref Context){c.display=c.display;c.screen=c.screen;c.wm=c.wm;}bytes(n:int):array of byte{a:=array[n]of byte;return a;}direct(a,b,c:chan of int):(chan of int,chan of int,chan of int){return(a,b,c);}tuple(a,b,c:chan of int):(chan of int,chan of int,chan of int){(x,y,z):=direct(a,b,c);return(x,y,z);}'
def generate(limbo,w):
 m=w/'factory.m';m.write_text(PREFIX+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Factory','-I',str(ROOT/'module'),str(m)],text=True);signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Factory_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'draw-native.h').write_text(h)
 rows=G['generate'](limbo,w,SOURCE,'Factory',(13,0,0,'~0U'),export_signatures=signatures);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 with p.open('a')as out:
  globals={g[0]:g[1]for g in rows[0]['globals']};out.write('static const uint factory_globals[]={'+str(globals['saved'])+','+str(globals['other'])+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in(172,173,174):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,IndexError,KeyError,ValueError,StopIteration):controls+=1
    else:raise AssertionError(('Context oracle missed',r['name'],pc,k))
 print('PASS Context constructor/copy source:',controls,'record corruptions, generated Draw layout and unchanged classic bytes',flush=True)
def boundaries(limbo,w):
 for owner,declaration in(('Other','Other:adt{display:ref Draw->Display;screen:ref Draw->Screen;wm:'+WM+';};'),('Fake->Context','Fake:module{Context:adt{display:ref Draw->Display;screen:ref Draw->Screen;wm:'+WM+';};};')):
  source='implement Closed;'+PREFIX+declaration+'Closed:module{run:fn():ref '+owner+';};run():ref '+owner+'{return ref '+owner+';}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted']and rows[0]['reason']=='client-context-geometry',rows
 for source in (
  'implement Closed;'+PREFIX+'Other:adt{display:ref Draw->Display;};Closed:module{run:fn(src:ref Other,dst:ref Context);};run(src:ref Other,dst:ref Context){dst.display=src.display;}',
  'implement Closed;'+PREFIX+'Other:adt{display:ref Draw->Display;};Closed:module{run:fn(src:ref Context,dst:ref Other);};run(src:ref Context,dst:ref Other){dst.display=src.display;}'):
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert not rows[0]['accepted'],rows
 print('PASS Context allocation/copy boundary: other ADT, wrong namespace and foreign field owners refused; classic bytes preserved',flush=True)
def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text()
 before='Heap*\nheapz(Type *t)\n{';assert src.count(before)==1;src=src.replace(before,'static Type*factorytracked;static int factoryallocs,factoryfail;\n'+before+'\n if(t==factorytracked){factoryallocs++;if(factoryfail && factoryallocs==factoryfail)failat=allocations+1;}')
 src+='\nHeap*heap(Type*t){return heapz(t);}\n'+extract(vm,'static void\nnewrecord(')+'\n'+extract(vm,'OP(new)')+'\n'+extract(vm,'OP(newz)')+'\n'
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-context-new.c"\n'+(ROOT/'utils/tests/native64-context-new.c').read_text().replace(' FACTORY_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='context-new-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('copy-source-field','native64-typed-code.h','p->s.i.s = s->m;p->d.i.f=s->d;p->d.i.s=s->m;','p->s.i.s=s->m==0?8:s->m;p->d.i.f=s->d;p->d.i.s=s->m;','initialized Context payload'),
   ('copy-destination-field','native64-typed-code.h','p->s.i.s = s->m;p->d.i.f=s->d;p->d.i.s=s->m;','p->s.i.s=s->m;p->d.i.f=s->d;p->d.i.s=s->m==0?8:s->m;','initialized Context payload'),
   ('Context-descriptor','native64-typed-module.h','m->prog[b->offsets[i]+j].s.imm=m->ntype-1;','m->prog[b->offsets[i]+j].s.imm=s->nfunctions+1+b->imports+bytes;','separate Context/copy/byte descriptors')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Context allocation/copy active control:',name,flush=True)
if __name__=='__main__':main()
