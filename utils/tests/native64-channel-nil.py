#!/usr/bin/env python3
"""Execute full-width channel nil checks and canonical Client channel fields."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
TYPES=('int','string','(int,string)','ref Draw->Pointer')
FIELDS=('kbd','ptr','ctl','stop','images','wins')
NAMES=tuple('f'+str(i)for i in range(48))
def argtype(i):return 'ref Wmsrv->Client'if i>=24 else'chan of '+TYPES[i//4 if i<16 else(i-16)//2]
DECL='Channelnil:module{'+''.join(n+':fn(c:'+argtype(i)+');'for i,n in enumerate(NAMES))+'};'
SOURCE='implement Channelnil;'+PREFIX+DECL+''.join('slot'+str(i)+':chan of '+t+';'for i,t in enumerate(TYPES))+'holder:ref Wmsrv->Client;out:int;'
for i,n in enumerate(NAMES):
 if i<16:slot='slot'+str(i//4);value='c'if i%4<2 else slot;head=slot+'=c;'
 elif i<24:value='slot'+str((i-16)//2);head=''
 else:value=('c'if(i-24)%4<2 else'holder')+'.'+FIELDS[(i-24)//4];head='holder=c;'
 SOURCE+=n+'(c:'+argtype(i)+'){'+head+'if('+value+('==nil'if i%2==0 else'!=nil')+')out=1;else out=2;}'
def generate(limbo,w):
 m=w/'channelnil.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Channelnil','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Channelnil_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Channelnil',(48,0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint channel_globals[]={'+','.join(str(gl[n])for n in('slot0','slot1','slot2','slot3','holder','out'))+'};\n')
 (w/'client-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;roles=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(239,241,251):continue
   roles.add(c[5])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Channel nil oracle missed',row['name'],k))
 assert roles=={239,241,251},roles
 print('PASS Channel nil oracle:',controls,'corruptions detected; mapped pointer-width channel/nil and Client fields, unchanged bytes',flush=True)
 for source in('implement Closed;Closed:module{run:fn(a,b:chan of int);};out:int;run(a,b:chan of int){if(a==b)out=1;}', 'implement Closed;'+PREFIX+'Closed:module{run:fn(c:ref Wmsrv->Client);};out:int;run(c:ref Wmsrv->Client){if(c.wmctxt==nil)out=1;}'):
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in records if r['name']=='init');assert init['accepted']or init['pc']>257,init;print('AWM init:',init.get('pc'),init.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-channel-nil.c"\n'+(ROOT/'utils/tests/native64-channel-nil.c').read_text().replace(' PARSE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='channel-nil-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,before,after,predicate in (
   ('truncated pointer compare','p->op=s->op;','p->op=s->op==IBEQL?IBEQW:s->op==IBNEL?IBNEW:s->op;','full-width channel nil comparison'),
   ('wrong stop offset','p->s.i.s=s->m>>16;','p->s.i.s=(s->m>>16)==24?0:s->m>>16;','full-width channel nil comparison'),
   ('wrong wins offset','p->s.i.s=s->m>>16;','p->s.i.s=(s->m>>16)==48?56:s->m>>16;','full-width channel nil comparison')):
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   header=w/'native64-typed-code.h';text=header.read_text();assert text.count(before)==(3 if name=='truncated pointer compare'else 1);header.write_text(text.replace(before,after));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Channel nil active control:',name,flush=True)
if __name__=='__main__':main()
