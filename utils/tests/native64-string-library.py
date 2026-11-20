#!/usr/bin/env python3
"""Execute every original String-library body with its native64 exported ABI."""
import argparse,copy,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-string-word.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
OP_NAMES=set()
SOURCE=(ROOT/'appl/lib/string.b').read_text()
def generate(limbo,w):
 h=subprocess.check_output([str(limbo),'-H','-M','String',str(ROOT/'module/string.m')],text=True);signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_String_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 global OP_NAMES
 rows=G['generate'](limbo,w,SOURCE,'String',(21,0,0,'~0U'),export_signatures=signatures);OP_NAMES={name[1:].lower()for name,op in E['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])};p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')
 s+='\nstruct LibraryFunction {uint fn,argc,kinds[2],result,contract;};\nstatic const struct LibraryFunction library_functions[]={\n'
 for i,r in enumerate(rows):
  if r['export_name']not in signatures:continue
  kind=r['result'][0];result=kind if kind!=14 else 100+r['result_mixed'][0]if r.get('result_mixed')else 100
  contract=16 if r.get('result_mixed')else 48 if r.get('result_pair')else r['result'][1]|(16 if kind in E['ROOTS']else 0)
  s+='{'+','.join(map(str,(i,len(r['args']))))+',{'+','.join(str(a[1])for a in r['args'])+'},'+str(result)+','+str(contract)+'},\n'
 s+='};\n';p.write_text(s)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in(137,140,144,145,146,147,149,150,151,152,40,56)and c[0]!=E['OPCODES']['IINSC']:continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,[],rows=records)
    except(AssertionError,ValueError,KeyError,IndexError,StopIteration):controls+=1
    else:raise AssertionError(('String oracle missed',r['name'],pc,k))
 print('PASS complete original String: 21 bodies, canonical .m hashes, unchanged classic bytes,',controls,'independent mutations',flush=True)
def unit():
 original=BASE['BASE']['unit']();src=original.split('\n#line 1 "native64-alt-pairs.c"')[0];extract=CMD['NS']['S']['FILES']['FILES']['function'];strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('void\nfreestring(','String*\nstringdup(','void\ncvtup(','String*\naddstring(','String*\nslicer(','OP(addc)','OP(indc)','OP(insc)','OP(slicec)','OP(cvtcw)','OP(cvtcf)'):
  if sig not in src:src+='\n'+extract((ROOT/'libinterp/heap.c').read_text()if 'freestring'in sig else strings,sig)+'\n'
 xec=(ROOT/'libinterp/xec.c').read_text()
 if 'static void\nbranchstring('not in src:
  src+='\n'+extract(xec,'static void\nbranchstring(')+'\n'
 for n in('beqc','bnec','bltc','blec','bgtc','bgec'):
  if 'OP('+n+')'not in src:src+='\n'+extract(xec,'OP('+n+')')+'\n'
 for n in sorted(OP_NAMES):
  if 'OP('+n+')'not in src:
   owner= (ROOT/'libinterp/xec.c').read_text()if 'OP('+n+')'in (ROOT/'libinterp/xec.c').read_text()else strings
   src+='\n'+extract(owner,'OP('+n+')')+'\n'
 start=original.rindex(' optab[IMOVW]=countmovw;');end=original.index(' optab[IMOVP]=actual_movp;',start)
 handlers=''.join(' optab['+name+']=actual_'+name[1:].lower()+';\n'for name in E['OPCODES']if 'OP('+name[1:].lower()+')'in src)
 return src+'\n#line 1 "native64-string-library.c"\n'+(ROOT/'utils/tests/native64-string-library.c').read_text().replace(' LIBRARY_HANDLERS',original[start:end]+handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);
 if not __debug__:p.error('independent source checks require Python assertions')
 p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='string-library-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  # Removing the numeric result bounds proof must make a malformed record observable.
  for parent in ('native64-typed-module.h','native64-typed-alt.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
  header=(ROOT/'include/native64-typed-code.h').read_text();before=' || s->d!=(s->returned<=147?32+4*(s->returned-145):32)';assert header.count(before)==1
  (w/'native64-typed-code.h').write_text(header.replace(before,''));subprocess.run(command,check=True)
  result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'malformed String metadata refused'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS String active control: disabled numeric result bounds detected',flush=True)

if __name__=='__main__':main()
