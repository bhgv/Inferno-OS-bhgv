#!/usr/bin/env python3
"""Execute numeric String expression preparation inside fixed imported calls."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
INTERFACE='Sink:module{open:fn(path:string,mode:int):string;observe:fn(path:string);};'
DECL='Prepared:module{attach:fn(path:string);fresh:fn(n:int):string;forward:fn(n:int):string;prefix:fn(n:int):string;suffix:fn(n:int):string;fp:fn(n:int,left,right:string):string;reverse:fn(n:int,left,right:string):string;local:fn(n:int);};'
SOURCE='implement Prepared;'+INTERFACE+DECL+'sink:Sink;saved:string;attach(path:string){sink=load Sink path;}fresh(n:int):string{s:=sink->open("/prog/"+string n+"/ctl",0);return s;}forward(n:int):string{return sink->open("/prog/"+string n+"/ctl",0);}prefix(n:int):string{return sink->open("pre"+string n,1);}suffix(n:int):string{return sink->open(string n+"post",2);}fp(n:int,left,right:string):string{return sink->open(left+string n+right,3);}reverse(n:int,left,right:string):string{return sink->open(string n+left+right,4);}take(s:string){sink->observe(s);saved=s;}local(n:int){take(sink->open("/prog/"+string n+"/ctl",5));}'
def generate(limbo,w):
 m=w/'prepared.m';m.write_text(INTERFACE+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Sink',str(m)],text=True);frames={}
 for name in('open','observe'):
  size=int(re.search(r'^#define F_Sink_'+name+r'_size (\d+)',h,re.M)[1]);mapping=[int(x,0)for x in re.search(r'^#define F_Sink_'+name+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if x];signature=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_'+name+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(name,signature)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Prepared',(9,1,0,'~0U'),import_frames=frames);f=w/'full.h';f.write_text(f.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 with f.open('a')as out:
  globals={g[0]:g[1]for g in rows[0]['globals']};out.write('static const uint prep_globals[]={'+str(globals['sink'])+','+str(globals['saved'])+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in(180,181,182):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,IndexError,KeyError,ValueError,StopIteration):controls+=1
    else:raise AssertionError(('prepared String oracle missed',r['name'],pc,k))
 print('PASS prepared String source:',controls,'conversion/concat/clear corruptions, canonical prefix/hash and identical classic bytes',flush=True)
def boundaries(limbo,w):
 for kind in('big','real'):
  source=SOURCE.replace('n:int','n:'+kind)
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert any(not r['accepted'] for r in rows if r['name'] in('fresh','forward','prefix','suffix','fp','reverse')),rows
 print('PASS prepared String boundary: bigint/real conversions remain closed; classic bytes unchanged',flush=True)
def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');src='static int prep_allocations,prep_failure;\n'+src;src=src.replace('String*\nnewstring(int nb)\n{','String*\nnewstring(int nb)\n{\n prep_allocations++;if(prep_failure && prep_allocations==prep_failure)failat=allocations+1;').replace('String*\nnewrunes(int nr)\n{','String*\nnewrunes(int nr)\n{\n if(nr){prep_allocations++;if(prep_failure && prep_allocations==prep_failure)failat=allocations+1;}');extract=CMD['NS']['S']['FILES']['FILES']['function'];src=src.replace(extract(src,'int\nsnprint('),'',1);src+='\nvoid rerrstr(char*p,uint n){if(n)snprintf(p,n,"fixture");}\n';strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('String*\nstringdup(','void\ncvtup(','String*\naddstring(','OP(addc)','OP(cvtwc)'):src+='\n'+extract(strings,sig)+'\n'
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-string-prep.c"\n'+(ROOT/'utils/tests/native64-string-prep.c').read_text().replace(' PREP_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='string-prep-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';extra=[str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')];command=cc+[str(w/'unit.c')]+extra+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,before,after,predicate in (
   ('numeric-source','else if(s->returned==87 || s->returned==180 || s->returned==181){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','else if(s->returned==87 || s->returned==180 || s->returned==181){checkstep=*s;checkstep.returned=0;if(s->op==ICVTWC){checkstep.mode=SRC(AIMM)|DST(AFP);checkstep.s=0;}native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','complete prepared String length'),
   ('external-clear','else if(s->returned==87 || s->returned==180 || s->returned==181){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','else if(s->returned==87 || s->returned==180 || s->returned==181){checkstep=*s;checkstep.returned=0;if(s->returned==181){checkstep.mode=SRC(AFP)|DST(AFP);checkstep.s=s->d;}native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','external temporary cleared before callee'),
   ('local-clear','else if(s->returned==182){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','else if(s->returned==182){checkstep=*s;checkstep.returned=0;checkstep.mode=SRC(AFP)|DST(AFP);checkstep.s=s->d;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','local temporary cleared before callee')):
   original=(ROOT/'include/native64-typed-multi.h').read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/'native64-typed-multi.h').write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS prepared String active control:',name,flush=True)
if __name__=='__main__':main()
