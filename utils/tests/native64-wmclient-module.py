#!/usr/bin/env python3
"""Construct and execute the complete unchanged Wmclient module."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ALT=runpy.run_path(str(ROOT/'utils/tests/native64-aprint-alt.py'));G,CMD=ALT['G'],ALT['CMD']
def generate(limbo,w):
 R=ROOT;m=w/'interfaces.m';m.write_text(''.join('include "'+n+'.m";'for n in('sys','draw','tk','wmlib','titlebar','wmclient')));frames={};sigs={}
 for name in('Sys','Draw','Tk','Wmlib','Titlebar','Wmclient'):
  h=subprocess.check_output([str(limbo),'-H','-M',name,'-I',str(R/'module'),str(m)],text=True);(w/(name+'.h')).write_text(h)
  for fn,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+r'_(\w+) (0x[0-9a-f]+)U',h,re.M):
   public=fn.replace('_','.',1)if '_'in fn else fn;size=re.search(r'^#define F_'+name+'_'+fn+r'_size (\d+)',h,re.M);mp=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M)
   if size and mp:frames[(public,int(sig,16))]=(int(size[1]),[int(x,0)for x in mp[1].split(',')if x])
   if name=='Wmclient':sigs[public]=int(sig,16)
 h=(w/'Draw.h').read_text();(w/'geometry-layout.h').write_text(''.join('typedef struct F_Draw_'+n+' F_Draw_'+n+';\n'+re.search(r'struct F_Draw_'+n+r' \{.*?\n};',h,re.S)[0]+'\n'for n in('Rect_size','Rect_inset','Rect_dx','Rect_dy')))
 source=(R/'appl/lib/wmclient.b').read_text();data,rows=G['E']['compile_source'](limbo,w,source,interface='',loads=True);tables=G['E']['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];assert (len(rows),len(tables),sum(len(r.get('variadics',[]))for r in rows))==(21,5,11)
 (w/'window-native.h').write_text(re.sub(r'struct Sys_FD \{.*?\n};','',(w/'Wmclient.h').read_text(),flags=re.S))
 rows=G['generate'](limbo,w,source,'Wmclient',(len(rows),len(tables),sum(len(r.get('variadics',[]))for r in rows),'~0U'),import_frames=frames,export_signatures=sigs)

 count=0
 for row in rows:
  for pc,step in enumerate(row['code']):
   if step[5]not in(60,61,62,63,66,67,68,69,70,71,73):continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,tables,rows=rows)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):count+=1
    else:raise AssertionError(('unchecked original source mutation',row['name'],pc,k))
 print('PASS Wmclient new source roles:',count,'instruction corruptions detected',flush=True)

 global OP_NAMES
 OP_NAMES={name[1:].lower()for name,op in G['E']['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
def unit(w):
 src=ALT['unit'](w).replace('#define RuntimeBlocks 256','#define RuntimeBlocks 1024').split('\n#line 1 "native64-aprint-alt.c"')[0]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text()
 for sig in('static uchar*\narrayaddress(','static void\narrayindex(','static void\nnewarray(','static void\nbranchstring(','static Inst*\ncasepc(','static uvlong\ncaseccell('):
  if sig not in src:src+='\n'+extract(vm,sig)+'\n'
 src+='\n#include \"window-native.h\"\n#include \"geometry-layout.h\"\n'
 for n in('Rect_size','Rect_inset','Rect_dx','Rect_dy'):src+='\n#define F_'+n+' F_Draw_'+n+'\n'+extract((ROOT/'libinterp/geom.c').read_text(),'void\n'+n+'(')+'\n'
 src+='\n'+extract(strings,'String*\nslicer(')+'\n'
 for n in sorted(OP_NAMES):
  n={'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(n,n)
  if 'OP('+n+')'not in src:src+='\n'+extract(strings if 'OP('+n+')'in strings else vm,'OP('+n+')')+'\n'
 handlers=''.join('optab['+name+']=actual_'+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+';'for name in G['E']['OPCODES']if 'OP('+({'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase','casec':'casec'}.get(name[1:].lower(),name[1:].lower()))+')'in src)
 src=src.replace('newc(&Tword, movw);','newc(&Tword, actual_movw);').replace('newc(&Tptr, movp);','newc(&Tptr, actual_movp);')
 return src+'\n#line 1 "native64-wmclient-module.c"\n'+(ROOT/'utils/tests/native64-wmclient-module.c').read_text().replace(' WM_HANDLERS',handlers)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='wmclient-module-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit(w));arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  cc+=[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(w/'unit')];subprocess.run(cc,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],check=True,timeout=180)
  for label,header,before,after,predicate in (
   ('Window root result field','native64-typed-multi.h','else if(s->returned==60){p->reg=2048|512|24;p->s.i.f=s->s;p->s.i.s=s->m;','else if(s->returned==60){p->reg=2048|512|24;p->s.i.f=s->s;p->s.i.s=s->m==48?0:s->m;','Window context/display/control channel'),
   ('byte slice lower bound','native64-typed-multi.h','else if(s->returned==69||s->returned==71){checkstep=*s;checkstep.returned=0;','else if(s->returned==69||s->returned==71){checkstep=*s;if(s->returned==69&&s->op==ISLICEA)checkstep.s=1;checkstep.returned=0;','original byte slice String conversion'),
   ('Window image rectangle field','native64-typed-code.h','p->d.i.s=(s->m>>8)&255;continue;}','p->d.i.s=((s->m>>8)&255)==8?32:(s->m>>8)&255;continue;}','original nested context putimage and drawborder completion')):
   for name in ('native64-typed-module.h','native64-sys-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/name).write_text((ROOT/'include'/name).read_text())
   text=(ROOT/'include'/header).read_text();assert text.count(before)==1;(w/header).write_text(text.replace(before,after,1));subprocess.run(cc,check=True);r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(w/'unit')],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(label,r.returncode,r.stdout[-1000:],r.stderr[-1000:])
   for name in ('native64-typed-module.h','native64-sys-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/name).unlink()
   print('PASS complete Wmclient active control:',label,flush=True)

if __name__=='__main__':main()
