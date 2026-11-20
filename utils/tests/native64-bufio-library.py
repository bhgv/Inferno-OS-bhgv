#!/usr/bin/env python3
"""Execute the complete original Bufio library through its native64 ABI."""
import argparse,copy,json,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ALT=runpy.run_path(str(ROOT/'utils/tests/native64-alt-replies.py'));G,CMD,E=ALT['G'],ALT['CMD'],ALT['E']
SOURCE=(ROOT/'appl/lib/bufio.b').read_text()
OP_NAMES=set()
def generate(limbo,w):
 interface=w/'interfaces.m';interface.write_text('include "sys.m";include "bufio.m";')
 headers={n:subprocess.check_output([str(limbo),'-H','-M',n,'-I',str(ROOT/'module'),str(interface)],text=True)for n in('Sys','Bufio','BufioFill')}
 signatures={n.replace('Iobuf_','Iobuf.'):int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Bufio_(\w+) (0x[0-9a-f]+)U',headers['Bufio'],re.M)}
 (w/'bufio-layout.h').write_text('typedef struct Bufio_Iobuf Bufio_Iobuf;\n'+re.search(r'struct Bufio_Iobuf \{.*?\n};',headers['Bufio'],re.S)[0]+'\n')
 frames={}
 for name,h in headers.items():
  for fn,size in re.findall(r'^#define F_'+name+r'_(\w+)_size (\d+)',h,re.M):
   mapping=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M);sig=re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+'_'+fn+r' (0x[0-9a-f]+)U',h,re.M)
   if sig:frames[(fn,int(sig[1],16))]=(int(size),[int(x,0)for x in mapping[1].split(',')if x])
 fill={'name':'fill','signature':int(re.search(r'SIGNATURE_BufioFill_fill (0x[0-9a-f]+)',headers['BufioFill'])[1],16)}
 rows=G['generate'](limbo,w,SOURCE,'Bufio',(27,2,0,'~0U'),import_frames=frames,export_signatures=signatures,extra_imports=[[fill]])
 global OP_NAMES
 OP_NAMES={name[1:].lower()for name,op in E['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')
 s+='\nstruct LibraryFunction {uint fn,argc,kinds[3],offsets[3],result,contract;};\nstatic const struct LibraryFunction library_functions[]={\n'
 for i,r in enumerate(rows):
  if r['export_name']not in signatures:continue
  kind=r['result'][0];contract=r['result'][1]|(16 if kind in E['ROOTS']else 0)
  s+='{'+f'{i},{len(r["args"])}'+',{'+','.join(str(a[1])for a in r['args'])+'},{'+','.join(str(a[0])for a in r['args'])+'},'+f'{kind},{contract}'+'},\n'
 s+='};\n';(w/'full.h').write_text(s)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]+[[fill]];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in range(153,160):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,tables,rows=records)
    except(AssertionError,ValueError,KeyError,IndexError,StopIteration):controls+=1
    else:raise AssertionError(('Bufio oracle missed',row['name'],pc,k))
 print('PASS complete Bufio independent oracle:',controls,'mutations; canonical callback and Sys frames, unchanged classic bytes',flush=True)
 return rows

def unit():
 original=ALT['unit']();src=original.split('\n#line 1 "native64-alt-replies.c"')[0];extract=CMD['NS']['S']['FILES']['FILES']['function'];strings=(ROOT/'libinterp/string.c').read_text();xec=(ROOT/'libinterp/xec.c').read_text();runt=(ROOT/'libinterp/runt.c').read_text()
 for sig in('void\nfreestring(','String*\nstringdup(','void\ncvtup(','String*\naddstring(','String*\nslicer('):
  if sig not in src:src+='\n'+extract((ROOT/'libinterp/heap.c').read_text()if 'freestring'in sig else strings,sig)+'\n'
 for sig in('static uchar*\narrayspan(','static uchar*\narrayaddress(','static void\narrayindex(','static void\nnewarray('):
  if sig not in src:src+='\n'+extract(xec,sig)+'\n'
 for n in sorted(OP_NAMES):
  n={'load':'iload','goto':'igoto'}.get(n,n)
  if 'OP('+n+')'not in src:src+='\n'+extract(strings if 'OP('+n+')\n{'in strings else xec,'OP('+n+')\n')+'\n'
 src+='\n#include "sys-native64.h"\n'
 for sig in('static int\nutfnleng(','static int\nsysutfarray(','static uchar*\nsysutfresult(','void\nSys_utfbytes(','void\nSys_byte2char(','void\nSys_char2byte('):
  if sig not in src:src+='\n'+extract(runt,sig)+'\n'
 start=original.rindex(' optab[IMOVW]=countmovw;');end=original.index('optab[IMOVP]=actual_movp;',start)
 handlers=original[start:end]+''.join(' optab['+name+']=actual_'+({'load':'iload','goto':'igoto'}.get(name[1:].lower(),name[1:].lower()))+';\n'for name in E['OPCODES']if 'OP('+({'load':'iload','goto':'igoto'}.get(name[1:].lower(),name[1:].lower()))+')'in src)
 return src+'\n#line 1 "native64-bufio-library.c"\n'+(ROOT/'utils/tests/native64-bufio-library.c').read_text().replace(' LIBRARY_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='bufio-library-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h','native64-library-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
  header=(ROOT/'include/native64-library-layout.h').read_text();before='id>=1&&id<=3?0xc0';assert header.count(before)==1;(w/'native64-library-layout.h').write_text(header.replace(before,'id>=1&&id<=3?0x80'))
  subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'exact Iobuf/Filler descriptors'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS Bufio active control: lost buffer GC root detected',flush=True)

if __name__=='__main__':main()
