#!/usr/bin/env python3
"""Checked owned byte-array allocation, slices and String conversion in C/JIT."""
import argparse,json,os,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
CMD=runpy.run_path(str(ROOT/'utils/tests/native64-typed-command-files.py'));EXT=CMD['EXT']
SOURCE='''implement Bytes;Bytes:module{alloc:fn(n:int):string;cut:fn(a:array of byte,n:int):string;};
alloc(n:int):string{a:=array[n]of byte;return string a;}
cut(a:array of byte,n:int):string{return string a[0:n];}
'''
def generate(limbo,w):
 d,rows=EXT['compile_source'](limbo,w,SOURCE,interface='',loads=True)
 assert len(rows)==2 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows]
 EXT['MODULE']['check_exports'](d,rows)
 types=EXT['BASE']['READ']['legacy_instructions'](d,with_types=True)[2]
 for r in rows:
  # Classic INEWA must become zeroed native NEWAZ, never a raw descriptor ordinal.
  normal=json.loads(json.dumps(r));old=EXT['BASE']['READ']['legacy_instructions'](d)[1]
  for i,c in enumerate(normal['code']):
   if c[0]==EXT['OPCODES']['INEWAZ']:
    o=old[r['first']+i];assert o['op']==EXT['OPCODES']['INEWA']and c[3]==0
    assert types[o['m']][0]==1 and not any(types[o['m']][1])
    c[0]=o['op'];c[3]=o['m']
  EXT['verify'](d,normal,[],loads=True)
 _,closed=EXT['compile_source'](limbo,w,SOURCE,interface='',loads=False)
 assert all(not r['accepted']for r in closed)
 _,bad=EXT['compile_source'](limbo,w,SOURCE.replace('array of byte','array of int').replace('array[n]of byte','array[n]of int').replace('return string a','return string len a'),interface='',loads=True)
 assert all(not r['accepted']for r in bad)
 lines=['#include "native64-typed-module.h"']
 for r in rows:
  n=r['name'];lines+=['static const Native64TypedStep bytes_'+n+'[]={']+['{'+','.join(map(str,c))+'},'for c in r['code']]+['};','static const uchar bytes_map_'+n+'[]={'+','.join(map(str,r['frame']['map']))+'};']
 lines+=['static const Native64ModuleFunction bytes_functions[]={']+['{"'+r['name']+'",'+str(r['signature'])+'U,1,bytes_'+r['name']+f',{len(r["code"])},{r["frame"]["bytes"]},bytes_map_'+r['name']+','+str(len(r['frame']['map']))+'},'for r in rows]+['};','static const uchar bytes_data_map[]={'+','.join(map(str,rows[0]['data']['map']))+'};','static const Native64ModuleSpec bytes_spec={"Bytes","trusted/Bytes",bytes_functions,2,~0U,'+str(rows[0]['data']['bytes'])+',bytes_data_map,sizeof(bytes_data_map)};']
 (w/'bytes.h').write_text('\n'.join(lines)+'\n');print('PASS byte-array source: full bodies, classic descriptor/token relocation and int-array refusal',flush=True)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='emuinit-bytes-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w)
  source=CMD['unit']().replace('int main(int argc,char **argv)','int old_command_main(int argc,char **argv)');extract=CMD['NS']['S']['FILES']['FILES']['function'];heap=(ROOT/'libinterp/heap.c').read_text();xec=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
  source=source.replace(extract(source,'Heap*\nheaparray('),'').replace(extract(source,'void\ninitarray('),'')
  source+='\nvoid initarray(Type*,Array*);\n'
  for sig in ('Heap*\nheap(','void\nacheck(','Heap*\nheaparray(','void\ninitarray(','void\nfreearray('):source+='\n'+extract(heap,sig)+'\n'
  for sig in ('static uchar*\narrayspan(','static void\nnewarray(','OP(newaz)','OP(slicea)'):source+='\n'+extract(xec,sig)+'\n'
  source+='\n'+extract(strings,'OP(cvtac)')+'\n'
  start=source.index(' optab[IMOVW]=countmovw;');end=source.index(' require(prepare64calls(',start)
  source+='\n#line 1 "native64-emuinit-bytes.c"\n'+(ROOT/'utils/tests/native64-emuinit-bytes.c').read_text().replace(' BYTES_HANDLERS',source[start:end]);f=w/'unit.c';f.write_text(source)
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'));arch=os.environ.get('NATIVE64_ARCH','amd64')
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';cmd=cc+[str(f),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''));subprocess.run(cmd,check=True)
  r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180);assert r.returncode==0 and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr);print(r.stdout,end='',flush=True)
  header=(ROOT/'include/native64-typed-module.h').read_text();before='if(roots[k]!=!!(map[bit/8]&(0x80>>(bit%8))))return 0;';assert header.count(before)==1
  (w/'native64-typed-module.h').write_text(header.replace(before,';',1));subprocess.run(cmd,check=True);r=subprocess.run(runner+[str(binary),'metadata'],capture_output=True,text=True,timeout=180)
  assert r.returncode==10 and 'byte metadata refusal removed: case=0'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
  print('PASS byte active control: constructor requires exact root/scalar roles',flush=True)
  before='m->prog[b->offsets[i]+j].reg=f->code[j].returned==254?refarraytype:s->nfunctions+1+b->imports;';assert header.count(before)==1
  (w/'native64-typed-module.h').write_text(header.replace(before,';',1));subprocess.run(cmd,check=True);r=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=180)
  assert r.returncode==10 and 'byte-array predicate: resolved byte descriptor'in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(r.returncode,r.stdout,r.stderr)
  print('PASS byte active control: primitive descriptor resolved before execution',flush=True)
if __name__=='__main__':main()
