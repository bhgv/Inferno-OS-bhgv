#!/usr/bin/env python3
"""Build and execute every unchanged Winplace body with real Draw geometry."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ALT=runpy.run_path(str(ROOT/'utils/tests/native64-aprint-alt.py'));G,CMD=ALT['G'],ALT['CMD'];E=G['E']
METHODS=('Point_add','Point_sub','Point_div','Rect_dx','Rect_dy','Rect_size','Rect_inset','Rect_addpt','Rect_subpt')
def generate(limbo,w):
 m=w/'interfaces.m';m.write_text('include "sys.m";include "draw.m";include "winplace.m";');frames={};sigs={}
 for name in('Sys','Draw','Winplace'):
  h=subprocess.check_output([str(limbo),'-H','-M',name,'-I',str(ROOT/'module'),str(m)],text=True);(w/(name+'.h')).write_text(h)
  for fn,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+r'_(\w+) (0x[0-9a-f]+)U',h,re.M):
   public=fn.replace('_','.',1)if '_'in fn else fn;size=re.search(r'^#define F_'+name+'_'+fn+r'_size (\d+)',h,re.M);mp=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M)
   if size and mp:frames[(public,int(sig,16))]=(int(size[1]),[int(x,0)for x in mp[1].split(',')if x])
   if name=='Winplace':sigs[public]=int(sig,16)
 h=(w/'Draw.h').read_text();text='typedef struct Draw_Point Draw_Point;typedef struct Draw_Rect Draw_Rect;\n'
 for n in('Draw_Point','Draw_Rect'):text+=re.search(r'struct '+n+r' \{.*?\n};',h,re.S)[0]+'\n'
 for n in METHODS:text+='typedef struct F_Draw_'+n+' F_Draw_'+n+';\n'+re.search(r'struct F_Draw_'+n+r' \{.*?\n};',h,re.S)[0]+'\n'
 (w/'geometry-layout.h').write_text(text)
 source=(ROOT/'appl/lib/winplace.b').read_text();rows=G['generate'](limbo,w,source,'Winplace',(21,2,1,'~0U'),import_frames=frames,export_signatures=sigs)
 global OP_NAMES
 OP_NAMES={name[1:].lower()for name,op in E['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
 data,records=E['compile_source'](limbo,w,source,interface='',loads=True);tables=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];count=0
 for row in records:
  for pc,c in enumerate(row['code']):
   if c[5]not in(65,79,80,81)and not(c[5]==64 and c[0]==E['OPCODES']['IHEADM']):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,tables,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):count+=1
    else:raise AssertionError(('unchecked Winplace instruction',row['name'],pc,k))
 print('PASS complete Winplace new source roles:',count,'instruction corruptions detected',flush=True)
def unit(w):
 src=ALT['unit'](w).replace('#define RuntimeBlocks 256','#define RuntimeBlocks 1024').split('\n#line 1 "native64-aprint-alt.c"')[0]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('static uchar*\narrayaddress(','static void\narrayindex(','static uchar*\narrayspan(','static void\nnewarray('):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 src+='\n#include "geometry-layout.h"\n'
 for n in METHODS:src+='\n#define F_'+n+' F_Draw_'+n+'\n'+extract((ROOT/'libinterp/geom.c').read_text(),'void\n'+n+'(')+'\n'
 for n in sorted(OP_NAMES):
  n={'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(n,n)
  if 'OP('+n+')'not in src:src+='\n'+extract(strings if 'OP('+n+')'in strings else vm,'OP('+n+')')+'\n'
 handlers=''.join('optab['+name+']=actual_'+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+';'for name in E['OPCODES']if 'OP('+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+')'in src)
 return src+'\n#line 1 "native64-winplace-module.c"\n'+(ROOT/'utils/tests/native64-winplace-module.c').read_text().replace(' WINPLACE_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='full-winplace-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(binary)];subprocess.run(command,check=True);
  subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=240)
  headers=('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-raw-array-runtime.h','native64-raw-array-module.h','native64-raw-array-code.h')
  controls=(
   ('native64-raw-array-runtime.h','t->size!=(int)width','0','raw element type boundary'),
   ('native64-raw-array-module.h','if(flow[k]&&((flow[k]>>31)==(UDST(v->mode)==AMP))&&v->d>=(UDST(v->mode)==AFP?64:0)&&(unsigned long)v->d/8==((flow[k]&0x7fffffffU)>>2)/8)flow[k]=0;','if(0)flow[k]=0;','borrow owner overwrite refused'),
   ('native64-raw-array-code.h','p->s.i.s=(s->m>>24)&255;','p->s.i.s=0;','Winplace predicate:'))
  for header,before,after,predicate in controls:
   for name in headers:(w/name).write_text((ROOT/'include'/name).read_text())
   text=(w/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after));subprocess.run(command,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=240)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(header,r.returncode,r.stderr[-2000:])
   for name in headers:(w/name).unlink()
   print('PASS full Winplace active control:',predicate,flush=True)
if __name__=='__main__':main()
