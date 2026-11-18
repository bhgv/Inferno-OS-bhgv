#!/usr/bin/env python3
"""Execute checked primitive channel SEND/RECV through real scheduler queues."""
import argparse,copy,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SP=runpy.run_path(str(ROOT/'utils/tests/native64-spawn.py'));G=SP['G'];CMD=SP['CMD'];E=SP['E']
KINDS=('string','int','byte','big','real')
NAMES=('run','reverse','burst','nilvalue','word','bytevalue','bigvalue','realvalue')
DECL='Flow:module{'+''.join(n+':fn(c:chan of '+('string'if i<4 else KINDS[i-3])+',v:'+('string'if i<4 else KINDS[i-3])+');'for i,n in enumerate(NAMES))+'mp:fn(c:chan of string,v:string);clear:fn();};'
SOURCE='implement Flow;'+DECL+'count,wordout:int;byteout:byte;bigout:big;realout:real;saved,sending:string;storedchan:chan of string;'
for i,n in enumerate(NAMES):
 kind='string'if i<4 else KINDS[i-3];suffix=''if i<4 else str(i)
 order=('reader'+suffix+'(c);spawn sender'+suffix+'(c,v);')if i==1 else ('sender(c,v);spawn sender(c,v);spawn reader(c);spawn reader(c);')if i==2 else ('sender(c,nil);spawn reader(c);')if i==3 else ('sender'+suffix+'(c,v);spawn reader'+suffix+'(c);')
 SOURCE+=n+'(c:chan of '+kind+',v:'+kind+'){spawn '+order+'}'
for i,kind in enumerate(KINDS):
 suffix=''if not i else str(i+3);out=('saved','wordout','byteout','bigout','realout')[i]
 SOURCE+='sender'+suffix+'(c:chan of '+kind+',v:'+kind+'){c <-= v;}reader'+suffix+'(c:chan of '+kind+'){'+('x:= <-c;'+out+'=x;'if i in(0,3)else out+'= <-c;')+'count++;}'
SOURCE+='mp(c:chan of string,v:string){sending=v;storedchan=c;spawn sendermp();spawn readermp();}sendermp(){storedchan <-=sending;}readermp(){saved= <-storedchan;count++;}clear(){saved=nil;}'
def generate(limbo,w):
 rows=G['generate'](limbo,w,SOURCE,'Flow',(22,0,0,'~0U'));p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));globals={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:out.write('static const uint transfer_globals[]={'+','.join(str(globals[n])for n in('count','wordout','byteout','bigout','realout','saved','sending','storedchan'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]!=188:continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('transfer oracle missed',r['name'],pc,k))
 print('PASS checked transfer source:',controls,'corruptions detected; independent classic operands, root classes and width contracts',flush=True)
def boundaries(limbo,w):
 for payload in('ref Item','array of byte','list of string','chan of int','Service'):
  source='implement Probe;Item:adt{n:int;};Service:module{run:fn();};Probe:module{run:fn(c:chan of '+payload+',v:'+payload+');};run(c:chan of '+payload+',v:'+payload+'){c <-=v;x:= <-c;}'
  data,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert all(r['accepted']for r in rows),rows
  imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
  for r in rows:E['verify'](data,r,imports,loads=True,rows=rows)
 for payload in('(string,string)','(int,int)'):
  source='implement Closed;Closed:module{run:fn(c:chan of '+payload+',v:'+payload+');};run(c:chan of '+payload+',v:'+payload+'){c <-=v;x:= <-c;}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 source='implement Closed;Closed:module{run:fn(c:chan of string):string;};run(c:chan of string):string{return <-c;}'
 _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 print('PASS transfer boundary: six managed root classes accepted; aggregate payloads and borrowed receive result remain closed; classic bytes unchanged',flush=True)
def unit():
 src=SP['unit']().split('\n#line 1 "native64-spawn.c"')[0];extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();heap=(ROOT/'libinterp/heap.c').read_text()
 src=src.replace('#include "tests/leaf-cons-stub.h"','#define cchanleaf unused_transfer_leaf\n#define cnewcscalar unused_transfer_allocator\n#include "tests/leaf-cons-stub.h"\n#undef cchanleaf\n#undef cnewcscalar')
 for signature in('Heap*\nheaparray(','void\ninitarray('):src=src.replace(extract(src,signature),extract(heap,signature),1)
 src+='\n'+extract(heap,'void\nfreearray(')+'\nType Tword={1,0,0,4},Tlong={1,0,0,8},Treal={1,0,0,8};\n'+extract(vm,'OP(movb)')+'\n'
 src+='\nvoid actual_isend(void);void actual_irecv(void);\n#define movm actual_movm\n#define movp actual_movp\n#define movw actual_movw\n#define movb actual_movb\n#define movl actual_movl\n#define movf actual_movf\n#define isend actual_isend\n#define irecv actual_irecv\n#include "vmchannel.h"\n'
 src+='\nvoid altdone(Alt*a,Prog*p,Channel*c,int r){USED(a);USED(p);USED(c);USED(r);require(0);}\n'
 src+='\n'+vm[vm.index('typedef struct ChannelBuffer'):vm.index('/*\nint\ncqsize(',vm.index('typedef struct ChannelBuffer'))]
 for signature in('void\ncqadd(','void\ncqdel(','void\ncqdelp(','OP(isend)','OP(irecv)','Channel*\ncnewcscalar('):src+='\n'+extract(vm,signature)+'\n'
 before='int\ncchanleaf(REG *r, int receive, void *transaction)';assert src.count(before)==1;src=src.replace(before,'static uint transferfast,transferfallback;\nstatic int\nproduction_cchanleaf(REG *r, int receive, void *transaction)',1)
 src+='\nint cchanleaf(REG*r,int receive,void*transaction){int ok=production_cchanleaf(r,receive,transaction);if(ok)transferfast++;else transferfallback++;return ok;}\n'
 src+='\n#undef movm\n#undef movp\n#undef movw\n#undef movb\n#undef movl\n#undef movf\n#undef isend\n#undef irecv\n'
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-channel-transfer.c"\n'+(ROOT/'utils/tests/native64-channel-transfer.c').read_text().replace(' TRANSFER_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='channel-transfer-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('payload-offset','native64-typed-code.h','if(s->returned==188){p->reg=0;p->d.ind=s->d;continue;}','if(s->returned==188){p->reg=0;p->d.ind=s->d;if(s->m==4){if(s->op==ISEND)p->s.ind+=4;else p->d.ind+=4;}continue;}','exact payload, count and adjacent MP bytes'),
   ('mapped-value','native64-typed-module.h','if(!!(map[bit/8]&(128>>(bit%8)))!=((step->op==ISEND?k==1:k==0)||step->m==24))return 0;','if(0)return 0;','malformed transfer metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS checked transfer active control:',name,flush=True)
if __name__=='__main__':main()
