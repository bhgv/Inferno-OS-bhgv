#!/usr/bin/env python3
"""Execute exact Wmsrv.Client MP nil guards with native pointer-width sentinels."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
NAMES=('direct','reverse','empty','probe')
DECL='Focusnil:module{'+''.join(n+':fn(c:ref Wmsrv->Client);'for n in NAMES)+'};'
SOURCE='implement Focusnil;'+PREFIX+DECL+'focus:ref Wmsrv->Client;out:int;'
SOURCE+='direct(c:ref Wmsrv->Client){focus=c;if(focus!=nil)out=1;else out=2;}reverse(c:ref Wmsrv->Client){focus=c;if(nil==focus)out=2;else out=1;}empty(c:ref Wmsrv->Client){focus=c;if(nil!=focus)out=1;else out=2;}probe(c:ref Wmsrv->Client){if(focus==nil)out=2;else out=1;}'
def generate(limbo,w):
 m=w/'focusnil.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Focusnil','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Focusnil_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Focusnil',(4,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint focus_globals[]={'+','.join(str(gl[n])for n in('focus','out'))+'};\n')
 (w/'client-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[0]not in(E['OPCODES']['IBEQL'],E['OPCODES']['IBNEL']):continue
   assert c[1]==194
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('MP Client nil oracle missed',row['name'],k))
 print('PASS MP Client nil oracle:',controls,'corruptions detected; canonical hash, two mapped roots, branch relocation and unchanged bytes',flush=True)
 for ty,extra in(('Other','Other:adt{x:int;};'),('Draw->Image','')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(c:ref '+ty+');};focus:ref '+ty+';out:int;run(c:ref '+ty+'){focus=c;if(focus!=nil)out=1;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 original=(ROOT/'module/wmsrv.m').read_text();assert 'flags:\tint;'in original;(w/'badwmsrv.m').write_text(original.replace('flags:\tint;','flags:\tbig;',1))
 _,closed=E['compile_source'](limbo,w,SOURCE.replace('include "wmsrv.m";','include "badwmsrv.m";'),interface='',loads=True);assert not any(r['accepted']for r in closed),closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in records if r['name']=='init');assert init['accepted']or init['pc']>189,init;print('AWM init frontier:',init.get('pc'),init.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-mp-client-nil.c"\n'+(ROOT/'utils/tests/native64-mp-client-nil.c').read_text().replace(' PARSE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='mp-client-nil-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
  before='p->op=s->op;';after='p->op=s->op==IBEQL?IBEQW:s->op==IBNEL?IBNEW:s->op;';text=(ROOT/'include/native64-typed-code.h').read_text();assert text.count(before)==3;(w/'native64-typed-code.h').write_text(text.replace(before,after));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'full-width MP nil comparison'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS MP Client nil active control: truncated pointer compare detected',flush=True)
if __name__=='__main__':main()
