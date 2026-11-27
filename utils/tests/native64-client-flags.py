#!/usr/bin/env python3
"""Execute canonical Client.flags AND immediate as Word32."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
MASKS=(1,2,3,-2,32767,-32768,8)
NAMES=tuple('f'+str(i)for i in range(56))
DECL='Clientflags:module{'+''.join(n+':fn(p:ref Wmsrv->Client);'for n in NAMES)+'};'
SOURCE='implement Clientflags;'+PREFIX+DECL+'stored:ref Wmsrv->Client;out,after:int;'
for i,n in enumerate(NAMES):
 mode=i%28//7;owner='stored'if mode%2 else'p';head='stored=p;'if mode%2 else'';mask=str(MASKS[i%7]);expr=owner+'.flags & '+mask
 if i<28:body='n:='+owner+'.id;guard:=n;n='+expr+';out=n;after=guard;'if mode<2 else'out='+expr+';'
 else:body='n:='+owner+'.id;guard:=n;'+owner+'.flags|='+mask+';n='+owner+'.flags;out=n;after=guard;'if mode<2 else owner+'.flags|='+mask+';out='+owner+'.flags;'
 SOURCE+=n+'(p:ref Wmsrv->Client){'+head+body+'}'
def generate(limbo,w):
 m=w/'clientflags.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Clientflags','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Clientflags_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Clientflags',(56,0,0,'~0U'),export_signatures=signatures)
 read=rows[0];step=next(c for c in read['code']if c[5]==234);assert any(c[0]==E['OPCODES']['IMOVW']and c[2]==step[4]and c[4]==step[4]+4 for c in read['code']),read
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')+'\nstatic const uint field_globals[]={'+','.join(str(gl[n])for n in('stored','out','after'))+'};\n')
 (w/'client-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0;modes=set()
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(234,236):continue
   modes.add(c[1])
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Client flags oracle missed',row['name'],k))
 assert len(modes)==6,modes
 print('PASS Client flags oracle:',controls,'corruptions detected; exact Word32 opcode/flags/mask, maps and unchanged bytes',flush=True)
 for ty,extra in(('Other','Other:adt{flags:int;};'),('Fake->Client','Fake:module{Client:adt{flags:int;};};')):
  source='implement Closed;'+PREFIX+extra+'Closed:module{run:fn(p:ref '+ty+');};out:int;run(p:ref '+ty+'){n:=p.flags&2;out=n;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 for expr in('p.flags&m','p.flags|2','p.id&2'):
  source='implement Closed;'+PREFIX+'Closed:module{run:fn(p:ref Wmsrv->Client,m:int);};out:int;run(p:ref Wmsrv->Client,m:int){n:='+expr+';out=n;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 original=(ROOT/'module/wmsrv.m').read_text()
 for old,new in(('fid:\t\tint;','fid:\t\tbig;'),('cursor:\tstring;','cursor:\tarray of byte;')):
  assert old in original;(w/'badwmsrv.m').write_text(original.replace(old,new,1))
  source='implement Closed;'+PREFIX.replace('include "wmsrv.m";','include "badwmsrv.m";')+'Closed:module{run:fn(p:ref Wmsrv->Client);};out:int;run(p:ref Wmsrv->Client){n:=p.flags&2;out=n;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert not closed[0]['accepted'],closed
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in records if r['name']=='init');focus=next(r for r in records if r['name']=='setfocus');assert focus['accepted']or focus['pc']>1210,focus;assert next(r for r in records if r['name']=='reshaped')['accepted'];print('AWM init:',init.get('pc'),init.get('reason'),'; setfocus:',focus.get('pc'),focus.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-client-flags.c"\n'+(ROOT/'utils/tests/native64-client-flags.c').read_text().replace(' PARSE_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='client-flags-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);source=unit();(w/'unit.c').write_text(source);arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong flags offset','native64-typed-code.h','if(s->returned==234){p->reg=(short)(s->m&65535);p->s.i.f=s->s;p->s.i.s=32;', 'if(s->returned==234){p->reg=(short)(s->m&65535);p->s.i.f=s->s;p->s.i.s=72;','exact flags Word32 result'),
   ('lost mask sign','native64-typed-code.h','p->reg=(short)(s->m&65535);p->s.i.f=s->s;', 'p->reg=s->m&32767;p->s.i.f=s->s;','exact flags Word32 result'),
   ('missing flags owner map','native64-typed-module.h','step=f->code+j;if(!native64typedflagsand(step,f->bytes,s->databytes))return 0;\n            map=(USRC(step->mode)&~AIND)==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;', 'step=f->code+j;if(!native64typedflagsand(step,f->bytes,s->databytes))return 0;','malformed Client flags metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Client flags active control:',name,flush=True)
  extract=CMD['NS']['S']['FILES']['FILES']['function']
  for op,predicate in(('andw','adjacent local Word32 preserved'),('orw','Client payload preserved')):
   body=extract(source,'OP('+op+')');bad=body.replace('UWORD left, right, result;','UWORD left, right, result;LONG wide;').replace('memmove(R.d, &result, sizeof(result));','wide=(WORD)result;memmove(R.d, &wide, sizeof(wide));');assert bad!=body;(w/'unit.c').write_text(source.replace(body,bad,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(op,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   print('PASS Client flags active control: eight-byte '+op+' store detected',flush=True)
if __name__=='__main__':main()
