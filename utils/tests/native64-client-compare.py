#!/usr/bin/env python3
"""Execute canonical Wmsrv.Client equality with full-width roots."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
NAMES=tuple('f'+str(i)for i in range(8))+('probeeq','probene','calleq','callne')
DECL='Clientcompare:module{'+''.join(n+':fn(a,b:ref Wmsrv->Client);'for n in NAMES)+'};'
SOURCE='implement Clientcompare;'+PREFIX+DECL+'focus,other:ref Wmsrv->Client;out:int;'
for i,n in enumerate(NAMES[:8]):
 left,right=(('a','b'),('a','other'),('focus','b'),('focus','other'))[i//2]
 SOURCE+=n+'(a,b:ref Wmsrv->Client){focus=a;other=b;if('+left+('=='if i%2==0 else'!=')+right+')out=1;else out=2;}'
SOURCE+='probeeq(a,b:ref Wmsrv->Client){if(focus==other)out=1;else out=2;}probene(a,b:ref Wmsrv->Client){if(focus!=other)out=1;else out=2;}'
for op in('eq','ne'):
 SOURCE+='call'+op+'(a,b:ref Wmsrv->Client){n:=work'+op+'(a,b);out=n;}work'+op+'(a,b:ref Wmsrv->Client):int{n:=0;if(a'+('=='if op=='eq'else'!=')+'b)n=1;else n=2;return n;}'
def generate(limbo,w):
 m=w/'clientcompare.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Clientcompare','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Clientcompare_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Clientcompare',(14,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint compare_globals[]={'+','.join(str(gl[n])for n in('focus','other','out'))+'};\nstatic const uint compare_functions[]={'+','.join(str(next(i for i,r in enumerate(rows)if r['name']==n))for n in NAMES)+'};\n')
 (w/'client-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;modes=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]!=231:continue
   modes.add(c[1])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Client compare oracle missed',row['name'],k))
 assert modes=={130,138,194},modes
 print('PASS Client compare oracle:',controls,'corruptions detected; FP/MP pairs, full mapped roots and unchanged classic bytes',flush=True)
 for ty,extra in(('Other','Other:adt{x:int;};'),('Draw->Image','')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(a,b:ref '+ty+');};out:int;run(a,b:ref '+ty+'){if(a==b)out=1;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 original=(ROOT/'module/wmsrv.m').read_text();assert 'flags:\tint;'in original;(w/'badwmsrv.m').write_text(original.replace('flags:\tint;','flags:\tbig;',1))
 _,closed=E['compile_source'](limbo,w,SOURCE.replace('include "wmsrv.m";','include "badwmsrv.m";'),interface='',loads=True);assert not any(r['accepted']for r in closed),closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in records if r['name']=='init');focus=next(r for r in records if r['name']=='setfocus');assert focus['accepted']or focus['pc']>1189,focus;assert next(r for r in records if r['name']=='reshaped')['accepted'];print('AWM init:',init.get('pc'),init.get('reason'),'; setfocus:',focus.get('pc'),focus.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-client-compare.c"\n'+(ROOT/'utils/tests/native64-client-compare.c').read_text().replace(' PARSE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='client-compare-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('truncated pointer compare','native64-typed-code.h','p->op=s->op;','p->op=s->op==IBEQL?IBEQW:s->op==IBNEL?IBNEW:s->op;','full-width Client comparison'),
   ('middle register truncation','native64-typed-code.h','s->returned==231 && s->m<=65535','s->returned==231','malformed Client comparison metadata refused'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==(3 if name=='truncated pointer compare'else 1);(w/header).write_text(text.replace(before,after));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Client compare active control:',name,flush=True)
if __name__=='__main__':main()
