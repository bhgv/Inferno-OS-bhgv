#!/usr/bin/env python3
"""Execute all 13 unchanged Wmsrv queue methods in native64 C/JIT."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BUF=runpy.run_path(str(ROOT/'utils/tests/native64-bufio-library.py'));G,CMD,E=BUF['G'],BUF['CMD'],BUF['E']
ORIGINAL=(ROOT/'appl/lib/wmsrv.b').read_text()
DECL='Queues:module{init:fn();iput:fn(n:int);iget:fn():int;ipeek:fn():int;sput:fn(s:string);sget:fn():string;speek:fn():string;pput:fn(buttons,x,y,msec:int);pget:fn():ref Draw->Pointer;ppeek:fn():ref Draw->Pointer;nonempty:fn(which:int):int;flush:fn();};'
BODIES=ORIGINAL[ORIGINAL.index('Iqueue.put('):]
SOURCE='implement Queues;include "sys.m";include "draw.m";draw:Draw;Pointer,Point:import draw;'+DECL+ORIGINAL[ORIGINAL.index('Iqueue: adt'):ORIGINAL.index('\ninit():')]+BODIES+'''
iq:ref Iqueue;sq:ref Squeue;pq:ref Ptrqueue;
init(){iq=ref Iqueue;sq=ref Squeue;pq=ref Ptrqueue;}
iput(n:int){iq.put(n);}iget():int{return iq.get();}ipeek():int{return iq.peek();}
sput(s:string){sq.put(s);}sget():string{return sq.get();}speek():string{return sq.peek();}
pput(buttons,x,y,msec:int){s:=ref Pointer(buttons,Point(x,y),msec);if(s==nil)return;pq.put(s);}
pget():ref Pointer{return pq.get();}ppeek():ref Pointer{return pq.peek();}
nonempty(which:int):int{if(which==0)return iq.nonempty();if(which==1)return sq.nonempty();return pq.nonempty();}
flush(){pq.flush();}
'''
OP_NAMES=set()
def generate(limbo,w):
 m=w/'queues.m';m.write_text('include "sys.m";include "draw.m";'+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Queues','-I',str(ROOT/'module'),str(m)],text=True);signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Queues_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Queues',(25,0,0,'~0U'),export_signatures=signatures)
 global OP_NAMES
 OP_NAMES={name[1:].lower()for name,op in E['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
 (w/'queue-layout.h').write_text('typedef struct Draw_Point Draw_Point;typedef struct Draw_Pointer Draw_Pointer;\n'+''.join(re.search(r'struct '+n+r' \{.*?\n};',h,re.S)[0]+'\n'for n in('Draw_Point','Draw_Pointer')))
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');s+='\nstruct LibraryFunction {uint fn,argc,kinds[4],offsets[4],result,contract;};\nstatic const struct LibraryFunction library_functions[]={\n'
 for i,r in enumerate(rows):
  if r['export_name']not in signatures:continue
  kind=r['result'][0];s+='{'+f'{i},{len(r["args"])}'+',{'+','.join(str(a[1])for a in r['args'])+'},{'+','.join(str(a[0])for a in r['args'])+'},'+f'{kind},{r["result"][1]|(16 if kind in E["ROOTS"]else 0)}'+'},\n'
 s+='};\n';p.write_text(s)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(138,139,153,154,213):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=100 if c[5]==154 and k==2 else 1
    try:CMD['verify_nilbranches'](data,bad,[],rows=records)
    except(AssertionError,ValueError,KeyError,IndexError,StopIteration):controls+=1
    else:raise AssertionError(('queue oracle missed',row['export_name'],pc,k))
 # Verify the very same methods inside the complete, unchanged Wmsrv source.
 data,full=E['compile_source'](limbo,w,ORIGINAL,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 selected=[r for r in full if r.get('export_name','').split('.')[0]in('Iqueue','Squeue','Ptrqueue')];assert len(selected)==13 and all(r['accepted']for r in selected)
 for r in selected:CMD['verify_nilbranches'](data,r,imports,rows=full)
 print('PASS all 13 original Wmsrv methods in complete source and runtime cluster:',controls,'independent mutations; classic bytes unchanged',flush=True)
def unit():
 BUF['unit'].__globals__['OP_NAMES']=OP_NAMES
 original=BUF['unit']();prefix=original.split('\n#line 1 "native64-bufio-library.c"')[0]
 start=original.rindex(' optab[IMOVW]=countmovw;');end=original.index(' metadata();construction();',start)
 return prefix+'\n#line 1 "native64-wmsrv-queues.c"\n'+(ROOT/'utils/tests/native64-wmsrv-queues.c').read_text().replace(' LIBRARY_HANDLERS',original[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 if not __debug__:p.error('independent source checks require assertions')
 with tempfile.TemporaryDirectory(prefix='wm-queues-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
  h=w/'native64-typed-code.h';s=h.read_text();before='p->reg=16;p->d.i.f=s->d;p->d.i.s=0;';assert s.count(before)==1;h.write_text(s.replace(before,'p->reg=8;p->d.i.f=s->d;p->d.i.s=0;'))
  subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'coalesced Pointer identity and four Word32 cells'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS queue active control: truncated Pointer copy detected',flush=True)

if __name__=='__main__':main()
