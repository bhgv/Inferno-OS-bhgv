#!/usr/bin/env python3
"""Execute bounded String list prefixes over borrowed tails in imported calls."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
PREFIX='include "sys.m";include "draw.m";'
ARGS='ctx:ref Draw->Context,tail:list of string,head:string'
INTERFACE='Sink:module{run:fn(ctx:ref Draw->Context,args:list of string):list of string;observe:fn(ctx:ref Draw->Context,args:list of string);};'
DECL='Prefix:module{attach:fn(path:string);'+''.join(n+':fn('+ARGS+'):'+('list of string'if n!='observe'else'')+';'for n in('fresh','forward','two','fp','mp','observe'))+'clear:fn();};'
DECL=DECL.replace('):;',');')
SOURCE='implement Prefix;'+PREFIX+INTERFACE+DECL+'sink:Sink;saved:list of string;attach(path:string){sink=load Sink path;}fresh('+ARGS+'):list of string{a:=sink->run(ctx,"head"::tail);return a;}forward('+ARGS+'):list of string{return sink->run(ctx,"head"::tail);}two('+ARGS+'):list of string{return sink->run(ctx,"left"::"right"::tail);}fp('+ARGS+'):list of string{return sink->run(ctx,head::tail);}mp('+ARGS+'):list of string{saved=tail;return sink->run(ctx,"head"::saved);}observe('+ARGS+'){sink->observe(ctx,"head"::tail);}clear(){saved=nil;}'
def generate(limbo,w):
 m=w/'prefix.m';m.write_text(PREFIX+INTERFACE+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Sink','-I',str(ROOT/'module'),str(m)],text=True);frames={}
 for n in('run','observe'):
  size=int(re.search(r'^#define F_Sink_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(x,0)for x in re.search(r'^#define F_Sink_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if x];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n,sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Prefix',(8,1,0,'~0U'),import_frames=frames);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));globals={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:out.write('static const uint prefix_globals[]={'+str(globals['sink'])+','+str(globals['saved'])+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]!=187:continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('list prefix oracle missed',r['name'],pc,k))
 print('PASS borrowed list prefix source:',controls,'record corruptions, independent classic operands/maps and canonical import hashes',flush=True)
def sourceproofs(limbo,w):
 sh=PREFIX+'include "sh.m";'
 source='implement Probe;'+sh+'Probe:module{attach:fn(path:string);run:fn(c:ref Draw->Context,a:list of string):string;};shell:Sh;attach(path:string){shell=load Sh path;}run(c:ref Draw->Context,a:list of string):string{return shell->run(c,"{$*&}"::a);}'
 data,rows=E['compile_source'](limbo,w,source,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 for r in rows:CMD['verify_nilbranches'](data,r,imports,rows=rows)
 m=w/'sh-probe.m';m.write_text(sh+'Probe:module{run:fn();};');h=subprocess.check_output([str(limbo),'-H','-M','Sh','-I',str(ROOT/'module'),str(m)],text=True)
 signature=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sh_run (0x[0-9a-f]+)U',h,re.M)[1],16);site=next(r for r in rows if r['name']=='run')['externals'][0]
 assert site['name']=='run'and site['signature']==signature and site['contract']==24
 for count in(16,32):
  source='implement Bound;'+PREFIX+INTERFACE+'Bound:module{run:fn(sink:Sink,c:ref Draw->Context,a:list of string);};run(sink:Sink,c:ref Draw->Context,a:list of string){sink->observe(c,'+'"x"::'*count+'a);}'
  _,records=E['compile_source'](limbo,w,source,interface='',loads=True);assert len(records)==1 and records[0]['accepted'],records
 print('PASS list prefix boundary: canonical real Sh.run signature and 16/32 cons source chains; classic bytes unchanged',flush=True)
def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');before='List*\ncons(ulong size, void *slot)\n{';assert src.count(before)==1;src=src.replace(before,'static int prefixallocs,prefixfail;\n'+before+'\n prefixallocs++;if(prefixfail&&prefixallocs==prefixfail)failat=allocations+1;')
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-list-prefix.c"\n'+(ROOT/'utils/tests/native64-list-prefix.c').read_text().replace(' PREFIX_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='list-prefix-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);sourceproofs(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('temporary-clear','native64-typed-multi.h','else if(s->returned==187){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','else if(s->returned==187){checkstep=*s;checkstep.returned=0;if(s->op==IMOVP&&USRC(s->mode)==AMP){checkstep.mode=SRC(AFP)|DST(AFP);checkstep.s=s->d;}native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','prefix argument has one owner'),
   ('borrowed-tail','native64-typed-multi.h','else if(s->returned==187){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','else if(s->returned==187){checkstep=*s;checkstep.returned=0;if(s->op==IMOVP&&USRC(s->mode)==AFP){checkstep.mode=SRC(AFP)|DST(AFP);checkstep.s=s->d;}native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','exact borrowed tail identity')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS list prefix active control:',name,flush=True)
if __name__=='__main__':main()
