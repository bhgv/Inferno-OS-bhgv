#!/usr/bin/env python3
"""Execute a computed String list in a pending imported frame, including allocation failure."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
PREFIX='include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";wmclient:Wmclient;Window:import wmclient;'
SINK='Sink:module{take:fn(win:ref Window,devs:list of string);};'
INTERFACE='ListCalls:module{open:fn(path:string);run:fn(win:ref Window);};'
SOURCE='implement ListCalls;'+PREFIX+SINK+INTERFACE+'sink:Sink;open(path:string){sink=load Sink path;}run(win:ref Window){sink->take(win,"kbd"::"ptr"::nil);}'
def generate(limbo,w):
 m=w/'listcalls.m';m.write_text(PREFIX+SINK+INTERFACE)
 header=subprocess.check_output([str(limbo),'-H','-M','ListCalls','-I',str(ROOT/'module'),str(m)],text=True)
 canonical={n:int(value,16)for n,value in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_ListCalls_(\w+) (0x[0-9a-f]+)U',header,re.M)}
 sinkheader=subprocess.check_output([str(limbo),'-H','-M','Sink','-I',str(ROOT/'module'),str(m)],text=True);(w/'sink-native.h').write_text(sinkheader)
 size=int(re.search(r'^#define F_Sink_take_size (\d+)',sinkheader,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Sink_take_map \{([^}]*)\}',sinkheader,re.M)[1].split(',')if v]
 signature=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_take (0x[0-9a-f]+)U',sinkheader,re.M)[1],16)
 assert (size,mapping)==(80,[0,192])
 rows=G['generate'](limbo,w,SOURCE,'ListCalls',(2,1,0,'~0U'),import_frames={('take',signature):(size,mapping)},export_signatures=canonical)
 f=w/'full.h';f.write_text(f.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'static const uint device_sink='+str(next(g[1]for g in rows[0]['globals']if g[0]=='sink'))+';\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];r=records[1];controls=0
 for pc,c in enumerate(r['code']):
  if c[5]!=112:continue
  for k in range(6):
   bad=copy.deepcopy(r);bad['code'][pc][k]+=1
   try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
   except(AssertionError,KeyError,IndexError,StopIteration):controls+=1
   else:raise AssertionError(('list prep oracle missed',pc,k))
 print('PASS list call oracle:',controls,'prep/clear record mutations detected; canonical Sink prefix/hash and classic bytes preserved',flush=True)
def refusals(limbo,w):
 for kind,argument,extra in(('int','1::nil',''),('string','"kbd"::tail',',tail:list of string')):
  source='implement Closed;Sink:module{take:fn(devs:list of '+kind+');};Closed:module{run:fn(s:Sink'+extra+');};run(s:Sink'+extra+'){s->take('+argument+');}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted'],rows
 print('PASS pending list compiler boundary: int elements and borrowed tail remain closed; ordinary bytes preserved',flush=True)

def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
 extract=CMD['NS']['S']['FILES']['FILES']['function'];body=extract((ROOT/'libinterp/xec.c').read_text(),'List*\ncons(');assert src.count(body)==1;src=src.replace(body,body.replace('List*\ncons(', 'List*\ndevicecons(',1),1)
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-list-call.c"\n'+(ROOT/'utils/tests/native64-list-call.c').read_text().replace(' DEVICE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='list-call-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);refusals(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('clear','native64-typed-multi.h','else if(s->returned==Native64TypedListPrep){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}',
    'else if(s->returned==Native64TypedListPrep){if(j>i+1 && steps[j-1].returned==64){memset(p,0,sizeof(*p));p->op=IMOVP;p->add=SRC(AFP)|DST(AFP);p->s.ind=s->d;p->d.ind=s->d;}else{checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}}','prepared list owns one reference'),
   ('root-map','native64-typed-module.h','if((map[bit/8] & (128 >> (bit%8)))==0)return 0;','if(0)return 0;','malformed list metadata refused'),
   ('target-map','native64-typed-module.h','if(!(request->map[bit/8]&(128>>(bit%8))))return 0;','if(0)return 0;','malformed list metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS pending list active control:',name,flush=True)

if __name__=='__main__':main()
