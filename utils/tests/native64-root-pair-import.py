#!/usr/bin/env python3
"""Execute two-root results across imported C and native64 Limbo modules."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD,E=G['CMD'],G['E']
ARGS='flag:int,a,b:string'
PAIR='(string,string)'
INTERFACE='Sink:module{produce:fn('+ARGS+'):'+PAIR+';};'
NAMES=('direct','fresh','copied','forward','produce')
DECL='Pairs:module{open:fn();'+''.join(n+':fn('+ARGS+'):'+PAIR+';'for n in NAMES)+'};'
SOURCE='implement Pairs;'+INTERFACE+DECL+'sink:Sink;open(){sink=load Sink "sink";if(sink==nil)raise "missing";}'
SOURCE+='direct('+ARGS+'):'+PAIR+'{return sink->produce(flag,a,b);}'
SOURCE+='fresh('+ARGS+'):'+PAIR+'{(x,y):=sink->produce(flag,a,b);return(x,y);}'
SOURCE+='copied('+ARGS+'):'+PAIR+'{(x,y):=sink->produce(flag,a,b);value:=(x,y);return value;}'
SOURCE+='forward('+ARGS+'):'+PAIR+'{return direct(flag,a,b);}'
SOURCE+='produce('+ARGS+'):'+PAIR+'{if(flag)raise "pairfault";return(a,b);}'

def declarations(limbo,w):
 roots=('string','list of string','array of byte','ref P->R','chan of int','P')
 for left in roots:
  for right in roots:
   pair='('+left+','+right+')'
   interface='P:module{R:adt{v:int;};get:fn():'+pair.replace('P->','')+';};'
   source='implement Client;include "provider.m";Client:module{run:fn();forward:fn():'+pair+';open:fn();};p:P;left:'+left+';right:'+right+';run(){(a,b):=p->get();left=a;right=b;}forward():'+pair+'{return p->get();}open(){p=load P "provider";}'
   data,rows=E['compile_source'](limbo,w,source,interface=interface,loads=True)
   assert len(rows)==3 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
   imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
   for r in rows:
    E['verify'](data,r,imports,loads=True,rows=rows)
    if r['name']!='open':assert r['externals'][0]['contract']==48
   assert rows[1]['externals'][0]['inherited'] and rows[1]['externals'][0]['resultoffset']==32
 print('PASS imported root pairs: 36 declared combinations, destructured and inherited storage, exact classic bytes',flush=True)

def generate(limbo,w):
 path=w/'pairs.m';path.write_text(INTERFACE+DECL)
 headers={n:subprocess.check_output([str(limbo),'-H','-M',n,str(path)],text=True)for n in('Sink','Pairs')}
 h=headers['Sink'];size=int(re.search(r'^#define F_Sink_produce_size (\d+)',h,re.M)[1])
 mapping=[int(x,0)for x in re.search(r'^#define F_Sink_produce_map \{([^}]*)\}',h,re.M)[1].split(',')if x]
 sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_produce (0x[0-9a-f]+)U',h,re.M)[1],16)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Pairs_(\w+) (0x[0-9a-f]+)U',headers['Pairs'],re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Pairs',(6,1,0,'~0U'),import_frames={('produce',sig):(size,mapping)},export_signatures=signatures)
 p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint pair_global='+str(next(g[1]for g in rows[0]['globals']if g[0]=='sink'))+';\nstatic const uint pair_functions[]={'+','.join(str(next(i for i,r in enumerate(rows)if r['name']==n))for n in NAMES)+'};\n')
 data,rows=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in rows:
  for e in r.get('externals',[]):
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][e['end']-1][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('missed imported pair binding',r['name'],k))
   for k in range(2):
    bad=copy.deepcopy(r);bad['externals'][0]['result_pair'][k]=12
    try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('missed imported pair kind',r['name'],k))
 print('PASS imported pair independent oracle:',controls,'binding operand and declared-kind mutations detected',flush=True)

def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text()
 src+='\n'+extract((ROOT/'libinterp/heap.c').read_text(),'void\nincmem(')+'\n'+extract(vm,'static int\nmappedcopytype(')+'\n'+extract(vm,'OP(movmp)')
 at=src.index(' optab[IMOVW]=countmovw;');handlers=src[at:src.index(' require(prepare64calls(',at)]
 return src+'\n'+(ROOT/'utils/tests/native64-root-pair-import.c').read_text().replace('PAIR_HANDLERS',handlers)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args();limbo=a.limbo.resolve()
 with tempfile.TemporaryDirectory(prefix='root-pair-import-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](limbo,w,'Command',False,argv=True,local=True,pair=True,raises=True);declarations(limbo,w);generate(limbo,w);(w/'unit.c').write_text(unit())
  arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True)
  runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)]
  subprocess.run(runner,check=True,timeout=180)
  # Ensure the independent materializer controls require both result proofs.
  parents=('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-local-bind.h')
  for before,predicate in (
   ('((c->contract==48)!=(c->resultcontract==48))','callee pair proof required'),
   ('(c->contract==48 && c->inherited && c->callerresultcontract!=48)','caller pair proof required')):
   for name in parents:(w/name).write_text((ROOT/'include'/name).read_text())
   p=w/'native64-typed-multi.h';text=p.read_text();assert text.count(before)==1;p.write_text(text.replace(before,'0'))
   subprocess.run(command,check=True);result=subprocess.run(runner,capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout,result.stderr)
   for name in parents:(w/name).unlink()
   print('PASS imported pair active control:',predicate,flush=True)
if __name__=='__main__':main()
