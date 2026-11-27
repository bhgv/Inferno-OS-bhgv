#!/usr/bin/env python3
"""Execute mapped native64 Image arrays with real allocation/indexing/movers."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
NAMES=('create','direct','mp','replace','clear','alias','renew','rawbytes')
DECL='Images:module{'+''.join(n+':fn(n,i:int,x:ref Draw->Image);'for n in NAMES)+'};'
SOURCE='implement Images;'+PREFIX+DECL+'saved:array of ref Draw->Image;out,old:ref Draw->Image;count,fail:int;raw:array of byte;'
for k,name in enumerate(NAMES):
 if name=='rawbytes':
  SOURCE+=name+'(n,i:int,x:ref Draw->Image){a:=array[n] of byte;raw=a;if(fail)raise "fail:array";count=len raw;}';continue
 body='a:=array[n] of ref Draw->Image;saved=a;if(fail)raise "fail:array";'
 if k:
  owner='saved'if k>=2 else'a'
  body+=owner+'[i]=x;'
  if name=='replace':body+=owner+'[i]=old;'
  if name=='clear':body+=owner+'[i]=nil;'
  if name=='alias':body+='v:='+owner+'[i];'+owner+'[i]=v;'
  if name=='renew':body+='saved=array[n] of ref Draw->Image;'
  body+='out='+owner+'[i];'
 body+='count=len saved;'
 SOURCE+=name+'(n,i:int,x:ref Draw->Image){'+body+'}'
def generate(limbo,w):
 m=w/'images.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Images','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Images_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Images',(8,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint array_globals[]={'+','.join(str(gl[n])for n in('saved','out','old','count','fail'))+'};\n')
 (w/'draw-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(254,255):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,[],rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Image array oracle missed',row['name'],pc,k))
 print('PASS Image array oracle:',controls,'corruptions; classic element descriptor and unchanged Dis',flush=True)
 for ty,extra in(('int',''),('ref Draw->Pointer',''),('ref Other','Other:adt{r:Draw->Rect;};')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(n:int);};saved:array of '+ty+';run(n:int){saved=array[n] of '+ty+';}'
  raw,closed=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert closed[0]['accepted'],closed
  E['verify'](raw,closed[0],[],loads=True,rows=closed)
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest','sizewin','showborders'):
  row=next(r for r in records if r['name']==name);print('AWM',name,':',row['accepted'],row.get('pc'),row.get('reason'),flush=True)
 row=next(r for r in records if r['name']=='sizewin');assert row['accepted']or row['pc']>901
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];heap=(ROOT/'libinterp/heap.c').read_text();xec=(ROOT/'libinterp/xec.c').read_text()
 for sig in('Heap*heap(','void\nacheck(','Heap*\nheaparray(','void\ninitarray(','void\nfreearray('):src=src.replace(extract(src,sig),'')
 src+='\nvoid initarray(Type*,Array*);\n'
 for sig in('Heap*\nheap(','void\nacheck(','Heap*\nheaparray(','void\ninitarray(','void\nfreearray('):src+='\n'+extract(heap,sig)+'\n'
 for sig in('static void\nnewarray(','OP(newaz)','static uchar*\narrayaddress(','static void\narrayindex(','OP(indl)','OP(indw)','OP(lena)'):src+='\n'+extract(xec,sig)+'\n'
 return src+'\n#line 1 "native64-image-arrays.c"\n'+(ROOT/'utils/tests/native64-image-arrays.c').read_text().replace(' ARRAY_HANDLERS',BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0][start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='image-arrays-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('missing mapped element Type','native64-typed-module.h','uchar refmap[]={0x80};refarraytype=ordinal;', 'uchar refmap[]={0};refarraytype=ordinal;','exact mapped 8-byte array Type'),
   ('classic 4-byte index stride','native64-typed-code.h','if(s->returned==255){if(s->op==IINDL || s->op==IINDX){p->s.ind=s->s;', 'if(s->returned==255){if(s->op==IINDL || s->op==IINDX){if(s->op==IINDL)p->op=IINDW;p->s.ind=s->s;','exact 8-byte element stride'),
   ('branch into borrowed mover','native64-typed-module.h','        for(j=0;j<f->count;j++)if(native64typedbranch(f->code[j].op) && f->code[j].d>=0 && (unsigned long)f->code[j].d<f->count && native64typedprotectedentry(f->code,f->count,f->code[j].d))return 0;', '', 'array refusal before allocation'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-tables.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-tables.h'):(w/parent).unlink()
   print('PASS Image array active control:',name,flush=True)

if __name__=='__main__':main()
