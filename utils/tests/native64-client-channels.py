#!/usr/bin/env python3
"""Execute exact Wmsrv.Client fields and blocking channel transfers in C/JIT."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";'
FIELDS=('kbd','ptr','ctl','stop');KINDS=('int','ref Draw->Pointer','string','int')
ARGS='c:ref Wmsrv->Client,p:ref Draw->Pointer,s:string,w:int'
NAMES=tuple(n+suffix for suffix in('','mp','reverse')for n in FIELDS)+('kbdimm','stopimm')
DECL='Clienttest:module{'+''.join(n+':fn('+ARGS+');'for n in NAMES)+'inspect:fn(c:ref Wmsrv->Client);clear:fn();};'
STORED=('savedkbd','savedptr','savedctl','savedstop','savedcursor','savedwins','savednext','savedimages','savedcontext')
SOURCE='implement Clienttest;'+PREFIX+DECL+'stored:ref Wmsrv->Client;valuep,pout:ref Draw->Pointer;values,sout:string;valuew,wout:int;received,sent:int;savedkbd,savedstop:chan of int;savedptr:chan of ref Draw->Pointer;savedctl:chan of string;savedcursor:string;savedwins:list of ref Wmsrv->Window;savednext:ref Wmsrv->Client;savedimages:chan of (ref Draw->Point,ref Draw->Image,chan of int);savedcontext:ref Draw->Wmcontext;flags,id,fid,token:int;'
for i,name in enumerate(NAMES):
 field=FIELDS[i%4]if i<12 else'kbd'if i==12 else'stop';suffix='mp'if 4<=i<8 else'';receiver='recv'+field+suffix+('()'if suffix else'(c)');sender='send'+field+('imm'if i>=12 else suffix)+('()'if suffix else'(c)')
 SOURCE+=name+'('+ARGS+'){prepare(c,p,s,w);spawn '+(sender if 8<=i<12 else receiver)+';spawn '+(receiver if 8<=i<12 else sender)+';}'
SOURCE+='inspect(c:ref Wmsrv->Client){wout= <-c.kbd;}prepare('+ARGS+'){stored=c;valuep=p;values=s;valuew=w;project(c);}project(c:ref Wmsrv->Client){'
for name,field in zip(STORED,('kbd','ptr','ctl','stop','cursor','wins','znext','images','wmctxt')):SOURCE+=name+'=c.'+field+';c.'+field+'='+name+';'
for field in('flags','id','fid','token'):SOURCE+=field+'=c.'+field+';c.'+field+'='+field+';'
SOURCE+='}clear(){stored=nil;valuep=nil;pout=nil;values=nil;sout=nil;'+''.join(n+'=nil;'for n in STORED)+'}'
for suffix in('','mp'):
 for field,kind,out,value in zip(FIELDS,KINDS,('wout','pout','sout','wout'),('valuew','valuep','values','valuew')):
  args='()'if suffix else'(c:ref Wmsrv->Client)';owner='stored'if suffix else'c'
  SOURCE+='recv'+field+suffix+args+'{'+out+'= <-'+owner+'.'+field+';received++;}'
  SOURCE+='send'+field+suffix+args+'{'+owner+'.'+field+' <-='+value+';sent++;}'
for field in('kbd','stop'):SOURCE+='send'+field+'imm(c:ref Wmsrv->Client){c.'+field+' <-= -17;sent++;}'
def generate(limbo,w):
 interface=w/'clienttest.m';interface.write_text(PREFIX+DECL);h=subprocess.check_output([str(limbo),'-H','-M','Clienttest','-I',str(ROOT/'module'),str(interface)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Clienttest_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Clienttest',(36,0,0,'~0U'),export_signatures=signatures);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:out.write('static const uint client_globals[]={'+','.join(str(gl[n])for n in('wout','pout','sout','received','sent',*STORED,'flags','id','fid','token'))+'};\n')
 types=w/'types.m';types.write_text(PREFIX)
 for module in('Wmsrv','Draw'):(w/(module.lower()+'-native.h')).write_text(subprocess.check_output([str(limbo),'-H','-M',module,'-I',str(ROOT/'module'),str(types)],text=True))
 draw=(w/'draw-native.h').read_text();definition=next(line for line in draw.splitlines()if line.startswith('#define Draw_Wmcontext_map '))
 with(w/'wmsrv-native.h').open('a')as out:out.write('\n'+definition+'\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(204,205,206):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Client oracle missed',row['name'],pc,k))
 print('PASS Client source:',controls,'corruptions detected; independent classic fields/transfers/maps/tables/hash and unchanged bytes',flush=True)
def boundaries(limbo,w):
 source='implement Closed;'+PREFIX+'Other:adt{ctl:chan of string;};Closed:module{run:fn(c:ref Other,s:string);};run(c:ref Other,s:string){c.ctl <-=s;}'
 _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 original=(ROOT/'module/wmsrv.m').read_text()
 for before,after in(('flags:\tint;','flags:\tbig;'),('ctl:\t\tchan of string;','ctl:\t\tchan of int;'),('images:\tchan of (ref Draw->Point, ref Draw->Image, chan of int);','images:\tchan of (ref Draw->Point, ref Draw->Image, chan of string);')):
  assert before in original;(w/'badwmsrv.m').write_text(original.replace(before,after,1));source='implement Closed;include "sys.m";include "draw.m";include "badwmsrv.m";Closed:module{run:fn(c:ref Wmsrv->Client);};run(c:ref Wmsrv->Client){c.stop <-=1;}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in rows if r['name']=='init');assert not init['accepted']and init['pc']>169,init;assert next(r for r in rows if r['name']=='command')['accepted']
 print('PASS Client boundaries; real AWM init:',init['pc'],init['reason'],flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-client-channels.c"\n'+(ROOT/'utils/tests/native64-client-channels.c').read_text().replace(' CLIENT_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent Client oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='client-channels-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wide Client Word store','native64-typed-code.h','if(s->returned==205 || s->returned==206){p->reg=0;', 'if(s->returned==205 || s->returned==206){p->reg=0;if(s->returned==206 && s->m==32 && (UDST(s->mode)&AIND))p->op=IMOVL;','all Client fields and padding preserved'),
   ('wrong Client channel field','native64-typed-code.h','p->d.i.s=s->m>>8;', 'p->d.i.s=s->m>>8;if((s->m>>8)==0)p->d.i.s=24;','all Client channel peers resumed and retired'),
   ('missing scalar GC proof','native64-typed-module.h','root=!!(modes[k]&AIND) || step->returned==205;map=', 'root=!!(modes[k]&AIND) || step->returned==205;if(!root)continue;map=','malformed Client metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS Client active control:',name,flush=True)
if __name__=='__main__':main()
