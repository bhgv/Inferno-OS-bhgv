#!/usr/bin/env python3
"""Construct the complete unchanged Wmsrv module, including all call/ALT bodies."""
import argparse,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ALT=runpy.run_path(str(ROOT/'utils/tests/native64-aprint-alt.py'));G,CMD=ALT['G'],ALT['CMD']
def generate(limbo,w):
 interface=w/'wmsrv-interfaces.m';interface.write_text('include "sys.m";include "draw.m";include "wmsrv.m";')
 frames={};signatures={}
 for name in('Sys','Draw','Wmsrv'):
  h=subprocess.check_output([str(limbo),'-H','-M',name,'-I',str(ROOT/'module'),str(interface)],text=True)
  for fn,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+r'_(\w+) (0x[0-9a-f]+)U',h,re.M):
   # Generated underscores encode the single method separator in these imports.
   public=fn.replace('_','.',1)if '_'in fn else fn;size=re.search(r'^#define F_'+name+'_'+fn+r'_size (\d+)',h,re.M);mapping=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M)
   if size and mapping:frames[(public,int(sig,16))]=(int(size[1]),[int(x,0)for x in mapping[1].split(',')if x])
   if name=='Wmsrv':signatures[public]=int(sig,16)
 rows=G['generate'](limbo,w,(ROOT/'appl/lib/wmsrv.b').read_text(),'Wmsrv',(34,2,2,'~0U'),import_frames=frames,export_signatures=signatures,source_label='complete unchanged Wmsrv')
 assert all(r['accepted']for r in rows)
 global OP_NAMES
 OP_NAMES={name[1:].lower()for name,op in G['E']['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
 gl={x[0]:x[1]for x in rows[0]['globals']};(w/'full.h').write_text((w/'full.h').read_text()+'\nstatic const uint wm_sys_offset='+str(gl['sys'])+';\n')
def unit(w):
 src=ALT['unit'](w).split('\n#line 1 "native64-aprint-alt.c"')[0]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('static uchar*\narrayaddress(','static void\narrayindex(','static void\nnewarray(','static void\nbranchstring('):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 for n in sorted(OP_NAMES):
  n={'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'icasec'}.get(n,n)
  if 'OP('+n+')'not in src:src+='\n'+extract(strings if 'OP('+n+')'in strings else vm,'OP('+n+')')+'\n'
 handlers=''.join('optab['+name+']=actual_'+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'icasec'}.get(name[1:].lower(),name[1:].lower()))+';'for name in G['E']['OPCODES']if 'OP('+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'icasec'}.get(name[1:].lower(),name[1:].lower()))+')'in src)
 src=src.replace('newc(&Tword, movw);','newc(&Tword, actual_movw);').replace('newc(&Tptr, movp);','newc(&Tptr, actual_movp);')
 return src+'\n#line 1 "native64-wmsrv-module.c"\n'+(ROOT/'utils/tests/native64-wmsrv-module.c').read_text().replace(' WM_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='wmsrv-module-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  cc+=[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(w/'unit')];subprocess.run(cc,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],check=True,timeout=180)
  for name,header,before,after,predicate in (
   ('registry branch probe','native64-typed-multi.h','pair[0].d=1;','pair[0].d=0;','complete Wmsrv constructor'),
   ('original wm token field','native64-sys-module.h','else if(step->returned==95){out->s.i.f=step->s;out->s.i.s=step->m;','else if(step->returned==95){out->s.i.f=step->s;out->s.i.s=72;','original wm exact token bytes')):
   (w/'native64-typed-module.h').write_text((ROOT/'include/native64-typed-module.h').read_text())
   text=(ROOT/'include'/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(cc,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   (w/header).unlink();(w/'native64-typed-module.h').unlink();print('PASS complete Wmsrv active control:',name,flush=True)

if __name__=='__main__':main()
