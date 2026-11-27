#!/usr/bin/env python3
"""Execute layout-based mixed arrays, including yacc-style nested value/state."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-word-arrays.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
SHAPES=((1,1,False),(1,2,False),(1,2,True),(2,1,False),(2,2,False),(2,2,True),(3,1,False),(3,2,False),(3,2,True))
TYPES='Box:adt{v:int;};'
for k,(roots,words,nested) in enumerate(SHAPES):
 fields='a:ref Box;'+('b:string;'if roots>=2 else'')+('c:chan of int;'if roots==3 else'')+'op:int;'
 TYPES+=('Value'+str(k)+':adt{'+fields+'};'if nested else'')+'Row'+str(k)+':adt{'+('value:Value'+str(k)+';state:int;'if nested else fields+('state:int;'if words==2 else''))+'};'
NAMES=tuple(name+str(k)for k in range(9)for name in('run','get'))
DECL='Mixedarrays:module{'+TYPES+''.join(name+':fn(n,i:int,x:ref Box,s:string,c:chan of int):'+('ref Box'if name.startswith('get')else'int')+';'for name in NAMES+('pairclear',))+'};'
SOURCE='implement Mixedarrays;'+DECL+'fail:int;'
for k in range(9):SOURCE+='saved'+str(k)+':array of Row'+str(k)+';outa'+str(k)+':ref Box;outb'+str(k)+':string;outc'+str(k)+':chan of int;'
for k,(roots,words,nested)in enumerate(SHAPES):
 tag=str(k);ty='Row'+tag;path='.value'if nested else'';field=path+'.a';last='.state'if words==2 else'.op'
 start='a:=array[n]of '+ty+';saved'+tag+'=a;if(fail)raise "mixedfault";v:'+('Value'+tag if nested else ty)+';v.a=x;'+('v.b=s;'if roots>=2 else'')+('v.c=c;'if roots==3 else'')+'v.op=17;'
 start+=('a[i].state=29;a[i].value=v;'if nested else('v.state=29;'if words==2 else'')+'a[i]=v;')
 start+='a[i]'+field+'=nil;a[i]'+field+'=x;v=a[i]'+path+';a[i]'+path+'=v;b:=array[n]of '+ty+';b[0:]=a;a[i]'+path+'=b[i]'+path+';b=(array[n+1]of '+ty+')[0:]=b;saved'+tag+'=b;'
 start+='outa'+tag+'=saved'+tag+'[i]'+field+';'+('outb'+tag+'=saved'+tag+'[i]'+path+'.b;'if roots>=2 else'')+('outc'+tag+'=saved'+tag+'[i]'+path+'.c;'if roots==3 else'')
 for name in('run','get'):
  SOURCE+=name+tag+'(n,i:int,x:ref Box,s:string,c:chan of int):'+('int'if name=='run'else'ref Box')+'{'+start+'return saved'+tag+'[i]'+(last if name=='run'else field)+';}'
SOURCE+='''
pairs:array of (ref Box,string);
pairclear(n,i:int,x:ref Box,s:string,nil:chan of int):int{
    a:=array[n] of (ref Box,string);
    a[i]=(x,s);
    a[i-1]=(x,s);
    a[i]=(nil,nil);
    pairs=a;
    return len a;
}
'''

def generate(limbo,w):
 path=w/'mixedarrays.m';path.write_text(DECL)
 header=subprocess.check_output([str(limbo),'-H','-M','Mixedarrays',str(path)],text=True);(w/'mixed-native.h').write_text(header)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Mixedarrays_(\w+) (0x[0-9a-f]+)U',header,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Mixedarrays',(19,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';text=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={v[0]:v[1]for v in rows[0]['globals']}
 text+='\nstatic const uint mixed_globals[][4]={'+','.join('{'+','.join(str(gl.get(n+str(k),'~0U'))for n in('saved','outa','outb','outc'))+'}'for k in range(9))+'};\nstatic const uint mixed_fail='+str(gl['fail'])+';\n'
 text+='static const uint mixed_functions[][2]={'+','.join('{'+','.join(str(next(i for i,r in enumerate(rows)if r['name']==n+str(k)))for n in('run','get'))+'}'for k in range(9))+'};\n'
 pair=next(i for i,r in enumerate(rows)if r['name']=='pairclear')
 assert sum(step[0]==E['OPCODES']['IMOVMP']and step[5]==65 and step[3]==8 for step in rows[pair]['code'])==3
 text+=f'static const uint mixed_pair_function={pair}, mixed_pairs_offset={gl["pairs"]};\n'
 # Independent C sizeof/offsetof witnesses, including padding in nested values.
 text+='struct MixedLayout{uint roots,words,width,first,last,copy;};\nstatic const struct MixedLayout mixed_layouts[]={\n'
 for k,(roots,words,nested)in enumerate(SHAPES):
  ty='Mixedarrays_Row'+str(k);path='value.'if nested else''
  text+='{'+str(roots)+','+str(words)+',sizeof('+ty+'),offsetof('+ty+','+path+'op),offsetof('+ty+','+('state'if words==2 else'op')+'),sizeof('+('Mixedarrays_Value'+str(k)if nested else ty)+')},\n'
 text+='};\n';p.write_text(text)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);controls=0
 for row in records:
  for pc,code in enumerate(row['code']):
   if code[5]not in(65,79,80):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,[],rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('mixed array oracle missed',row['name'],pc,k))
 print('PASS mixed arrays independent oracle:',controls,'mutations; unchanged classic bytes/descriptors and generated C geometry',flush=True)

def boundaries(limbo,w):
 for ty in('Record:adt{n:int;s:string;};','Record:adt{s:string;n:int;t:string;};','Record:adt{s:string;n:big;};','Record:adt{s:string;a,b,c:int;};'):
  _,rows=E['compile_source'](limbo,w,'implement Closed;Closed:module{run:fn(n:int);};'+ty+'a:array of Record;run(n:int){a=array[n]of Record;}',interface='',loads=True)
  assert not rows[0]['accepted'],rows
 for ty in('string','list of string','array of byte','ref Box','chan of int','Probe'):
  source='implement Probe;Probe:module{Box:adt{v:int;};Record:adt{value:'+ty+';state:int;};run:fn(n:int);};a:array of Record;run(n:int){a=array[n]of Record;}'
  data,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert rows[0]['accepted'],rows;E['verify'](data,rows[0],[],loads=True,rows=rows)
 print('PASS layout boundaries: all six root kinds accepted; interleaved roots, big and excess Words remain closed',flush=True)

def shstack(limbo,w):
 sh=(ROOT/'appl/cmd/sh/sh.b').read_text()
 declarations=''.join(re.search(r'(?m)^'+name+r':\s*adt\s*\{.*?^\};',sh,re.S).group(0)for name in('YYSTYPE','YYS'))
 source='implement Stack;include "sys.m";include "draw.m";include "sh.m";Node:type Sh->Node;Redir:type Sh->Redir;'+declarations+'Stack:module{run:fn(n,i:int):int;};run(n,i:int):int{yys:=array[n]of YYS;yyval:YYSTYPE;yys[i].yys=29;yys[i].yyv=yyval;yys=(array[len yys*2]of YYS)[0:]=yys;yyval=yys[i].yyv;return yys[i].yys;}'
 data,rows=E['compile_source'](limbo,w,source,interface='',loads=True);row=rows[0];assert row['accepted'],rows;E['verify'](data,row,[],loads=True,rows=rows)
 assert any(c[5]==79 and c[0]==E['OPCODES']['INEWAZ']and c[3]==61 for c in row['code'])
 assert any(c[5]==65 and c[0]==E['OPCODES']['IMOVMP']and c[3]&65535==28 for c in row['code'])
 frontier=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](limbo,'appl/cmd/sh/sh.b',w)
 yy=next(f for f in frontier['functions']if f['name']=='yyparse');assert yy['accepted']or yy['pc']>yy['first'],yy
 assert all(f['accepted']for f in frontier['functions']if f['name'] in ('removebuiltin','removesbuiltin'))
 print('PASS unchanged Sh declarations: YYS allocation, nested YYSTYPE copies and yacc-style growth; full yyparse frontier',yy.get('pc'),yy.get('reason'),flush=True)

def unit():
 src=BASE['unit']().split('\n#include "full.h"')[0]
 at=src.index(' optab[IMOVW]=countmovw;');handlers=src[at:src.index(' require(prepare64calls(',at)]
 return src+'\n#line 1 "native64-mixed-arrays.c"\n'+(ROOT/'utils/tests/native64-mixed-arrays.c').read_text().replace('MIXEDARRAY_HANDLERS',handlers)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args();limbo=a.limbo.resolve()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='mixed-arrays-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](limbo,w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(limbo,w);boundaries(limbo,w);shstack(limbo,w);(w/'unit.c').write_text(unit())
  arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)]
  subprocess.run(command,check=True);subprocess.run(runner,check=True,timeout=180)
  parents=('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-raw-array-runtime.h','native64-raw-array-module.h','native64-raw-array-code.h','native64-array-layout.h','native64-typed-tables.h','native64-string-tables.h')
  for name,header,before,after,predicate in (
   ('mapped element Type','native64-typed-module.h','uchar map=native64arraymap(i);rawtypes[i]=ordinal;','uchar map=0;rawtypes[i]=ordinal;','exact mixed element Type'),
   ('padding overwrite','native64-array-layout.h','if(!root&&native64arrayword(copy,i)!=native64arrayword(owner,off+i))return 0;','','copy padding preserves active state'),
   ('nested copy Type','native64-typed-module.h','m->prog[b->offsets[i]+j].reg=rawtypes[f->code[j].m&65535];','m->prog[b->offsets[i]+j].reg=rawtypes[61];','nested mapped copy descriptor')):
   for parent in parents:(w/parent).write_text((ROOT/'include'/parent).read_text())
   path=w/header;text=path.read_text();assert text.count(before)==1;path.write_text(text.replace(before,after,1));subprocess.run(command,check=True)
   result=subprocess.run(runner,capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in parents:(w/parent).unlink()
   print('PASS mixed array active control:',name,flush=True)
if __name__=='__main__':main()
