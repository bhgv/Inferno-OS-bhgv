#!/usr/bin/env python3
"""Execute checked String-to-Word conversion through the actual VM parser and UTF cache."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
NAMES=('parse','alias','mp','repeat','branch','call')
DECL='Parseword:module{'+''.join(n+':fn(s:string);'for n in NAMES)+'};'
SOURCE='implement Parseword;'+DECL+'saved:string;out,after:int;'
SOURCE+='parse(s:string){n:=len s;guard:=n;n=int s;out=n;after=guard;}alias(s:string){hold:=s;s=nil;n:=int hold;out=n;}mp(s:string){saved=s;n:=int saved;out=n;}repeat(s:string){a:=int s;b:=int s;out=a+b;}branch(s:string){n:=int s;if(n>0)out=1;else if(n<0)out= -1;else out=0;}call(s:string){n:=worker(s);out=n;}worker(s:string):int{n:=int s;return n;}'
def generate(limbo,w):
 m=w/'parseword.m';m.write_text(DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Parseword',str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Parseword_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Parseword',(7,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint parse_globals[]={'+','.join(str(gl[n])for n in('saved','out','after'))+'};\n')
 parsed=next(r for r in rows if r['name']=='parse');convert=next(c for c in parsed['code']if c[0]==E['OPCODES']['ICVTCW']);length=next(c for c in parsed['code']if c[0]==E['OPCODES']['ILENC']);assert length[4]==convert[4]and any(c[0]==E['OPCODES']['IMOVW']and c[2]==convert[4]and c[4]==convert[4]+4 for c in parsed['code']),(convert,length)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[0]!=E['OPCODES']['ICVTCW']:continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('String word oracle missed',row['name'],k))
 print('PASS String word oracle:',controls,'corruptions detected; exact classic opcode, root/Word leaves, maps and unchanged bytes',flush=True)
 for declaration,body in(('Closed:module{run:fn(s:string):int;};','run(s:string):int{return int s;}'),('Closed:module{run:fn(s:string);};out:int;','run(s:string){out=int s;}'),('Closed:module{run:fn(s:string);};out:big;','run(s:string){v:=big s;out=v;}'),('Closed:module{run:fn(s:string);};out:real;','run(s:string){v:=real s;out=v;}')):
  _,closed=E['compile_source'](limbo,w,'implement Closed;'+declaration+body,interface='',loads=True);assert not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 init=next(r for r in records if r['name']=='init');assert init['accepted']or init['pc']>187,init;assert next(r for r in records if r['name']=='reshaped')['accepted'];print('AWM init frontier:',init.get('pc'),init.get('reason'),'; reshaped accepted',flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start);handlers=src[start:end]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\n'+extract((ROOT/'libinterp/heap.c').read_text(),'void\nfreestring(')+'\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(cvtcw)\n')+'\n'
 return src+'\n#line 1 "native64-string-word.c"\n'+(ROOT/'utils/tests/native64-string-word.c').read_text().replace(' PARSE_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='string-word-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);source=unit();(w/'unit.c').write_text(source);arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,before,after in (
   ('source must be a GC root','mode=USRC(step->mode);map=mode==AFP?f->map:s->datamap;bit=step->s/8;\n            if(!(map[bit/8]&(128>>(bit%8))))return 0;', 'mode=USRC(step->mode);map=mode==AFP?f->map:s->datamap;bit=step->s/8;\n            if(0)return 0;'),
   ('destination must be raw','for(k=0;k<4;k++){bit=(step->d+k)/8;if(f->map[bit/8]&(128>>(bit%8)))return 0;}', 'for(k=0;k<4;k++){bit=(step->d+k)/8;if(0)return 0;}')):
   text=(ROOT/'include/native64-typed-module.h').read_text();assert text.count(before)==1;(w/'native64-typed-module.h').write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and 'malformed String Word metadata refused'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   (w/'native64-typed-module.h').unlink();print('PASS String Word active control:',name,flush=True)
  extract=CMD['NS']['S']['FILES']['FILES']['function'];body=extract(source,'OP(cvtcw)\n');bad=body.replace('WORD value;','WORD value;LONG wide;').replace('memmove(R.d, &value, sizeof(value));','wide=value;memmove(R.d, &wide, sizeof(wide));');assert bad!=body;(w/'unit.c').write_text(source.replace(body,bad,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'four-byte conversion preserves adjacent local'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS String Word active control: full 8-byte store detected by adjacent local',flush=True)
if __name__=='__main__':main()
