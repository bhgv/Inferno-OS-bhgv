#!/usr/bin/env python3
"""Execute bounded three-channel return contracts across checked Limbo/C/JIT calls."""
import argparse,copy,os,re,resource,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
PREFIX='include "sys.m";include "draw.m";include "tk.m";include "wmsrv.m";'
TRIPLE='(chan of int,chan of int,chan of int)'
REAL='(chan of (string,chan of (string,ref Draw->Wmcontext)),chan of (ref Wmsrv->Client,chan of string),chan of (ref Wmsrv->Client,array of byte,Sys->Rwrite))'
ARGS='a,b,c:chan of int'
INTERFACE='Sink:module{produce:fn('+ARGS+'):'+TRIPLE+';};'
DECL='Triples:module{open:fn(path:string);'+''.join(n+':fn('+('flag:int,'if n=='choose'else'')+ARGS+'):'+TRIPLE+';'for n in('direct','forward','fresh','choose','swapped','external','externalfresh'))+'nils:fn():'+TRIPLE+';actual:fn():'+REAL+';store:fn('+ARGS+');bytes:fn(n:int):array of byte;};'
SOURCE='implement Triples;'+PREFIX+INTERFACE+DECL+'sink:Sink;wmsrv:Wmsrv;ga,gb,gc:chan of int;open(path:string){sink=load Sink path;wmsrv=load Wmsrv path;}direct('+ARGS+'):'+TRIPLE+'{return(a,b,c);}nils():'+TRIPLE+'{return(nil,nil,nil);}forward('+ARGS+'):'+TRIPLE+'{return direct(a,b,c);}fresh('+ARGS+'):'+TRIPLE+'{(x,y,z):=direct(a,b,c);return(x,y,z);}choose(flag:int,'+ARGS+'):'+TRIPLE+'{if(flag)return(a,b,c);return(nil,nil,nil);}swapped('+ARGS+'):'+TRIPLE+'{return(c,b,a);}external('+ARGS+'):'+TRIPLE+'{return sink->produce(a,b,c);}externalfresh('+ARGS+'):'+TRIPLE+'{(x,y,z):=sink->produce(a,b,c);return(x,y,z);}actual():'+REAL+'{return wmsrv->init();}store('+ARGS+'){(x,y,z):=external(a,b,c);ga=x;gb=y;gc=z;}bytes(n:int):array of byte{a:=array[n]of byte;return a;}'
def generate(limbo,w):
 m=w/'triples.m';m.write_text(PREFIX+INTERFACE+DECL);headers={n:subprocess.check_output([str(limbo),'-H','-M',n,'-I',str(ROOT/'module'),str(m)],text=True)for n in('Sink','Wmsrv','Triples')};frames={}
 for owner,name in(('Sink','produce'),('Wmsrv','init')):
  h=headers[owner];size=int(re.search(r'^#define F_'+owner+'_'+name+r'_size (\d+)',h,re.M)[1]);mapping=[int(x,0)for x in re.search(r'^#define F_'+owner+'_'+name+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if x];signature=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_'+owner+'_'+name+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(name,signature)]=(size,mapping)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Triples_(\w+) (0x[0-9a-f]+)U',headers['Triples'],re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Triples',(12,2,0,'~0U'),import_frames=frames,export_signatures=signatures);f=w/'full.h';f.write_text(f.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 with f.open('a')as out:
  globals={g[0]:g[1]for g in rows[0]['globals']};out.write('static const uint triple_globals[]={'+','.join(str(globals[n])for n in('sink','wmsrv','ga','gb','gc'))+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in(168,169,170,171,200):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('triple oracle missed',r['name'],pc,k))
 print('PASS triple source oracle:',controls,'component/binding mutations; real Wmsrv signature/prefix and classic bytes',flush=True)
def boundaries(limbo,w):
 for result,values in(('(chan of int,chan of int,string)','(nil,nil,nil)'),('(chan of int,chan of int,chan of int,chan of int)','(nil,nil,nil,nil)')):
  source='implement Closed;Closed:module{run:fn():'+result+';};run():'+result+'{return'+values+';}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted']and rows[0]['reason']=='aggregate-result',rows
 print('PASS triple boundary: mixed roots and four-channel results remain closed; classic bytes preserved',flush=True)
def flowproof(w):
 base=(ROOT/'utils/tests/dis64-typed.c').read_text().replace('int main(void)','int previous_typed_main(void)')
 tests=r'''
extern int triple_flow(Dis64TypedBody*);
static void flowcase(Dis64TypedBody*p,int expected){p->reason=nil;p->badpc=0;assert(triple_flow(p)==expected);assert(owners==0);if(!expected)assert(p->reason&&strcmp(p->reason,"undefined-result")==0);}
int main(void){Dis64TypedBody p={0};Dis64Instruction code[8]={{0}};uchar flags[8]={2,4,8};uint i;p.resultkind=Ttuple;p.resulttriple=1;p.first=700;p.count=4;p.code=code;p.returns=flags;for(i=0;i<3;i++)code[i].op=IMOVP;code[3].op=IRET;flowcase(&p,1);for(i=0;i<3;i++){flags[i]=0;flowcase(&p,0);flags[i]=2<<i;}
 memset(code,0,sizeof(code));memset(flags,0,sizeof(flags));p.count=8;code[0].op=IBEQW;code[0].destination=4;code[1].op=code[2].op=code[4].op=code[5].op=code[6].op=IMOVP;flags[1]=flags[4]=2;flags[2]=flags[5]=4;flags[6]=8;code[3].op=IJMP;code[3].destination=6;code[7].op=IRET;flowcase(&p,1);flags[5]=0;flowcase(&p,0);
 memset(code,0,sizeof(code));memset(flags,0,sizeof(flags));p.count=6;for(i=0;i<3;i++){code[i].op=IMOVP;flags[i]=2<<i;}code[3].op=IBEQW;code[3].destination=5;code[4].op=IJMP;code[4].destination=3;code[5].op=IRET;flowcase(&p,1);flags[2]=0;flowcase(&p,0);
 p.count=2;code[0].op=ICALL;code[1].op=IRET;flags[0]=32;flowcase(&p,1);code[0].op=IMOVMP;flags[0]=171;flowcase(&p,1);return 0;}
'''
 fixture=w/'flow-fixture.c';core=w/'flow-core.c';binary=w/'flow';fixture.write_text(base+tests);original=(ROOT/'limbo/dis64-typed.c').read_text();wrapper='\nint triple_flow(Dis64TypedBody*p){return typedresults(p); }\n';core.write_text(original+wrapper)
 cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
 command=cc+['-Dfree=typedfree','-ffunction-sections','-fdata-sections']+['-I'+str(ROOT/x)for x in('Linux/386/include','include','limbo')]+[str(ROOT/'limbo/dis64-layout.c'),str(core),str(fixture),'-Wl,--gc-sections','-o',str(binary)]
 runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''));subprocess.run(command,check=True);subprocess.run(runner+[str(binary)],check=True,timeout=30)
 before='state[pc]!=15';assert original.count(before)==1;core.write_text(original.replace(before,'(state[pc]|8)!=15')+wrapper);subprocess.run(command,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30);assert r.returncode and 'assert'in r.stderr.lower() and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stderr)
 print('PASS triple definite-result flow: ten cases, missing each component, diamond/loop joins, inherited/mapped facts and missing-third-root guard control',flush=True)
def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();heap=(ROOT/'libinterp/heap.c').read_text()
 src+='\nHeap*heap(Type*t){return heapz(t); }\n'+extract(heap,'void\nincmem(')+'\n'+extract(vm,'static int\nmappedcopytype(')+'\n'+extract(vm,'OP(movmp)')+'\n'+extract(vm,'OP(movm)')+'\nvoid movtmp(void){require(0);}\n'
 src+='void killcomm(Progq**q){if(*q){require((*q)->prog==nil&&(*q)->next==nil);free(*q);*q=nil;}}\n'+extract(heap,'void\nfreechan(')+'\nType Tchannel={1,freechan,markheap,sizeof(Channel),1,0,0,{0x80}};\n'+extract(heap,'void\nacheck(')+'\n'
 start=vm.index('typedef struct ChannelBuild');end=vm.index('/* Validate capacity',start);src+=vm[start:end]
 handlers=src[src.index(' optab[IMOVW]=countmovw;'):src.index(' require(prepare64calls(',src.index(' optab[IMOVW]=countmovw;'))]
 return src+'\n#line 1 "native64-channel-triples.c"\n'+(ROOT/'utils/tests/native64-channel-triples.c').read_text().replace(' TRIPLE_HANDLERS',handlers)
def main():
 resource.setrlimit(resource.RLIMIT_CORE,(0,0));p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='channel-triples-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);flowproof(w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('component-offset','native64-typed-code.h','p->d.i.s=8*(s->returned-168);','p->d.i.s=s->returned==170?8:8*(s->returned-168);','exact three caller result cells'),
   ('mapped-copy-third-root','native64-typed-module.h','uchar roots[]={0xe0};m->type[ordinal]=dtype(freeheap,24,roots,1);','uchar roots[]={0xc0};m->type[ordinal]=dtype(freeheap,24,roots,1);','exact channel reference ownership'),
   ('binding-third-root','native64-local-bind.h','at<(unsigned int)instruction.s.ind+width','at<(unsigned int)instruction.s.ind+(contract==72?16:width)','triple execution/error'),
   ('byte-copy-descriptors','native64-typed-module.h','m->prog[b->offsets[i]+j].reg=s->nfunctions+1+b->imports;','m->prog[b->offsets[i]+j].reg=m->ntype-1;','separate byte/copy descriptors'),
   ('callee-result-proof','native64-typed-multi.h','((c->contract==72)!=(c->resultcontract==72))','0','callee triple result proof required')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-local-bind.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-local-bind.h'):(w/parent).unlink()
   print('PASS triple active control:',name,flush=True)
if __name__=='__main__':main()
