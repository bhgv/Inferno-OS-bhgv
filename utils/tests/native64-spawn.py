#!/usr/bin/env python3
"""Execute checked local spawn with real scheduler publication and argument owners."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
TR=runpy.run_path(str(ROOT/'utils/tests/native64-channel-triples.py'));G=TR['G'];CMD=TR['CMD'];E=TR['E']
ARGS='n:int,s:string,c:chan of string'
LIST_ARGS='n:int,items:list of string,c:chan of string'
ARRAY_ARGS='n:int,items:array of ref Item,c:chan of string'
SOURCE='implement Workers;Workers:module{Item:adt{text:string;};launch:fn('+ARGS+');twice:fn('+ARGS+');chain:fn('+ARGS+');fresh:fn('+ARGS+');tailspawn:fn('+LIST_ARGS+');direct:fn('+LIST_ARGS+');compound:fn('+ARRAY_ARGS+');clear:fn();};count,total:int;saved:string;channel:chan of string;launch('+ARGS+'){spawn child(n,s,c);}twice('+ARGS+'){spawn child(n,s,c);spawn child(n,s,c);}chain('+ARGS+'){spawn relay(n,s,c);}fresh('+ARGS+'){t:=s+"!";spawn child(n,t,c);}tailspawn('+LIST_ARGS+'){spawn listchild(n,tl items,c);}listchild('+LIST_ARGS+'){child(n,hd items,c);}child('+ARGS+'){count++;total+=n;saved=s;channel=c;if(n<0)raise "child fault";}relay('+ARGS+'){spawn child(n,s,c);}clear(){saved=nil;channel=nil;wrapped=nil;}'
SOURCE+='''
Words:adt{a,b,c:int;};
Payload:adt{item:ref Item;note:string;words:Words;};
Envelope:adt{items:list of Payload;};
wrapped:ref Envelope;
direct(n:int,items:list of string,c:chan of string){
    spawn directchild(n,tl items,ref Envelope,c);
}
directchild(n:int,items:list of string,r:ref Envelope,c:chan of string){
    wrapped=r;
    child(n,hd items,c);
}
compound(n:int,items:array of ref Item,c:chan of string){
    w:=Words(n,n+1,n+2);
    spawn compoundchild(n,ref Envelope((items[1],nil,w)::nil),c);
}
compoundchild(n:int,r:ref Envelope,c:chan of string){
    wrapped=r;
    child(n,nil,c);
}
'''
def generate(limbo,w):
 rows=G['generate'](limbo,w,SOURCE,'Workers',(13,0,0,'~0U'));p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));globals={g[0]:g[1]for g in rows[0]['globals']}
 names=('launch','twice','chain','fresh','tailspawn','direct','compound')
 with p.open('a')as out:
  out.write('static const uint spawn_globals[]={'+','.join(str(globals[n])for n in('count','total','saved','channel','wrapped'))+'};\n')
  out.write('static const uint spawn_functions[]={'+','.join(str(next(i for i,r in enumerate(rows)if r['name']==n))for n in names)+'};\n')
 tail=next(r for r in rows if r['name']=='tailspawn')
 assert any(c[0]==E['OPCODES']['ITAIL'] and c[5]==64 for c in tail['code']),tail
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5] not in (38,179,186,248,255):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,IndexError,KeyError,ValueError,StopIteration):controls+=1
    else:raise AssertionError(('spawn oracle missed',r['name'],pc,k))
 print('PASS spawn source:',controls,'corruptions detected; independent classic FRAME/arguments/SPAWN and unchanged bytes',flush=True)
def boundaries(limbo,w):
 recursive='implement Closed;Closed:module{run:fn(n:int);};run(n:int){if(n>0){next:=n-1;spawn run(next);}}'
 _,rows=E['compile_source'](limbo,w,recursive,interface='',loads=True)
 assert rows[0]['accepted'] and any(step[5]==186 for step in rows[0]['code']),rows
 for source in(
  'implement Closed;Closed:module{run:fn(c:chan of (int,int));};run(c:chan of (int,int)){spawn child(c);}child(c:chan of (int,int)){n:= <-c;}',
  'implement Closed;Service:module{child:fn();};Closed:module{run:fn(s:Service);};run(s:Service){spawn s->child();}'):
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not next(r for r in rows if r['name']=='run')['accepted'],rows
 print('PASS spawn boundary: checked recursion, unsupported child and imported mspawn',flush=True)

def shell(limbo,w):
 source=(ROOT/'appl/cmd/sh/sh.b').read_text()
 source,count=re.subn(r'(?m)^(runasync\([^\{]+)\{.*?^\}',r'\1{ startchan <-= (1, ref Expropagate); }',source,flags=re.S)
 assert count==1
 data,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
 pipe=next(r for r in rows if r['name']=='pipecmd')
 assert pipe['accepted'] and {38,179,186,248}<=set(step[5]for step in pipe['code']),pipe
 imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 CMD['verify_nilbranches'](data,pipe,imports,rows=rows)
 print('PASS unchanged Sh pipecmd: real pipe argument sequence with an isolated runasync callee',flush=True)
def unit(records=False):
 src=TR['unit']().split('\n#line 1 "native64-channel-triples.c"')[0];extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();stack=(ROOT/'libinterp/stack.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
 src+='\nchar exSpawn[]="spawn a builtin module";\n'+extract(stack,'void\nunframe(')+'\n';start=vm.index('typedef struct SpawnProg');end=vm.index('/* Validate module ownership',start);src+='\n'+vm[start:end]+'\n'
 src=src.replace('static int\nspawnready(void *arg)\n{','static int spawnattempts,spawnfailure,spawnfailat;\nstatic int\nspawnready(void *arg)\n{\n spawnattempts++;if(spawnfailure&&spawnattempts==spawnfailat)failat=allocations+spawnfailure;')
 for signature in('String*\nstringdup(','void\ncvtup(','String*\naddstring(','OP(addc)'):src+='\n'+extract(strings,signature)+'\n'
 if records:
  src='#define TEST_REAL_MAPPED_CONS 1\n'+src
  heap=(ROOT/'libinterp/heap.c').read_text()
  for signature in('Heap*\nheaparray(','void\ninitarray('):
   src=src.replace(extract(src,signature),extract(heap,signature),1)
  src+='\n'+extract(heap,'void\nfreearray(')+'\n'
  for signature in('static uchar*\narrayaddress(','static void\narrayindex(','OP(indl)',
                    'static void\nnewrecord(','OP(newz)','Type*\ncconsmappedtype('):
   src+='\n'+extract(vm,signature)+'\n'
  src+='\n'+vm[vm.index('typedef struct ConsMapped'):vm.index('/* Preserve the destination, tail')]
  src+='\n'+extract(vm,'OP(consmp)')+'\n'
 start=src.index(' optab[IMOVW]=countmovw;');stop=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-spawn.c"\n'+(ROOT/'utils/tests/native64-spawn.c').read_text().replace(' SPAWN_HANDLERS',src[start:stop])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='spawn-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);shell(a.limbo.resolve(),w);(w/'unit.c').write_text(unit(records=True));arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('scheduler-operation','native64-typed-multi.h','else if(j==c->end){p->s.ind=40;','else if(j==c->end){if(s->op==ISPAWN)p->op=ICALL;p->s.ind=40;','actual scheduler process retirement'),
   ('void-callee-proof','native64-typed-module.h','if((body->returned && body->returned<=32) || body->returned==40 || body->returned==56 || (body->returned>=168 && body->returned<=170) || body->returned==171 || body->returned==174 || body->returned==185)return 0;','if(0)return 0;','spawn refusal before allocation')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS spawn active control:',name,flush=True)
if __name__=='__main__':main()
