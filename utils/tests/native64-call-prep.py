#!/usr/bin/env python3
"""Execute String slice/concat, list head and rooted Window FD call preparation."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FMT=runpy.run_path(str(ROOT/'utils/tests/native64-format-fields.py'));G,CMD,E=FMT['G'],FMT['CMD'],FMT['E']
PREFIX='include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";Sink:module{PATH:con "/dis/sink.dis";send:fn(s:string);};'
DECL='Callprep:module{init:fn();slice:fn(w:ref Wmclient->Window);head:fn(w:ref Wmclient->Window);fd:fn(w:ref Wmclient->Window);slicemp:fn(w:ref Wmclient->Window);headmp:fn(w:ref Wmclient->Window);};'
SOURCE='implement Callprep;'+PREFIX+DECL+'sys:Sys;sink:Sink;request,out:string;start,status:int;devices:list of string;init(){sys=load Sys Sys->PATH;sink=load Sink Sink->PATH;}slice(w:ref Wmclient->Window){req:=request;sink->send("prefix "+req[start:]);}head(w:ref Wmclient->Window){devs:=devices;s:=sys->sprint("%q",hd devs);out=s;}fd(w:ref Wmclient->Window){n:=sys->fprint(w.ctxt.connfd,"%s",request);status=n;}'
SOURCE+='slicemp(w:ref Wmclient->Window){sink->send("prefix "+request[start:]);}headmp(w:ref Wmclient->Window){s:=sys->sprint("%q",hd devices);out=s;}'
def generate(limbo,w):
 m=w/'callprep.m';m.write_text(PREFIX+DECL);headers={n:subprocess.check_output([str(limbo),'-H','-M',n,'-I',str(ROOT/'module'),str(m)],text=True)for n in('Callprep','Sink','Sys','Wmclient')}
 sig={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Callprep_(\w+) (0x[0-9a-f]+)U',headers['Callprep'],re.M)};frames={}
 for n in('Sink','Sys'):
  h=headers[n]
  for f,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+n+r'_(\w+) (0x[0-9a-f]+)U',h,re.M):
   size=re.search(r'^#define F_'+n+'_'+f+r'_size (\d+)',h,re.M);mp=re.search(r'^#define F_'+n+'_'+f+r'_map \{([^}]*)\}',h,re.M)
   if size and mp:frames[(f,int(v,16))]=(int(size[1]),[int(x,0)for x in mp[1].split(',')if x])
 rows=G['generate'](limbo,w,SOURCE,'Callprep',(6,2,3,'~0U'),import_frames=frames,export_signatures=sig)
 p=w/'full.h';gl={g[0]:g[1]for g in rows[0]['globals']};p.write_text(p.read_text()+'\nstatic const uint globals[]={'+','.join(str(gl[n])for n in('sys','sink','request','out','start','status','devices'))+'};\n');(w/'window-native.h').write_text(re.sub(r'struct Sys_FD \{.*?\n};','',headers['Wmclient'],flags=re.S))
 data,rows=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];count=0
 for row in rows:
  for pc,c in enumerate(row['code']):
   if c[5]not in(58,59,87):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):count+=1
    else:raise AssertionError(('missed preparation mutation',row['name'],pc,k))
 print('PASS call preparation source oracle:',count,'mutations, canonical frames and unchanged classic bytes',flush=True)
 _,rows=E['compile_source'](limbo,w,(ROOT/'appl/lib/wmclient.b').read_text(),interface='',loads=True);print('Wmclient frontier:',[(r['name'],r.get('pc'),r.get('reason'))for r in rows if not r['accepted']],flush=True)
 for prefix,argument in (('Other:adt{ctxt:ref Draw->Wmcontext;};','Other'),('Fake:module{Window:adt{ctxt:ref Draw->Wmcontext;};};','Fake->Window')):
  source='implement Closed;include "sys.m";include "draw.m";'+prefix+'Closed:module{run:fn(sys:Sys,w:ref '+argument+');};run(sys:Sys,w:ref '+argument+'){n:=sys->fprint(w.ctxt.connfd,"%s","x");}'
  _,r=E['compile_source'](limbo,w,source,interface='',loads=True);assert not r[0]['accepted'],r
 print('PASS preparation boundary: foreign FD owner and namespace refused',flush=True)
def unit(w):
 src=FMT['unit'](w).split('\n#line 1 "native64-format-fields.c"')[0];start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start);extract=CMD['NS']['S']['FILES']['FILES']['function'];strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('String*\nslicer(','OP(slicec)'):src+='\n'+extract(strings,sig)+'\n'
 return src+'\n#line 1 "native64-call-prep.c"\n'+(ROOT/'utils/tests/native64-call-prep.c').read_text().replace(' PREP_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='call-prep-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];cmd=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','fmtquote','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(w/'unit')];subprocess.run(cmd,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('wrong slice lower bound','native64-typed-multi.h','else if(s->returned==63 || s->returned==87 || s->returned==180 || s->returned==181){checkstep=*s;if(s->returned!=63)checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','else if(s->returned==63 || s->returned==87 || s->returned==180 || s->returned==181){checkstep=*s;if(s->returned==87&&s->op==ISLICEC){checkstep.mode=SRC(AIMM)|AXINF|DST(AFP);checkstep.s=0;}if(s->returned!=63)checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}','exact slice/concat payload'),
   ('head passed as list','native64-sys-module.h','else if(step->returned==58){out->s.ind=step->s;','else if(step->returned==58){out->op=IMOVP;out->s.ind=step->s;','list head identity'),
   ('wrong Window context offset','native64-sys-module.h','out->s.i.s=step->m;if(UDST','out->s.i.s=step->m==48?24:step->m;if(UDST','exact preparation completion/error')):
   parents=('native64-typed-module.h','native64-typed-multi.h','native64-sys-module.h')
   for n in parents:(w/n).write_text((ROOT/'include'/n).read_text())
   text=(ROOT/'include'/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(cmd,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for n in parents:(w/n).unlink()
   print('PASS preparation active control:',name,flush=True)
if __name__=='__main__':main()
