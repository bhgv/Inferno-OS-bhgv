#!/usr/bin/env python3
"""Execute arrays of all six managed-reference kinds through native64 C/JIT."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-image-arrays.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
KINDS=('string','list of string','array of byte','ref Box','chan of int','Rootarrays')
PREFIX='Box:adt{v:int;};'
DECL='Rootarrays:module{'+''.join(name+str(k)+':fn(n,i:int,x:'+ty+'):'+('array of '+ty if name=='slice'else ty)+';'for k,ty in enumerate(KINDS)for name in('run','slice'))+'};'
SOURCE='implement Rootarrays;'+PREFIX+DECL+'fail:int;'
for k,ty in enumerate(KINDS):
 SOURCE+='saved'+str(k)+':array of '+ty+';old'+str(k)+',out'+str(k)+':'+ty+';'
for k,ty in enumerate(KINDS):
 tag=str(k);start='a:=array[n]of '+ty+';saved'+tag+'=a;if(fail)raise "arrayfault";a[i]=nil;a[i]=x;'
 SOURCE+='run'+tag+'(n,i:int,x:'+ty+'):'+ty+'{'+start+'a[i]=old'+tag+';a[i]=x;b:=array[n]of '+ty+';b[0:]=a;v:=b[i];b[i]=v;saved'+tag+'=b;if(b[i]==nil)out'+tag+'=nil;else out'+tag+'=b[i];return saved'+tag+'[i];}'
 SOURCE+='slice'+tag+'(n,i:int,x:'+ty+'):array of '+ty+'{'+start+'return saved'+tag+'[i:];}'

def generate(limbo,w):
 path=w/'rootarrays.m';path.write_text(PREFIX+DECL)
 header=subprocess.check_output([str(limbo),'-H','-M','Rootarrays',str(path)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Rootarrays_(\w+) (0x[0-9a-f]+)U',header,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Rootarrays',(12,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';text=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={v[0]:v[1]for v in rows[0]['globals']}
 text+='\nstatic const uint rootarray_globals[][3]={'+','.join('{'+','.join(str(gl[n+str(k)])for n in('saved','old','out'))+'}'for k in range(6))+'};\nstatic const uint rootarray_fail='+str(gl['fail'])+';\n'
 text+='static const uint rootarray_functions[][2]={'+','.join('{'+','.join(str(next(i for i,r in enumerate(rows)if r['name']==n+str(k)))for n in('run','slice'))+'}'for k in range(6))+'};\n';p.write_text(text)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);controls=0
 for row in records:
  for pc,code in enumerate(row['code']):
   if code[5]not in(254,255,111):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,[],rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('missed managed array proof mutation',row['name'],pc,k))
 print('PASS managed arrays: six element kinds, exact classic descriptors/bytes and',controls,'oracle mutations',flush=True)
 frontier=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](limbo,'appl/cmd/sh/sh.b',w)
 for n in('yytokname','yystatname'):assert next(f for f in frontier['functions']if f['name']==n)['accepted']
 print('PASS unchanged Sh parser-name functions: global String array reads and direct element results',flush=True)

def boundaries(limbo,w):
 for ty in ('big','real','Record'):
  source='implement Closed;Record:adt{n:int;s:string;};Closed:module{run:fn(n:int);};saved:array of '+ty+';run(n:int){saved=array[n]of '+ty+';}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert not rows[0]['accepted'],rows
 for ty in ('ref Draw->Image','ref Draw->Pointer','chan of array of string'):
  source='implement Probe;include "sys.m";include "draw.m";Probe:module{get:fn(a:array of '+ty+',i:int):'+ty+';};get(a:array of '+ty+',i:int):'+ty+'{if(a[i]!=nil)return a[i];return nil;}'
  data,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert rows[0]['accepted'],rows
  CMD['verify_nilbranches'](data,rows[0],[],rows=rows)
 print('PASS managed array boundaries: big/real/interleaved-record allocation remains closed; nominal Image/Pointer and nested channels use common root indexing',flush=True)

def unit():
 src=BASE['unit']().split('\n#line 1 "native64-image-arrays.c"')[0]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text()
 for name in('static uchar*\narrayspan(','OP(slicea)','OP(slicela)','static void\nbranchstring(','OP(beqc)','OP(bnec)'):src+='\n'+extract(vm,name)
 at=src.index(' optab[IMOVW]=countmovw;');handlers=src[at:src.index(' require(prepare64calls(',at)]
 return src+'\n'+(ROOT/'utils/tests/native64-root-arrays.c').read_text().replace('ROOTARRAY_HANDLERS',handlers)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args();limbo=a.limbo.resolve()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='root-arrays-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](limbo,w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(limbo,w);boundaries(limbo,w);(w/'unit.c').write_text(unit())
  arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)]
  subprocess.run(command,check=True);subprocess.run(runner,check=True,timeout=180)
  parents=('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-tables.h')
  for name,header,before,after,predicate in (
   ('mapped element Type','native64-typed-module.h','uchar refmap[]={0x80};refarraytype=ordinal;','uchar refmap[]={0};refarraytype=ordinal;','mapped pointer-sized element Type'),
   ('allocated-empty String comparison','native64-typed-code.h','if(s->returned==111){if(s->op==IINDL){','if(s->returned==111){if(s->op==IBEQC)p->op=IBEQL;if(s->op==IBNEC)p->op=IBNEL;if(s->op==IINDL){','nil and allocated-empty String comparison semantics'),
   ('unproved slice copy','native64-typed-module.h','        for(j=0;j<f->count;j++)if(f->code[j].op==ISLICELA && f->code[j].returned!=79 && f->code[j].returned!=111 && f->code[j].returned!=153 && f->code[j].returned!=159)return 0;','','array metadata refused before allocation')):
   for parent in parents:(w/parent).write_text((ROOT/'include'/parent).read_text())
   path=w/header;text=path.read_text();assert text.count(before)==1;path.write_text(text.replace(before,after,1))
   subprocess.run(command,check=True);result=subprocess.run(runner,capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in parents:(w/parent).unlink()
   print('PASS managed array active control:',name,flush=True)
if __name__=='__main__':main()
