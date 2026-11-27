#!/usr/bin/env python3
"""Execute initialized MP Word32 arrays and proved FP/MP raw-owner lifetimes."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-image-arrays.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
NAMES=('read','sparseread','emptyread','write','renew','slice','copy','compare','aftercall','noop','clear','record','delta')
DECL='Wordtables:module{'+''.join(n+':fn(i,n:int):int;'for n in NAMES)+'};'
SOURCE='''implement Wordtables;include "sys.m";include "draw.m";
Winplace:module{Delta:adt{d,wid,coord:int;};};
'''+DECL+'''
table:=array[]of{-2147483647-1,17,0,2147483647,-31};
sparse:=array[7]of{2 => 11,5 => -7,* => 0};
empty:=array[0]of{* => 0};
view:array of int;rects:array of Draw->Rect;deltas:array of Winplace->Delta;
read(i,n:int):int{return table[i];}
sparseread(i,n:int):int{return sparse[i];}
emptyread(i,n:int):int{return empty[i];}
write(i,n:int):int{table[i]=n;return table[i];}
renew(i,n:int):int{table=array[n]of int;table[i]=n;return table[i];}
slice(i,n:int):int{view=table[i:n];return len view;}
copy(i,n:int):int{a:=array[n]of int;a[0:]=table;view=a;return view[i];}
compare(i,n:int):int{if(table[i]<n)return 1;return 0;}
aftercall(i,n:int):int{x:=table[i];n=noop(i,n);return x;}
noop(i,n:int):int{return n;}
clear(i,n:int):int{table=nil;return n;}
record(i,n:int):int{rects=array[n]of Draw->Rect;rects[i].max.x=n;return rects[i].max.x;}
delta(i,n:int):int{deltas=array[n]of Winplace->Delta;deltas[i].coord=n;return deltas[i].coord;}
'''

def generate(limbo,w):
 interface=w/'wordtables.m';interface.write_text(DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Wordtables',str(interface)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Wordtables_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Wordtables',(len(NAMES),0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={v[0]:v[1]for v in rows[0]['globals']}
 s+='\nstatic const uint word_globals[]={'+','.join(str(gl[n])for n in('table','sparse','empty','view','rects','deltas'))+'};\n'
 s+='static const uint word_functions[]={'+','.join(str(next(i for i,r in enumerate(rows)if r['name']==n))for n in NAMES)+'};\n';p.write_text(s)
 data,proofs=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);controls=0
 for r in proofs:
  for pc,c in enumerate(r['code']):
   if c[5]not in(65,79,80):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,[],rows=proofs)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Word array oracle missed',r['name'],pc,k))
 print('PASS Word arrays independent oracle:',controls,'mutations; initialized dense/sparse/empty arrays and exact classic bytes',flush=True)

def unit():
 src=BASE['unit']().split('\n#line 1 "native64-image-arrays.c"')[0]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text()
 for name in('static uchar*\narrayspan(','OP(slicea)','OP(slicela)','OP(indx)'):src+='\n'+extract(vm,name)
 heap=(ROOT/'libinterp/heap.c').read_text()
 src=src.replace(extract(src,'Array*\narraycpy('),'void* arraycpy(Array*);',1)
 src+='\ntypedef struct ArrayCopy ArrayCopy;\n'+re.search(r'struct ArrayCopy \{.*?\n};',heap,re.S)[0]+'\n'+extract(heap,'static int\ncopyarray(')+'\n'+extract(heap,'void*\narraycpy(')+'\n'
 at=src.index(' optab[IMOVW]=countmovw;');handlers=src[at:src.index(' require(prepare64calls(',at)]
 return src+'\n'+(ROOT/'utils/tests/native64-word-arrays.c').read_text().replace('WORDARRAY_HANDLERS',handlers)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args();limbo=a.limbo.resolve()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='word-arrays-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](limbo,w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(limbo,w);(w/'unit.c').write_text(unit())
  arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)]
  subprocess.run(command,check=True);subprocess.run(runner,check=True,timeout=180)
  parents=('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-raw-array-runtime.h','native64-raw-array-module.h','native64-raw-array-code.h','native64-typed-tables.h','native64-string-tables.h')
  for name,header,before,after,predicate in (
   ('initializer bits','native64-typed-module.h','for(j=0;j<v->count;j++)memmove(a->data+4*j,v->values+j,4);','for(j=0;j<v->count;j++){UWORD zero=0;memmove(a->data+4*j,&zero,4);}','exact dense and sparse initializer bits'),
   ('FP/MP owner distinction','native64-raw-array-module.h','&&((flow[k]>>31)==(mode==AMP))','','FP/MP owner identity kept distinct'),
   ('global call invalidation','native64-raw-array-module.h','if(v->op==ICALL||v->op==IMCALL||v->op==ISPAWN)for(k=0;k<2;k++)if(flow[k]&0x80000000U)flow[k]=0;','','global borrow invalidated by call')):
   for parent in parents:(w/parent).write_text((ROOT/'include'/parent).read_text())
   path=w/header;text=path.read_text();assert text.count(before)==1;path.write_text(text.replace(before,after,1))
   subprocess.run(command,check=True);result=subprocess.run(runner,capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in parents:(w/parent).unlink()
   print('PASS Word array active control:',name,flush=True)
if __name__=='__main__':main()
