#!/usr/bin/env python3
"""Execute unchanged Wmlib.snarfget with partial reads, growing arrays and EOF."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
B=runpy.run_path(str(ROOT/'utils/tests/native64-bufio-library.py'));G,CMD,E=B['G'],B['CMD'],B['E']
CLIENT=re.search(r'(?m)^Client: adt\{.*?\n\};',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]
SOURCE='''implement Wmlib;include "sys.m";Wmlib:module{run:fn(n:int);};sys:Sys;snarfbuf,out:string;zclient:Client;client,allocated:ref Client;run(n:int){sys=load Sys Sys->PATH;clienttest(n);snarfbuf="fallback";out=snarfget();}'''+CLIENT+'''clienttest(n:int){c:=ref zclient;c.ptrpid=n;c.kbdpid=-1;c.ctlpid=2147483647;c.dir="private";fd:=sys->open(c.dir+"/pointer",Sys->OREAD);if(fd!=nil)c.ctlfd=fd;client=c;d:=ref Client;d.dir="allocated-client";allocated=d;}'''+re.search(r'(?m)^snarfget\([^\n]*\n\{.*?\n\}',(ROOT/'appl/lib/wmlib.b').read_text(),re.S)[0]
def generate(limbo,w):
 m=w/'interfaces.m';m.write_text('include "sys.m";Wmlib:module{run:fn(n:int);};');frames={};sigs={}
 for name in('Sys','Wmlib'):
  h=subprocess.check_output([str(limbo),'-H','-M',name,'-I',str(ROOT/'module'),str(m)],text=True)
  for fn,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+r'_(\w+) (0x[0-9a-f]+)U',h,re.M):
   size=re.search(r'^#define F_'+name+'_'+fn+r'_size (\d+)',h,re.M);mp=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M)
   if size and mp:frames[(fn,int(sig,16))]=(int(size[1]),[int(x,0)for x in mp[1].split(',')if x])
   if name=='Wmlib':sigs[fn]=int(sig,16)
 rows=G['generate'](limbo,w,SOURCE,'Wmlib',(3,1,0,'~0U'),import_frames=frames,export_signatures=sigs,source_label='original Wmlib snarfget fixture')
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={x[0]:x[1]for x in rows[0]['globals']};p.write_text(s+'\nstatic const uint snarf_out='+str(gl['out'])+';\nstatic const uint client_out='+str(gl['client'])+', allocated_out='+str(gl['allocated'])+';\n')
 B['unit'].__globals__['OP_NAMES']={n[1:].lower()for n,op in E['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];count=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(153,154,155,156,159,130,68):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,tables,rows=records)
    except(AssertionError,KeyError,IndexError,ValueError,StopIteration):count+=1
    else:raise AssertionError(('unchecked byte call setup',row['name'],pc,k))
 print('PASS Wmlib snarf source oracle:',count,'instruction corruptions detected',flush=True)
 for badsource in(SOURCE.replace('Client','Other'),SOURCE.replace('Wmlib','Foreign'),SOURCE.replace('ptrpid','otherpid'),SOURCE.replace('Sys->Rwrite','chan of (int,int)')):
  _,badrows=E['compile_source'](limbo,w,badsource,interface='',loads=True);assert not next(r for r in badrows if r['name']=='clienttest')['accepted'],badrows
 print('PASS private Client boundary: foreign names/modules, fields and request payload refused',flush=True)
def unit():
 src=B['unit']().split('\n#line 1 "native64-bufio-library.c"')[0]
 handlers=''.join('optab['+name+']=actual_'+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+';'for name in E['OPCODES']if 'OP('+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+')'in src)
 return src+'\n#line 1 "native64-wmlib-snarf.c"\n'+(ROOT/'utils/tests/native64-wmlib-snarf.c').read_text().replace(' SNARF_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='wmlib-snarf-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=240)
  headers=('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-library-code.h','native64-library-layout.h')
  for name,before,after,predicate in(
   ('native64-typed-multi.h','else p->s.imm=s->s;p->reg=native64librarymiddle(s);','else p->s.imm=0;p->reg=native64librarymiddle(s);','original snarf completion'),
   ('native64-library-layout.h','id==12?0x3c','id==12?0x38','constructor baseline')):
   for h in headers:(w/h).write_text((ROOT/'include'/h).read_text())
   header=w/name;text=header.read_text();assert text.count(before)==1;header.write_text(text.replace(before,after));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=240)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stderr[-1500:])
   for h in headers:(w/h).unlink()
   print('PASS Wmlib snarf active control:',predicate,flush=True)
if __name__=='__main__':main()
