#!/usr/bin/env python3
"""Execute a computed String-list head directly in imported call arguments."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-alt-pairs.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
PREFIX='include "sys.m";include "draw.m";'
PROVIDER='Sink:module{take:fn(s:string):int;pair:fn(a:string,n:int,b:string):int;};'
NAMES=('one','two','global','repeat','reuse','pair')
DECL='Headarg:module{'+''.join(n+':fn(xs:list of string);'for n in NAMES)+'open:fn(path:string);};'
SOURCE='implement Headarg;'+PREFIX+PROVIDER+DECL+'sink:Sink;hold:list of string;holdp:ref Draw->Pointer;out:int;'
SOURCE+='one(xs:list of string){v:=sink->take(hd tl xs);out=v;}two(xs:list of string){v:=sink->take(hd tl tl xs);out=v;}global(xs:list of string){hold=xs;v:=sink->take(hd tl hold);out=v;}repeat(xs:list of string){a:=sink->take(hd tl xs);b:=sink->take(hd tl tl xs);out=a+b;}'
SOURCE+='reuse(xs:list of string){p:=ref Draw->Pointer(0,(1,2),3);holdp=p;if(tl xs==nil)out=1;else out=2;}pair(xs:list of string){v:=sink->pair(hd tl xs,17,hd tl tl xs);out=v;}open(path:string){sink=load Sink path;}'
def generate(limbo,w):
 m=w/'headarg.m';m.write_text(PREFIX+PROVIDER+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Headarg','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Headarg_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 header=subprocess.check_output([str(limbo),'-H','-M','Sink','-I',str(ROOT/'module'),str(m)],text=True);(w/'sink-native.h').write_text(header);frames={}
 for n in('take','pair'):
  size=int(re.search(r'^#define F_Sink_'+n+r'_size (\d+)',header,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Sink_'+n+r'_map \{([^}]*)\}',header,re.M)[1].split(',')if v];sig=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Sink_'+n+r' (0x[0-9a-f]+)U',header,re.M)[1],16);frames[(n,sig)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'Headarg',(7,1,0,'~0U'),import_frames=frames,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(s+'\nstatic const uint head_globals[]={'+','.join(str(gl[n])for n in('sink','hold','holdp','out'))+'};\n')
 reuse=next(r for r in rows if r['name']=='reuse');tail=next(c for c in reuse['code']if c[0]==E['OPCODES']['ITAIL']);leaf=next(l for l in reuse['frame']['leaves']if l[1]==tail[4]);assert leaf[4]!=10,leaf
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]!=252:continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('String head argument oracle missed',row['name'],k))
 print('PASS head argument oracle:',controls,'corruptions; real reused-root nil comparison and unchanged classic Dis',flush=True)
 for n in(32,33):
  source='implement Closed;'+PROVIDER+'Closed:module{run:fn(xs:list of string);};sink:Sink;out:int;run(xs:list of string){v:=sink->take(hd '+'tl '*n+'xs);out=v;}'
  _,closed=E['compile_source'](limbo,w,source,interface='',loads=True);assert closed[0]['accepted']==(n==32),closed
 print('PASS head argument boundary: 32 tails accepted; 33 refused',flush=True)
 _,records=E['compile_source'](limbo,w,(ROOT/'appl/wm/awm.b').read_text(),interface='',loads=True)
 for name in('init','handlerequest'):
  row=next(r for r in records if r['name']==name);print('AWM',name,':',row['accepted'],row.get('pc'),row.get('reason'),flush=True)
def unit():
 src=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text()
 for sig in('static void\nnewrecord(','OP(new)','OP(newz)'):src+='\n'+extract(vm,sig)+'\n'
 return src+'\n#line 1 "native64-head-args.c"\n'+(ROOT/'utils/tests/native64-head-args.c').read_text().replace(' HEAD_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='head-args-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wrong head source','native64-typed-multi.h','if(s->op==IHEADP || s->op==ITAIL && UDST(s->mode)==(AFP|AIND)){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}', 'if(s->op==IHEADP || s->op==ITAIL && UDST(s->mode)==(AFP|AIND)){p->s.ind=64;p->d.i.f=40;p->d.i.s=s->d;}','exact String head and identity'),
   ('missing temporary clear','native64-typed-multi.h','else if(s->returned==252){if(s->op==IHEADP || s->op==ITAIL && UDST(s->mode)==(AFP|AIND)){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else native64typedcode(p,1,s,1,fpbytes,mpbytes);}', 'else if(s->returned==252){if(s->op==IHEADP || s->op==ITAIL && UDST(s->mode)==(AFP|AIND)){p->s.ind=s->s;p->d.i.f=40;p->d.i.s=s->d;}else{native64typedcode(p,1,s,1,fpbytes,mpbytes);if(s->op==IMOVP){p->add=SRC(AFP)|DST(AFP);p->s.ind=s->d;}}}','tail temporary cleared before dispatch'),
   ('missing tail source root','native64-typed-module.h','if(!native64typedheadprep(step,f->bytes,s->databytes))return 0;\n                map=USRC(step->mode)==AFP?f->map:s->datamap;bit=step->s/8;if(!(map[bit/8]&(128>>(bit%8))))return 0;', 'if(!native64typedheadprep(step,f->bytes,s->databytes))return 0;\n                map=USRC(step->mode)==AFP?f->map:s->datamap;bit=step->s/8;if(0)return 0;','malformed head argument metadata refused'))
  for name,header,before,after,predicate in controls:
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS head argument active control:',name,flush=True)
if __name__=='__main__':main()
