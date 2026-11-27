#!/usr/bin/env python3
"""Execute checked Limbo exit across nested calls, results and spawned processes."""
import argparse,copy,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SP=runpy.run_path(str(ROOT/'utils/tests/native64-spawn.py'));G=SP['G'];CMD=SP['CMD'];E=SP['E']
ARGS='n:int,s:string,c:chan of string'
NAMES=('direct','nested','one','twice','temporary','scalar','rootresult','always','grandchild')
DECL='Stops:module{'+''.join(n+':fn('+ARGS+')'+(':int'if n=='scalar'else':string'if n=='rootresult'else'')+';'for n in NAMES)+'clear:fn();};'
BODY='saved=s;channel=c;count++;if(n<=0)exit;total+=n;'
SOURCE='implement Stops;'+DECL+'count,total:int;saved:string;channel:chan of string;direct('+ARGS+'){'+BODY+'}nested('+ARGS+'){helper(n,s,c);total+=100;}one('+ARGS+'){spawn helper(n,s,c);}twice('+ARGS+'){spawn helper(n,s,c);spawn helper(n,s,c);}temporary('+ARGS+'){a:=s+"!";helper(n,a,c);}scalar('+ARGS+'):int{if(n<=0)exit;return n;}rootresult('+ARGS+'):string{if(n<=0)exit;return s;}always('+ARGS+'){saved=s;channel=c;count++;exit;}grandchild('+ARGS+'){spawn relay(n,s,c);}clear(){saved=nil;channel=nil;}helper('+ARGS+'){'+BODY+'}relay('+ARGS+'){spawn helper(n,s,c);}'
def generate(limbo,w):
 rows=G['generate'](limbo,w,SOURCE,'Stops',(12,0,0,'~0U'));p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as o:o.write('static const uint exit_globals[]={'+','.join(str(gl[n])for n in('count','total','saved','channel'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[0]!=E['OPCODES']['IEXIT']:continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('exit oracle missed',r['name'],pc,k))
 print('PASS process exit source:',controls,'record corruptions detected; exact classic terminals and unchanged bytes',flush=True)
 # EXIT is terminal even in a result-valued function. RET still needs a result.
 for tail in('int','string'):
  source='implement Probe;Probe:module{run:fn(n:int):'+tail+';};run(n:int):'+tail+'{exit;}'
  _,r=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(r)==1 and r[0]['accepted'],r
  _,old=E['compile_source'](limbo,w,source,interface='',loads=False);assert len(old)==1 and not old[0]['accepted'],old
 print('PASS process exit source: resultless exit paths accepted independently of borrowed scalar/root result stores',flush=True)
def awmproof(limbo,w):
 data,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 command=next(r for r in rows if r['name']=='command');assert command['accepted'],command
 imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 CMD['verify_nilbranches'](data,command,imports,rows=rows)
 controls=0
 for pc,c in enumerate(command['code']):
  for k in range(6):
   bad=copy.deepcopy(command);bad['code'][pc][k]+=1
   try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
   except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
   else:raise AssertionError(('AWM command oracle missed',pc,k))
 assert controls==462
 assert any(c[0]==E['OPCODES']['IEXIT']for c in command['code'])
 init=next(r for r in rows if r['name']=='init');assert init['accepted']or init['pc']>=136,init
 print('PASS unchanged real awm.command: 462 code corruptions detected; complete independent opcode/operand/call/descriptor proof; awm.init reaches at least the former PC136 barrier',flush=True)
def unit():
 src=SP['unit']().split('\n#line 1 "native64-spawn.c"')[0]
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-process-exit.c"\n'+(ROOT/'utils/tests/native64-process-exit.c').read_text().replace(' EXIT_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='process-exit-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);awmproof(a.limbo.resolve(),w);src=w/'unit.c';source=unit();src.write_text(source);arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(src),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''));subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('exit-is-not-return','native64-typed-code.h','p->op=s->op;p->add=s->mode;p->reg=s->m;','p->op=s->op==IEXIT?IRET:s->op;p->add=s->mode;p->reg=s->m;','caller continuation after process exit'),
   ('preallocation-terminal','native64-typed-module.h','Inst probe;step=f->code+j;\n            if(!native64typedcode(&probe,1,step,1,f->bytes,s->databytes))return 0;\n            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;','Inst probe;step=f->code+j;\n            for(at=0;at<f->nsites;at++)if(f->sites[at].first<=j && j<=f->sites[at].end)return 0;','exit refusal before allocation')):
   header=(ROOT/'include'/path).read_text();assert header.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(header.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS process exit active control:',name,flush=True)
if __name__=='__main__':main()
