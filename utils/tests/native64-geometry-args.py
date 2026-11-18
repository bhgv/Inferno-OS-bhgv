#!/usr/bin/env python3
"""Pass Draw.Point/Rect across real checked Limbo imports through C/JIT."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
PREFIX='include "sys.m";include "draw.m";draw:Draw;Point,Rect:import draw;'
INTERFACE='Geometry:module{rect:fn(r:Rect):int;point:fn(p:Point):int;reshape:fn(w:ref Draw->Context,r:Rect);mixed:fn(w:ref Draw->Context,p:Point,r:Rect,n:big,tag:byte):int;};'
PROVIDER='implement Geometry;'+PREFIX+INTERFACE+'''a,b,c,d,px,py,t:int;last:big;seen:ref Draw->Context;
rect(r:Rect):int{if(r.min.x==777)raise "fail:geometry";a=r.min.x;b=r.min.y;c=r.max.x;d=r.max.y;return a^(b<<1)^(c<<2)^(d<<3);}
point(p:Point):int{if(p.x==777)raise "fail:geometry";a=p.x;b=p.y;c=0;d=0;return a^(b<<1);}
reshape(w:ref Draw->Context,r:Rect){if(r.min.x==777)raise "fail:geometry";seen=w;a=r.min.x;b=r.min.y;c=r.max.x;d=r.max.y;}
mixed(w:ref Draw->Context,p:Point,r:Rect,n:big,tag:byte):int{if(r.min.x==777)raise "fail:geometry";seen=w;t=int tag;px=p.x;py=p.y;a=r.min.x;b=r.min.y;c=r.max.x;d=r.max.y;last=n;return a^(b<<1)^(c<<2)^(d<<3)^px^(py<<1)^t^int n;}
'''
CONSUMER='implement Calls;'+PREFIX+INTERFACE+'''Calls:module{open:fn(path:string);rect:fn(p:Geometry,r:Rect):int;freshrect:fn(p:Geometry,r:Rect):int;point:fn(p:Geometry,v:Point):int;literal:fn(p:Geometry);fields:fn(p:Geometry,a:int,b:int,c:int,d:int);mixed:fn(p:Geometry,w:ref Draw->Context,tag:byte,v:Point,r:Rect,n:big):int;global:fn(p:Geometry):int;};
loaded:Geometry;box:Rect=((1,2),(3,4));
open(path:string){loaded=load Geometry path;}
rect(p:Geometry,r:Rect):int{return p->rect(r);}
freshrect(p:Geometry,r:Rect):int{x:=p->rect(r);return x+7;}
point(p:Geometry,v:Point):int{return p->point(v);}
literal(p:Geometry){p->reshape(nil,((0,0),(100,100)));}
fields(p:Geometry,a:int,b:int,c:int,d:int){p->reshape(nil,((a,b),(c,d)));}
mixed(p:Geometry,w:ref Draw->Context,tag:byte,v:Point,r:Rect,n:big):int{return p->mixed(w,v,r,n,tag);}
global(p:Geometry):int{return p->rect(box);}
'''
def no_variadics(w):
 p=w/'full.h';p.write_text(p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
def generate(limbo,w):
 providers=G['generate'](limbo,w,PROVIDER,'Geometry',(4,0,0,'~0U'));no_variadics(w)
 text=(w/'full.h').read_text().replace('full_','geometry_provider_').replace('fullload','geometryload')
 globals={g[0]:g[1]for g in providers[0]['globals']}
 for n in('a','b','c','d','px','py','t','last','seen'):text+=f'static const uint geometry_{n}={globals[n]};\n'
 (w/'geometry-provider.h').write_text(text)
 m=w/'geometry.m';m.write_text(PREFIX+INTERFACE)
 header=subprocess.check_output([str(limbo),'-H','-M','Geometry','-I',str(ROOT/'module'),str(m)],text=True)
 frames={}
 for r in providers:
  n=r['name'];size=int(re.search(r'^#define F_Geometry_'+n+r'_size (\d+)',header,re.M)[1]);mapping=[int(x,0)for x in re.search(r'^#define F_Geometry_'+n+r'_map \{([^}]*)\}',header,re.M)[1].split(',')if x]
  assert size<=r['frame']['bytes']and all(a[0]+a[3]<=size for a in r['args'])
  assert all(bool(mapping[i//64]&(128>>(i//8%8)))==bool(r['frame']['map'][i//64]&(128>>(i//8%8)))for i in range(0,size,8))
  frames[(n,r['signature'])]=(size,mapping)
 m.write_text(PREFIX+INTERFACE+re.search(r'Calls:module\{.*?\};',CONSUMER)[0])
 callsheader=subprocess.check_output([str(limbo),'-H','-M','Calls','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(value,16)for n,value in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Calls_(\w+) (0x[0-9a-f]+)U',callsheader,re.M)}
 rows=G['generate'](limbo,w,CONSUMER,'Calls',(8,1,0,'~0U'),import_frames=frames,export_signatures=signatures);no_variadics(w)
 with(w/'full.h').open('a')as stream:
  stream.write('static const uint geometry_loaded='+str(next(g[1]for g in rows[0]['globals']if g[0]=='loaded'))+';\n')
  for r in rows:
   leaves=[]
   for index,(offset,kind,oldbytes,width)in enumerate(r['args']):
    leaves +=[(l[1],l[3],l[4])for l in r['frame']['leaves']if offset<=l[1]<offset+width]
   stream.write('static const uint geometry_args_'+r['name']+'[][3]={'+','.join('{'+','.join(map(str,l))+'}'for l in leaves)+'};\n')
  stream.write('static const struct{const uint(*args)[3];uint n;}geometry_arguments[]={'+','.join('{geometry_args_'+r['name']+','+str(len([l for a in r['args']for l in r['frame']['leaves']if a[0]<=l[1]<a[0]+a[3]]))+'}'for r in rows)+'};\n')
 data,records=E['compile_source'](limbo,w,CONSUMER,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for j,e in enumerate(r.get('externals',[])):
   for k,a in enumerate(e.get('aggregate_args',[])):
    for label in('index','bytes','map','leaf','store'):
     bad=copy.deepcopy(r);value=bad['externals'][j]['aggregate_args'][k]
     if label=='index':value['index']+=1
     elif label=='bytes':value['layout']['bytes']+=4
     elif label=='map':value['layout']['map'][0]=128
     elif label=='leaf':value['layout']['leaves'][0][1]+=4
     else:bad['code'][e['first']+1][4]+=4
     try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
     except(AssertionError,IndexError,KeyError,StopIteration):controls+=1
     else:raise AssertionError(('geometry oracle missed',r['name'],label))
 print('PASS geometry oracle:',controls,'aggregate metadata/store corruptions detected; native provider argument prefixes equal generated .m headers',flush=True)
 return rows

def refusals(limbo,w):
 for declaration,kind in(('Other:adt{x:int;y:int;};','Other'),('','Draw->Pointer'),('Other:adt{s:string;n:int;};','Other')):
  source='implement Closed;'+PREFIX+declaration+'G:module{take:fn(x:'+kind+'):int;};Closed:module{run:fn(g:G,x:'+kind+'):int;};run(g:G,x:'+kind+'):int{return g->take(x);}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted']and rows[0]['reason']=='external-geometry-argument',rows
 print('PASS geometry compiler boundary: other Word ADT, Draw.Pointer and managed aggregate refused; ordinary bytes preserved',flush=True)

def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
 extract=CMD['NS']['S']['FILES']['FILES']['function']
 for name in('movm','movb','cvtbw','cvtlw'):src+='\n'+extract((ROOT/'libinterp/xec.c').read_text(),'OP('+name+')')+'\n'
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-geometry-args.c"\n'+(ROOT/'utils/tests/native64-geometry-args.c').read_text().replace(' GEOMETRY_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='geometry-args-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);refusals(a.limbo.resolve(),w)
  (w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('copy-count','native64-typed-multi.h','if(s->op==IADDC || s->op==IMOVM)p->reg=s->m;','if(s->op==IADDC)p->reg=s->m;','exact Word result'),
   ('source-map','native64-typed-module.h','for(j=0;j<(unsigned long)step->m;j+=4){bit=(step->s+j)/8;if(map[bit/8]&(128>>(bit%8)))return 0;}','for(j=0;j<(unsigned long)step->m;j+=4){bit=(step->s+j)/8;if(0)return 0;}','malformed geometry metadata refused'),
   ('target-map','native64-typed-multi.h','if(map[bit/8]&(128>>(bit%8)))return 0;','if(0)return 0;','malformed geometry metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   for parent in('native64-typed-module.h','native64-typed-multi.h'):(w/parent).unlink()
   print('PASS geometry active control:',name,flush=True)

if __name__=='__main__':main()
