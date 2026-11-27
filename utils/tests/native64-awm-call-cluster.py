#!/usr/bin/env python3
"""Execute the unchanged AWM border call cluster and its shared argument forms."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ARRAY=runpy.run_path(str(ROOT/'utils/tests/native64-image-arrays.py'));BASE,G,CMD,E=ARRAY['BASE'],ARRAY['G'],ARRAY['CMD'],ARRAY['E']
AWM=(ROOT/'appl/wm/awm.b').read_text()
PREFIX='include "sys.m";include "draw.m";include "wmsrv.m";include "string.m";'
NAMES=('run','words','fields','tail','bytes','divide')
DECL='Cluster:module{'+''.join(n+':fn(b:array of ref Draw->Image,r:Draw->Rect,show:int);'for n in NAMES)+'open:fn(d,s:string);};'
SOURCE='implement Cluster;'+PREFIX+DECL+'draw:Draw;strings:String;Screen,Image,Rect,Point:import draw;screen:ref Screen;image:ref Image;ptr:ref Draw->Pointer;client:ref Wmsrv->Client;args:list of string;raw:array of byte;outstr:string;seenpoint:Point;seenrect:Rect;cmp:int;'+','.join('w'+str(i)for i in range(10))+':int;'
SOURCE+='Bdwidth:con 3;Sminx,Sminy,Smaxx,Smaxy:con iota;Minx,Miny,Maxx,Maxy:con 1<<iota;'
SOURCE+='run(b:array of ref Image,r:Rect,show:int){showborders(b,r,show);}'
SOURCE+='words(b:array of ref Image,r:Rect,show:int){takewords('+','.join('show'+op+'3'for op in('+','-','*','/','%','&','|','^','<<','>>'))+',((r.min.x+3,r.min.y-3),r.max));}'
SOURCE+='fields(b:array of ref Image,r:Rect,show:int){seenpoint=ptr.xy;seenrect=image.r;cmp=0;'+''.join('if(image.r.min.y'+op+'show)cmp|='+str(1<<i)+';'for i,op in enumerate(('<','<=','>','>=','==','!=')))+'if(client.id!=show)cmp|=64;client.flags &= ~12;client.flags ^= 1;client.flags |= 2;}'
SOURCE+='tail(b:array of ref Image,r:Rect,show:int){outstr=strings->quoted(tl tl args);}'
SOURCE+='bytes(b:array of ref Image,r:Rect,show:int){takestring(b,ptr,client,string raw);}'
SOURCE+='divide(b:array of ref Image,r:Rect,show:int){takesmall(9/show);}takesmall(n:int){cmp=n;}'
for name in('showborders','showborder'):
 SOURCE+=re.search(r'\n'+name+r'\([^)]*\)[^\n]*\n\{.*?\n\}',AWM,re.S)[0]
SOURCE+='takewords('+','.join('v'+str(i)for i in range(10))+':int,r:Rect){'+''.join('w'+str(i)+'=v'+str(i)+';'for i in range(10))+'seenrect=r;}'
SOURCE+='takestring(b:array of ref Image,p:ref Draw->Pointer,c:ref Wmsrv->Client,s:string){outstr=s;}open(d,s:string){draw=load Draw d;strings=load String s;}'
def frames(limbo,w,m,names):
 result={}
 for name in names:
  h=subprocess.check_output([str(limbo),'-H','-M',name,'-I',str(ROOT/'module'),str(m)],text=True)
  (w/(name.lower()+'-native.h')).write_text(h)
  for fn,size in re.findall(r'^#define F_'+name+r'_(\w+)_size (\d+)',h,re.M):
   sig=re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+'_'+fn+r' (0x[0-9a-f]+)U',h,re.M)
   if not sig:continue
   mapping=[int(v,0)for v in re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v]
   result[(fn.replace('_','.'),int(sig[1],16))]=(int(size),mapping)
 return result
def generate(limbo,w):
 m=w/'cluster.m';m.write_text(PREFIX+DECL)
 h=subprocess.check_output([str(limbo),'-H','-M','Cluster','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Cluster_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 imports=frames(limbo,w,m,('Draw','String'))
 rows=G['generate'](limbo,w,SOURCE,'Cluster',(12,2,0,'~0U'),import_frames=imports,export_signatures=signatures)
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={g[0]:g[1]for g in rows[0]['globals']}
 names=('draw','strings','screen','image','ptr','client','args','raw','outstr','seenpoint','seenrect','cmp')+tuple('w'+str(i)for i in range(10))
 offsets={r['name']:i for i,r in enumerate(rows)}
 s+='\nstatic const uint cluster_globals[]={'+','.join(str(gl[n])for n in names)+'};\n'
 s+='static const uint border_function='+str(offsets['showborder'])+';\nstatic const uint borders_function='+str(offsets['showborders'])+';\n';p.write_text(s)
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True)
 (w/'client-native.h').write_text('typedef struct Wmsrv_Client Wmsrv_Client;\n'+re.search(r'struct Wmsrv_Client \{.*?\n\};',h,re.S)[0]+'\n'+next(l for l in h.splitlines()if l.startswith('#define Wmsrv_Client_map '))+'\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(127,130,131,133,236,252):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,tables,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('AWM cluster oracle missed',row['name'],pc,k))
 print('PASS AWM call cluster independent oracle:',controls,'mutations; unchanged border source and classic Dis',flush=True)
 data,records=E['compile_source'](limbo,w,AWM,interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 accepted=[r for r in records if r['accepted']];assert len(accepted)>=19,[(r['name'],r.get('pc'),r.get('reason'))for r in records if not r['accepted']]
 for row in accepted:CMD['verify_nilbranches'](data,row,tables,rows=records)
 print('PASS unchanged AWM:',len(accepted),'of',len(records),'bodies, complete independent operand/call/layout oracles',flush=True)
 for row in records:
  if not row['accepted']:print('AWM next barrier:',row['name'],row['pc'],row['reason'],flush=True)
 return rows
def unit():
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src=ARRAY['unit']().split('\n#line 1 "native64-image-arrays.c"')[0];original=BASE['unit']().split('\n#line 1 "native64-alt-pairs.c"')[0];start=original.index(' optab[IMOVW]=countmovw;');end=original.index(' require(prepare64calls(',start)
 xec=(ROOT/'libinterp/xec.c').read_text()
 for name in('addw','subw','mulw','divw','modw','andw','orw','xorw','shlw','shrw'):
  sig='OP('+name+')'
  if sig in src:src=src.replace(extract(src,sig),'')
  src+='\n'+extract(xec,sig)+'\n'
 sig='OP(cvtac)'
 if sig not in src:src+='\n'+extract((ROOT/'libinterp/string.c').read_text(),sig)+'\n'
 src+='\n#include "draw-native.h"\n'
 for name in('Rect_canon','Rect_size','Point_eq'):
  src+='\n#define F_'+name+' F_Draw_'+name+'\nvoid\n'+extract((ROOT/'libinterp/geom.c').read_text(),name+'(void *fp)')+'\n'
 src='\n'+''.join('void actual_'+n+'(void);\n'for n in('addw','subw','mulw','divw','modw','andw','orw','xorw','shlw','shrw'))+src
 return src+'\n#line 1 "native64-awm-call-cluster.c"\n'+(ROOT/'utils/tests/native64-awm-call-cluster.c').read_text().replace(' CLUSTER_HANDLERS',original[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='awm-cluster-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  controls=(
   ('wrong Image argument slot','native64-typed-multi.h','else{p->s.i.f=56;p->s.i.s=0;p->d.i.f=40;p->d.i.s=s->d;}','else{p->s.i.f=56;p->s.i.s=0;p->d.i.f=40;p->d.i.s=s->d+8;}','exact cluster completion/error'),
   ('wrong computed Word operand','native64-typed-multi.h','else if(s->returned==130 || s->returned==137 || (s->returned==152 && s->op==ICVTWF)){p->s.imm=s->s;p->reg=s->m;','else if(s->returned==130 || s->returned==137 || (s->returned==152 && s->op==ICVTWF)){p->s.imm=s->s+1;p->reg=s->m;','real showborder Image and mixed Rect arguments'),
   ('wrong flags mask','native64-typed-code.h','if(s->returned==153){p->reg=native64librarymiddle(s);','if(s->returned==153){p->reg=native64librarymiddle(s);if(s->op==IXORW && USRC(s->mode)==AIMM)p->s.imm=s->s^1;','Image comparisons Client.id and in-place flags'))
  parents=('native64-typed-code.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-tables.h')
  for name,header,before,after,predicate in controls:
   for parent in parents:(w/parent).write_text((ROOT/'include'/parent).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in parents:(w/parent).unlink()
   print('PASS AWM cluster active control:',name,flush=True)

if __name__=='__main__':main()
