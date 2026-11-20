#!/usr/bin/env python3
"""Execute checked reference identity branches through canonical ADT fields."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";include "wmsrv.m";'
# Owner, field, value type, classic/native field offsets, family tag.
FIELDS=[('Draw->Image','display','Draw->Display',44,48,1),('Draw->Image','screen','Draw->Screen',48,56,1),('Draw->Screen','image','Draw->Image',4,8,1),('Draw->Screen','fill','Draw->Image',8,16,1),('Draw->Screen','display','Draw->Display',12,24,1)]
FIELDS += [('Draw->Display',n,'Draw->Image',4*i,8*i,1)for i,n in enumerate(('image','white','black','opaque','transparent'))]
FIELDS += [('Wmclient->Window',n,t,o,z,2)for n,t,o,z in(('display','Draw->Display',0,0),('image','Draw->Image',20,24),('ctxt','Draw->Wmcontext',40,48),('titlebar','Tk->Toplevel',56,72),('screen','Draw->Screen',84,104))]
FIELDS += [('Draw->Context','display','Draw->Display',0,0,3),('Draw->Context','screen','Draw->Screen',4,8,3),('Wmsrv->Client','znext','Wmsrv->Client',28,56,4),('Wmsrv->Client','wmctxt','Draw->Wmcontext',48,88,4)]
TYPES=tuple(sorted({a for a,_,b,*_ in FIELDS}|{b for _,_,b,*_ in FIELDS}))
DECL='Refbranch:module{'+''.join('test'+str(i)+'v'+str(v)+':fn(o:ref '+owner+',x:ref '+value+');'for i,(owner,_,value,*_)in enumerate(FIELDS)for v in range(4))+'};'
SOURCE='implement Refbranch;'+PREFIX+DECL+'equal,unequal,seen,guard:int;'
for i,(owner,field,value,*_)in enumerate(FIELDS):SOURCE+='holder'+str(i)+':ref '+owner+';other'+str(i)+':ref '+value+';'
for i,(owner,field,value,*_)in enumerate(FIELDS):
 for v in range(4):
  left=('holder'+str(i)if v&1 else'o')+'.'+field;right='other'+str(i)if v&2 else'x'
  SOURCE+='test'+str(i)+'v'+str(v)+'(o:ref '+owner+',x:ref '+value+'){holder'+str(i)+'=o;other'+str(i)+'=x;if('+left+'=='+right+')equal=11;else equal=22;if('+right+'!='+left+')unequal=33;else unequal=44;seen++;if(guard== -99)seen=99;}'
def generate(limbo,w):
 interface=w/'refbranch.m';interface.write_text(PREFIX+DECL);header=subprocess.check_output([str(limbo),'-H','-M','Refbranch','-I',str(ROOT/'module'),str(interface)],text=True)
 sigs={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Refbranch_(\w+) (0x[0-9a-f]+)U',header,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Refbranch',(76,0,0,'~0U'),export_signatures=sigs);p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''));gl={g[0]:g[1]for g in rows[0]['globals']}
 with p.open('a')as out:
  out.write('static const uint branch_globals[]={'+','.join(str(gl[n])for n in('equal','unequal','seen','guard'))+'};\n')
  out.write('static const uint branch_fields[][4]={'+','.join('{'+','.join(map(str,(TYPES.index(a),TYPES.index(t),native,family)))+'}'for a,_,t,_,native,family in FIELDS)+'};\n')
  out.write('static const uint branch_holdings[][2]={'+','.join('{'+str(gl['holder'+str(i)])+','+str(gl['other'+str(i)])+'}'for i in range(19))+'};\n')
 headers={m:subprocess.check_output([str(limbo),'-H','-M',m,'-I',str(ROOT/'module'),str(interface)],text=True)for m in('Draw','Tk','Wmclient','Wmsrv')}
 with(w/'ref-native.h').open('w')as out:
  out.write(header+'\n')
  for t in TYPES:
   module,name=t.split('->');c=module+'_'+name;definition=next(l for l in headers[module].splitlines()if l.startswith('#define '+c+'_map '));out.write(definition+'\nstatic const uchar map_'+c+'[]='+c+'_map;\n')
  out.write('static const struct{uint bytes,np;const uchar*map;} branch_types[]={'+','.join('{sizeof('+t.replace('->','_')+'),sizeof(map_'+t.replace('->','_')+'),map_'+t.replace('->','_')+'}'for t in TYPES)+'};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  idx=int(re.fullmatch(r'test(\d+)v\d',row['name'])[1])
  for pc,c in enumerate(row['code']):
   if c[5]!=207:continue
   assert c[3]>>24==FIELDS[idx][5]and(c[3]>>16)&255==FIELDS[idx][4]
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('field branch oracle missed',row['name'],pc,k))
 print('PASS field reference source:',controls,'corruptions detected; canonical types/signatures, independent classic operands/branches and unchanged bytes',flush=True)
def boundaries(limbo,w):
 for declaration,body in(('Other:adt{image:ref Draw->Image;};','run(o:ref Other,x:ref Draw->Image){if(o.image==x); }'),('','run(o:ref Draw->Context,x:chan of (string,chan of (string,ref Draw->Wmcontext))){if(o.wm==x); }')):
  args=body[3:body.index('){')];source='implement Closed;'+PREFIX+declaration+'Closed:module{run:fn'+args+');};'+body
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True);init=next(r for r in rows if r['name']=='init');assert init['accepted']or init['pc']>178,init;assert next(r for r in rows if r['name']=='command')['accepted']
 print('PASS field reference boundaries; real AWM init:',init.get('pc','complete'),init.get('reason','accepted'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-field-reference.c"\n'+(ROOT/'utils/tests/native64-field-reference.c').read_text().replace(' BRANCH_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('independent branch oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='field-reference-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);boundaries(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('truncated reference branch','native64-typed-code.h','if(s->returned==207){p->reg=s->m&65535;', 'if(s->returned==207){p->reg=s->m&65535;p->op=s->op==IBEQL?IBEQW:IBNEW;','full 64-bit reference comparison'),
   ('wrong reference field','native64-typed-code.h','p->s.i.s=(s->m>>16)&255;', 'p->s.i.s=(s->m>>16)&255;if((s->m>>24)==1 && p->s.i.s==48)p->s.i.s=56;','exact reference identity branches and Word guard'),
   ('missing peer root proof','native64-typed-module.h','offsets[1]=step->m&65535;\n            for(k=0;k<2;k++)', 'offsets[1]=step->m&65535;\n            for(k=0;k<1;k++)','malformed field reference metadata refused')):
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h'):(w/parent).unlink()
   print('PASS field reference active control:',name,flush=True)
if __name__=='__main__':main()
