#!/usr/bin/env python3
"""Execute checked int32 list construction/head/tail and canonical pctl arguments."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
ARGS='v:int,tail:list of int'
INTERFACE='Sink:module{accept:fn(flags:int,fds:list of int):int;observe:fn(flags:int,fds:list of int);};'
DECL='Ints:module{attach:fn(path:string);literal:fn('+ARGS+'):list of int;prepend:fn('+ARGS+'):list of int;global:fn('+ARGS+'):list of int;head:fn('+ARGS+'):int;tail:fn('+ARGS+'):list of int;length:fn('+ARGS+'):int;pctl:fn('+ARGS+'):int;consume:fn('+ARGS+');clear:fn();localhead:fn('+ARGS+'):int;replace:fn('+ARGS+'):list of int;};'
SOURCE='implement Ints;'+INTERFACE+DECL+'sink:Sink;saved:list of int;number:int;attach(path:string){sink=load Sink path;}literal('+ARGS+'):list of int{a:=list of {0,1,2};return a;}prepend('+ARGS+'):list of int{a:=v::tail;return a;}global('+ARGS+'):list of int{number=v;saved=tail;saved=number::saved;return saved;}head('+ARGS+'):int{return hd tail;}tail('+ARGS+'):list of int{return tl tail;}length('+ARGS+'):int{return len tail;}pctl('+ARGS+'):int{return sink->accept(v,tail);}consume('+ARGS+'){sink->observe(v,tail);}clear(){saved=nil;}localhead('+ARGS+'):int{a:=v::tail;return hd a;}replace('+ARGS+'):list of int{a:=v::tail;a=0::a;return a;}'
def generate(limbo,w):
 m=w/'integers.m';m.write_text(INTERFACE+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Sink',str(m)],text=True);frames={}
 for n in('accept','observe'):
  size=int(re.search(r'^#define F_Sink_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(x,0)for x in re.search(r'^#define F_Sink_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if x];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n,sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Ints',(12,1,0,'~0U'),import_frames=frames);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));globals={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:out.write('static const uint integer_globals[]={'+str(globals['sink'])+','+str(globals['saved'])+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[0]not in(E['OPCODES']['ICONSW'],E['OPCODES']['IHEADW']):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('int list oracle missed',r['name'],pc,k))
 print('PASS integer list source:',controls,'corruptions, independent classic operands/maps and unchanged bytes',flush=True)
def pctlproof(limbo,w):
 source='implement Probe;include "sys.m";Probe:module{run:fn(sys:Sys):int;};run(sys:Sys):int{fds:=list of {0,1,2};return sys->pctl(Sys->NEWFD,fds);}'
 data,rows=E['compile_source'](limbo,w,source,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 h=subprocess.check_output([str(limbo),'-H','-M','Sys',str(ROOT/'module/sys.m')],text=True);sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sys_pctl (0x[0-9a-f]+)U',h,re.M)[1],16);call=rows[0]['externals'][0];assert call['name']=='pctl'and call['signature']==sig and call['contract']==4
 assert int(re.search(r'^#define F_Sys_pctl_size (\d+)',h,re.M)[1])==80
 assert [int(x,0)for x in re.search(r'^#define F_Sys_pctl_map \{([^}]*)\}',h,re.M)[1].split(',')if x]==[0,0x40]
 assert not imports and call['binding']==24 # Built-in Sys uses its fixed module table.
 imports=[{'name':'','signature':0}for _ in range(25)];imports[24]={'name':'pctl','signature':sig}
 for r in rows:CMD['verify_nilbranches'](data,r,imports,rows=rows)
 print('PASS canonical Sys.pctl source: fd literal0/1/2 and generated native signature; no fd service executed',flush=True)
def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();before='List*\ncons(ulong size, void *slot)\n{';assert src.count(before)==1;src=src.replace(before,'static int intallocs,intfail;\n'+before+'\n intallocs++;if(intfail&&intallocs==intfail)failat=allocations+1;');src+='\n'+extract(vm,'OP(consw)')+'\n'+extract(vm,'OP(headw)')+'\n'
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-integer-lists.c"\n'+(ROOT/'utils/tests/native64-integer-lists.c').read_text().replace(' INTEGER_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 if not __debug__:p.error('independent integer-list oracles require assertions')
 with tempfile.TemporaryDirectory(prefix='integer-lists-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);pctlproof(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('head-result-offset','native64-typed-code.h','p->d.i.s=s->returned==56?8:0;','p->d.i.s=s->op==IHEADW?4:s->returned==56?8:0;','int32 scalar result'),
   ('root-map','native64-typed-module.h','if(!!(map[bit/8] & (0x80>>(bit%8)))!=root)return 0;','if(0)return 0;','malformed integer list metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS integer list active control:',name,flush=True)
if __name__=='__main__':main()
