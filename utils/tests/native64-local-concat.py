#!/usr/bin/env python3
"""Execute one computed String argument across checked local call frames."""
import argparse,copy,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
EX=runpy.run_path(str(ROOT/'utils/tests/native64-process-exit.py'));G=EX['G'];CMD=EX['CMD'];E=EX['E']
ARGS='n:int,s:string,c:chan of string'
NAMES=('fresh','forward','reverse','fp','mp','observe','stop','nested')
DECL='Concat:module{'+''.join(n+':fn('+ARGS+')'+(''if n in('observe','stop')else':string')+';'for n in NAMES)+'clear:fn();};'
SOURCE='implement Concat;'+DECL+'count,total,number:int;saved,text:string;fresh('+ARGS+'):string{number=n;a:=accept("prefix"+s);total++;return a;}forward('+ARGS+'):string{number=n;return accept("prefix"+s);}reverse('+ARGS+'):string{number=n;return accept(s+"prefix");}fp('+ARGS+'):string{number=n;return accept(s+s);}mp('+ARGS+'):string{number=n;text=s;return accept("prefix"+text);}observe('+ARGS+'){number=n;consume("prefix"+s);total++;}stop('+ARGS+'){number=n;fatal("cannot run command: "+s);total++;}nested('+ARGS+'):string{number=n;return wrapper(s);}clear(){saved=nil;text=nil;}accept(s:string):string{count++;saved=s;if(number == -1)raise "local fault";if(number == -2)exit;return s;}consume(s:string){count++;saved=s;if(number == -1)raise "local fault";if(number == -2)exit;}fatal(s:string){count++;saved=s;exit;}wrapper(s:string):string{return accept("prefix"+s);}'
def generate(limbo,w):
 rows=G['generate'](limbo,w,SOURCE,'Concat',(13,0,0,'~0U'));p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as o:o.write('static const uint concat_globals[]={'+','.join(str(gl[n])for n in('count','total','saved'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[0]!=E['OPCODES']['IADDC']or c[5]!=64:continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('local concat oracle missed',r['name'],pc,k))
 print('PASS local concat source:',controls,'corruptions detected, independent classic call/argument/result/frame proof and unchanged bytes',flush=True)
 for source in('implement Bad;Bad:module{run:fn(s:string);};run(s:string){spawn child("x"+s);}child(s:string){}','implement Bad;Bad:module{run:fn(s:string);};run(s:string){child("x"+s,0);}child(s:string,n:int){}'):
  _,r=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(x for x in r if x['name']=='run')['accepted'],r
 print('PASS local concat boundary: allocating spawn arguments and multiple formal arguments remain closed',flush=True)
def awmproof(limbo,w):
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);command=next(r for r in rows if r['name']=='command');init=next(r for r in rows if r['name']=='init');assert command['accepted']and (init['accepted']or init['pc']>136),init
 print('PASS real awm.init advances past PC136:',init.get('pc','complete'),init.get('reason','accepted'),flush=True)
def unit():
 src=EX['unit']().split('\n#line 1 "native64-process-exit.c"')[0];before='OP(addc)\n{';assert src.count(before)==1;src=src.replace(before,'static int concatfail;\n'+before+'\n if(concatfail){concatfail=0;failat=allocations+1;}')
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-local-concat.c"\n'+(ROOT/'utils/tests/native64-local-concat.c').read_text().replace(' CONCAT_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent local concat oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='local-concat-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);awmproof(a.limbo.resolve(),w);src=w/'unit.c';src.write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''));subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('middle-offset','native64-typed-multi.h','if(s->op==IADDC)p->reg=s->m;','if(s->op==IADDC)p->reg=s->m+((s->mode&ARM)==AXINF?8:0);','complete computed argument length'),
   ('callee-root','native64-typed-multi.h','bit=s->d/8;return bit/8<np && (map[bit/8]&(0x80>>(bit%8)));','bit=s->d/8;return bit/8<np;','malformed local concat metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS local concat active control:',name,flush=True)
if __name__=='__main__':main()
