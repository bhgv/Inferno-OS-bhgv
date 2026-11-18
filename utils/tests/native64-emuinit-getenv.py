#!/usr/bin/env python3
"""Execute unchanged emuinit getenv/unquoted through real Sys bodies and native calls."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'));EXT=CMD['EXT'];PARSE=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-unquoted.py'))
def generate(limbo,w):
 body=(ROOT/'appl/cmd/emuinit.b').read_text();body=body[body.index('getenv(v:'):]
 source='implement Env;include "provider.m";Env:module{getenv:fn(v:string):list of string;envload:fn(path:string);};sys:Sys;\n'+body+'\nenvload(path:string){sys=load Sys path;}\n'
 d,rows=EXT['compile_source'](limbo,w,source,interface=(ROOT/'module/sys.m').read_text(),loads=True)
 assert len(rows)==3 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'),r.get('pc'))for r in rows]
 imports=EXT['BASE']['READ']['legacy_instructions'](d,with_imports=True)[-1];assert len(imports)==1 and [e['name']for e in imports[0]]==['fstat','open','read']
 EXT['MODULE']['check_exports'](d,rows)
 for r in rows:CMD['verify_nilbranches'](d,r,imports[0],rows=rows)
 for fault in('length-source','conversion-source','prelude-destination','local-target'):
  changed=json.loads(json.dumps(rows));r=changed[0]
  if fault=='local-target':r['calls'][0][2]=changed[2]['first']
  else:
   op=EXT['OPCODES']['ILENA']if fault=='length-source'else EXT['OPCODES']['ICVTAC']if fault=='conversion-source'else EXT['OPCODES']['ISLICEA']
   step=next(c for c in r['code']if c[0]==op);step[4 if fault=='prelude-destination'else 2]+=4
  try:
   for r in changed:CMD['verify_nilbranches'](d,r,imports[0],rows=changed)
  except (AssertionError,StopIteration,KeyError):pass
  else:raise AssertionError('getenv source oracle missed '+fault)
 _,old=EXT['compile_source'](limbo,w,source,interface=(ROOT/'module/sys.m').read_text(),loads=False)
 assert all(not r['accepted']for r in old)
 wrong='implement Wrong;P:module{n:fn(n:int):int;};Wrong:module{run:fn(p:P,a:array of int):int;};run(p:P,a:array of int):int{return p->n(len a);}'
 _,bad=EXT['compile_source'](limbo,w,wrong,interface='',loads=True)
 assert not bad[0]['accepted']and bad[0]['reason']=='external-byte-length-argument'
 boundary='implement Boundary;Boundary:module{run:fn(a:array of byte):list of string;};run(a:array of byte):list of string{return pair(string a,string a);}pair(x:string,y:string):list of string{return nil;}'
 _,closed=EXT['compile_source'](limbo,w,boundary,interface='',loads=True)
 assert not next(r for r in closed if r['name']=='run')['accepted']
 lines=['#include "native64-typed-module.h"']
 for i,e in enumerate(imports[0]):
  size=72 if e['name']=='fstat'else 80 if e['name']=='open'else 88;bits=192 if e['name']=='read'else 128
  lines+=['static const uchar env_import_map_'+str(i)+'[]={0,'+str(bits)+'};']
 lines+=['static const Native64ModuleImport env_imports[]={']+['{"'+e['name']+'",'+str(e['signature'])+'U,'+str(72 if e['name']=='fstat'else 80 if e['name']=='open'else 88)+',env_import_map_'+str(i)+',2},'for i,e in enumerate(imports[0])]+['};','static const Native64ModuleTable env_tables[]={{env_imports,3}};']
 exports={e['name']:e for e in EXT['BASE']['READ']['legacy_instructions'](d,with_exports=True)[-1]}
 for r in rows:
  n=r['name'];lines+=['static const Native64TypedStep env_code_'+n+'[]={']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};','static const uchar env_map_'+n+'[]={'+','.join(map(str,r['frame']['map']))+'};']
  sites=[(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])for e in r.get('externals',[])]+[(a,b,next(i for i,x in enumerate(rows)if x['first']==target),contract,inherited,0,0,0,0)for a,b,target,desc,offset,contract,inherited in r.get('calls',[])]
  lines+=['static const Native64ModuleSite env_sites_'+n+'[]={']+['{'+','.join(map(str,s))+'},'for s in sorted(sites)]+['};']
 lines+=['static const Native64ModuleFunction env_functions[]={']
 for r in rows:
  n=r['name'];e=exports.get(n);lines+=['{"'+n+'",'+str(e['signature']if e else 0)+f'U,{int(bool(e))},env_code_{n},{len(r["code"])},{r["frame"]["bytes"]},env_map_{n},{len(r["frame"]["map"])},env_sites_{n},'+str(len(r.get('calls',[]))+len(r.get('externals',[])))+'},']
 lines+=['};','static const uchar env_data_map[]={'+','.join(map(str,rows[0]['data']['map']))+'};']
 def raw(n,b):lines.append('static const uchar '+n+'[]={'+','.join(map(str,b or b'\0'))+'};')
 initial=EXT['BASE']['READ']['legacy_instructions'](d,with_types=True,with_data=True)[3];strings=[]
 for old,cell in initial.items():
  assert cell['kind']=='string';text=bytes(cell['bytes']);name='env_literal_'+str(old);raw(name,text);offset=next(l[1]for l in rows[0]['data']['leaves']if l[0]==old);strings.append('{'+f'{offset},{len(text)},{name}'+'}')
 lines+=['static const Native64ModuleString env_strings[]={'+','.join(strings)+'};','static const Native64ModuleSpec env_spec={"Env","trusted/Env",env_functions,3,~0U,'+str(rows[0]['data']['bytes'])+',env_data_map,sizeof(env_data_map),nil,0,env_tables,1,c2string,env_strings,'+str(len(strings))+',read64ownedimport};','static const uint env_sys_offset='+str(next(g[1]for g in rows[0]['globals']if g[0]=='sys'))+';']
 cases=PARSE['CASES']
 for i,(text,expected)in enumerate(cases):
  raw('env_input_'+str(i),text or b'')
  for j,word in enumerate(expected):raw(f'env_word_{i}_{j}',word)
  lines+=['static const EnvWord env_words_'+str(i)+'[]={']+['{'+f'{len(word)},env_word_{i}_{j}'+'},'for j,word in enumerate(expected)]+(['{0,nil},']if not expected else[])+['};']
 lines+=['static const EnvCase env_cases[]={']+['{'+f'{len(text or b"")},env_input_{i},{len(expected)},env_words_{i}'+'},'for i,(text,expected)in enumerate(cases)]+['};']
 (w/'env.h').write_text('\n'.join(lines)+'\n');(w/'sys-native64.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Sys',str(ROOT/'module/sys.m')],text=True))
 print('PASS unchanged emuinit getenv and unquoted: complete native bodies and exact real Sys import signatures',flush=True)
 return d,rows,imports[0]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 if not __debug__:p.error('source proofs require assertions')
 runpy.run_path(str(ROOT/'utils/tests/dis64-computed-args.py'))['main']()
 with tempfile.TemporaryDirectory(prefix='real-getenv-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w)
  frontier=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](a.limbo.resolve(),'appl/cmd/emuinit.b',w)
  assert next(f for f in frontier['functions']if f['name']=='getenv')['accepted']
  source=CMD['unit']().replace('int main(int argc,char **argv)','int old_command_main(int argc,char **argv)');extract=CMD['NS']['S']['FILES']['FILES']['function'];heap=(ROOT/'libinterp/heap.c').read_text();xec=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text();port=(ROOT/'emu/port/inferno.c').read_text()
  source=source.replace(extract(source,'Heap*\nheaparray('),'').replace(extract(source,'void\ninitarray('),'')
  source+='\nvoid initarray(Type*,Array*);\n'
  for sig in('Heap*\nheap(','void\nacheck(','Heap*\nheaparray(','void\ninitarray(','void\nfreearray('):source+='\n'+extract(heap,sig)+'\n'
  for sig in('static uchar*\narrayspan(','static void\nnewarray(','OP(newaz)','OP(slicea)','OP(lena)','OP(cvtlw)'):source+='\n'+extract(xec,sig)+'\n'
  for sig in('String*\nstringdup(','void\ncvtup(','String*\naddstring(','String*\nslicer(','OP(addc)','OP(indc)','OP(slicec)','OP(cvtac)'):source+='\n'+extract(strings,sig)+'\n'
  source+='\n#include "sys-native64.h"\nstatic Dir*envdir(int);static int envopen(char*,int);static long envread(int,void*,long);static Sys_FD*envmkfd(int);\nchar Ebadarg[]="bad argument";void seterror(char*s,...){kwerrstr("%s",s);}\n'
  for sig in('static int\nsysnativeint(','static int\nsysiofd(','static void\nsysstatusframe(','static void*\nsysresultowner(','static void\nsysdirowners(','static void\nunpackdir(','static int\nsysiocount(','static void\nsysioframe('):source+='\n'+extract(port,sig)+'\n'
  source+='\n'+extract((ROOT/'libinterp/runt.c').read_text(),'void\nretstr(')+'\n'
  source+='\n#define kdirfstat envdir\n#define kopen envopen\n#define kread envread\n#define mkfd envmkfd\n'
  for sig in('void\nSys_open(','void\nSys_fstat(','void\nSys_read('):source+='\n'+extract(port,sig)+'\n'
  source+='\n#undef kdirfstat\n#undef kopen\n#undef kread\n#undef mkfd\n'
  start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start)
  source+='\n#line 1 "native64-emuinit-getenv.c"\n'+(ROOT/'utils/tests/native64-emuinit-getenv.c').read_text().replace(' ENV_HANDLERS',source[start:end]);f=w/'unit.c';f.write_text(source)
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'));arch=os.environ.get('NATIVE64_ARCH','amd64')
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';cmd=cc+[str(f),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(cmd,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert r.returncode==0 and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr);print(r.stdout,end='',flush=True)
  header=(ROOT/'include/native64-typed-module.h').read_text();before='map=USRC(step->mode)==AFP?f->map:s->datamap;bit=step->s/8;\n            if(!(map[bit/8]&(0x80>>(bit%8))))return 0;';assert header.count(before)==1
  (w/'native64-typed-module.h').write_text(header.replace(before,';',1));subprocess.run(cmd,check=True)
  r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),'metadata'],capture_output=True,text=True,timeout=180)
  assert r.returncode==10 and 'getenv metadata refusal removed: case=0'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
  print('PASS getenv active control: computed arguments require mapped source roots',flush=True)
  (w/'native64-typed-module.h').unlink()
  multi=(ROOT/'include/native64-typed-multi.h').read_text();before='else if(!s->returned){native64typedcode(p,1,s,1,fpbytes,mpbytes);}';assert multi.count(before)==1
  (w/'native64-typed-module.h').write_text(header);(w/'native64-typed-multi.h').write_text(multi.replace(before,'else if(!s->returned){native64typedcode(p,1,s,1,fpbytes,mpbytes);p->d.ind+=8;}',1))
  r=None;subprocess.run(cmd,check=True)
  # Validate prep publication directly before VM execution, avoiding a bad callback.
  r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary),'metadata'],capture_output=True,text=True,timeout=180)
  assert r.returncode==10 and 'real getenv predicate: direct call prelude publication'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
  print('PASS getenv active control: direct prelude destinations preserved',flush=True)
if __name__=='__main__':main()
