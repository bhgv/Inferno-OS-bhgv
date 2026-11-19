#!/usr/bin/env python3
"""Execute five checked Wmcontext ALT channels and managed reference payloads."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ALT=runpy.run_path(str(ROOT/'utils/tests/native64-alt.py'));G=ALT['G'];CMD=ALT['CMD'];E=ALT['E']
PREFIX='include "sys.m";include "draw.m";'
FIELDS=('kbd','ptr','ctl','wctl','images');KINDS=('int','ref Draw->Pointer','string','string','ref Draw->Image')
def args(k):return 'w:ref Draw->Wmcontext,v:'+k+',c:chan of '+k
DECL='Wmselect:module{'+''.join(n+('mp'if mp else'')+':fn('+args(k)+');'for mp in(0,1)for n,k in zip(FIELDS,KINDS))+'directptr:fn('+args('ref Draw->Pointer')+');directimage:fn('+args('ref Draw->Image')+');inspect:fn('+args('int')+');clear:fn();};'
SOURCE='implement Wmselect;'+PREFIX+DECL+'count,choice,wordout:int;saved:string;pout:ref Draw->Pointer;iout:ref Draw->Image;stored:ref Draw->Wmcontext;'
SENDERS=('sendw','sendp','sends','sends','sendi')
for mp in(0,1):
 for n,k,sender in zip(FIELDS,KINDS,SENDERS):SOURCE+=n+('mp'if mp else'')+'('+args(k)+'){'+('stored=w;spawn selectmp();'if mp else'spawn selectwm(w);')+'spawn '+sender+'(c,v);}'
SOURCE+='directptr('+args('ref Draw->Pointer')+'){spawn selectp(c);spawn sendp(c,v);}directimage('+args('ref Draw->Image')+'){spawn selecti(c);spawn sendi(c,v);}'
SOURCE+='inspect('+args('int')+'){selectwm(w);}clear(){saved=nil;pout=nil;iout=nil;stored=nil;}'
for mp in(0,1):
 SOURCE+=('selectmp(){'if mp else'selectwm(w:ref Draw->Wmcontext){')+'alt{'+''.join('v:= <-'+('stored'if mp else'w')+'.'+n+' => '+out+'=v;choice='+str(i+1)+';count++;'for i,(n,out)in enumerate(zip(FIELDS,('wordout','pout','saved','saved','iout'))))+'}}'
for suffix,kind,out,n in(('p','ref Draw->Pointer','pout',2),('i','ref Draw->Image','iout',5)):
 SOURCE+='select'+suffix+'(c:chan of '+kind+'){alt{'+''.join('v:= <-c => '+out+'=v;choice='+str(n)+';count++;'for _ in range(2))+'}}'
for n,k in(('sendw','int'),('sendp','ref Draw->Pointer'),('sends','string'),('sendi','ref Draw->Image')):SOURCE+=n+'(c:chan of '+k+',v:'+k+'){c <-=v;}'
def generate(limbo,w):
 m=w/'wmselect.m';m.write_text(PREFIX+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Wmselect','-I',str(ROOT/'module'),str(m)],text=True);canonical={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Wmselect_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Wmselect',(22,0,0,'~0U'),export_signatures=canonical);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as o:o.write('static const uint wm_globals[]={'+','.join(str(gl[n])for n in('count','choice','wordout','saved','pout','iout'))+'};\n')
 types=w/'draw-types.m';types.write_text(PREFIX);(w/'wm-native.h').write_text(subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(types)],text=True))
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in(189,190,193):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Wmcontext ALT oracle missed',r['name'],pc,k))
 print('PASS Wmcontext ALT source:',controls,'corruptions detected, full classic opcode/operand/frame/table/export proof and unchanged bytes',flush=True)
def boundaries(limbo,w):
 fake='Draw:module{Pointer:adt{n:int;};Image:adt{n:int;};Context:adt{n:int;};Wmcontext:adt{kbd:chan of string;ptr:chan of ref Pointer;ctl,wctl:chan of string;images:chan of ref Image;connfd:ref Sys->FD;ctxt:ref Context;};};'
 source='implement Wrong;include "sys.m";'+fake+'Wrong:module{run:fn(w:ref Draw->Wmcontext);};run(w:ref Draw->Wmcontext){alt{v:= <-w.kbd => ;}}'
 _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted']and rows[0]['reason']=='alt-field-channel',rows
 source='implement Wrong;'+PREFIX+'Other:adt{kbd:chan of int;};Wrong:module{run:fn(w:ref Other);};run(w:ref Other){alt{v:= <-w.kbd => ;}}'
 _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted']and rows[0]['reason']=='alt-field-channel',rows
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in rows if r['name']=='init');assert not init['accepted']and init['pc']>149,init;assert next(r for r in rows if r['name']=='command')['accepted']
 print('PASS Wmcontext boundary: same namespace/layout with wrong channel payload and foreign owner refused; real AWM init:',init['pc'],init['reason'],flush=True)
def unit():
 src=ALT['unit']().split('\n#line 1 "native64-alt.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-wmcontext-alt.c"\n'+(ROOT/'utils/tests/native64-wmcontext-alt.c').read_text().replace(' WM_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent Wmcontext ALT oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='wmcontext-alt-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicates in (
   ('field-selection','native64-typed-code.h','p->s.i.s=s->returned==192?64:s->m;','p->s.i.s=s->returned==192?64:s->m==16?24:s->m;',('all Wmcontext ALT peers resumed','exact Wmcontext selected guard and count')),
   ('field-payload-contract','native64-typed-alt.h','(c->returned==193 && p->m!=(c->m==0?4:24))','0',('malformed Wmcontext ALT metadata refused',))):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and any(p in r.stderr for p in predicates)and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS Wmcontext active control:',name,flush=True)
if __name__=='__main__':main()
