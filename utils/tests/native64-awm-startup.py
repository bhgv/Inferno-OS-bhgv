#!/usr/bin/env python3
"""Run unchanged emuinit -> AWM with nine real native Limbo modules and controlled devices."""
import argparse,copy,os,re,runpy,shlex,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ALT=runpy.run_path(str(ROOT/'utils/tests/native64-aprint-alt.py'));G,CMD=ALT['G'],ALT['CMD'];E=G['E']
MODULES=(('boot','Emuinit','appl/cmd/emuinit.b',4),('arg','Arg','appl/lib/arg.b',8),('awm','Wm','appl/wm/awm.b',20),('string','String','appl/lib/string.b',21),('wmsrv','Wmsrv','appl/lib/wmsrv.b',34),('wmclient','Wmclient','appl/lib/wmclient.b',21),('winplace','Winplace','appl/lib/winplace.b',21),('wmlib','Wmlib','appl/lib/wmlib.b',25),('titlebar','Titlebar','appl/lib/titlebar.b',7))
PANEL_MODULES=(('tkclient','Tkclient','appl/lib/tkclient.b',16),('awmpanel','Awmpanel','appl/wm/awmpanel.b',13),('awmcalc','Awmcalc','appl/wm/awmcalc.b',6),('awmfiles','Awmfiles','appl/wm/awmfiles.b',7),('awmkeys','Awmkeys','appl/wm/awmkeys.b',6))
TOOLBAR=('atoolbar','Toolbar','appl/wm/atoolbar.b',26)
def generate(limbo,w,metadata=None,panel=False):
 selected=MODULES+PANEL_MODULES if panel else MODULES
 if panel and metadata is None:
  selected+=(TOOLBAR,)
 interface=w/'graph.m';interface.write_text(''.join('include "'+n+'.m";'for n in(('sys','draw','tk','string','wmsrv','wmlib','titlebar','wmclient','winplace','arg','sh')+('tkclient',)*panel))+'Wm:module{init:fn(ctxt:ref Draw->Context,argv:list of string);};Emuinit:module{init:fn();};')
 if panel:
  for name in ('Awmpanel','Awmcalc','Awmfiles','Awmkeys'):
   interface.write_text(interface.read_text()+name+':module{init:fn(ctxt:ref Draw->Context,argv:list of string);};')
  if metadata is None:
   source=(ROOT/TOOLBAR[2]).read_text()
   declaration=re.search(r'(?ms)^Toolbar: module\s*\n\{.*?^\};',source)[0]
   interface.write_text(interface.read_text()+'shell: Sh;Listnode, Context: import shell;'+declaration)
 frames={};signatures={};headers={}
 names=('Sys','Draw','Tk','String','Wmsrv','Wmlib','Titlebar','Wmclient','Winplace','Arg','Sh','Wm','Emuinit')
 for name in names+(('Tkclient','Awmpanel','Awmcalc','Awmfiles','Awmkeys') if panel else ())+(('Toolbar',) if panel and metadata is None else ()):
  h=headers[name]=subprocess.check_output([str(limbo),'-H','-M',name,'-I',str(ROOT/'module'),str(interface)],text=True);signatures[name]={}
  for fn,sig in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_'+name+r'_(\w+) (0x[0-9a-f]+)U',h,re.M):
   public=fn.replace('_','.',1)if '_'in fn else fn;signatures[name][public]=int(sig,16)
   size=re.search(r'^#define F_'+name+'_'+fn+r'_size (\d+)',h,re.M);mp=re.search(r'^#define F_'+name+'_'+fn+r'_map \{([^}]*)\}',h,re.M)
   if size and mp:
    key=(public,int(sig,16));value=(int(size[1]),[int(x,0)for x in mp[1].split(',')if x]);assert key not in frames or frames[key]==value;frames[key]=value
 modules=[];ops=set();alltables={};summaries=[];awmglobals=None
 for prefix,name,path,count in selected:
  source=(ROOT/path).read_text();data,rows=E['compile_source'](limbo,w,source,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];nv=sum(len(r.get('variadics',[]))for r in rows)
  assert len(rows)==count and all(r['accepted']for r in rows),(path,[(r['name'],r.get('reason'))for r in rows])
  rows=G['generate'](limbo,w,source,name,(count,len(imports),nv,'0'if prefix=='boot'else'~0U'),import_frames=frames,export_signatures=signatures[name],source_label='startup graph '+path)
  controls=0
  for row in rows:
   for pc,c in enumerate(row['code']):
    if c[0]!=E['OPCODES']['ILOAD']:continue
    bad=copy.deepcopy(row);bad['code'][pc][3]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=rows)
    except(AssertionError,IndexError,KeyError,ValueError):controls+=1
    else:raise AssertionError(('unchecked LOAD namespace',path,row['name'],pc))
  print('PASS startup LOAD namespace oracle:',path,controls,'wrong table ordinals refused',flush=True)
  text=(w/'full.h').read_text().replace('fullload','graphload').replace('full_',prefix+'_').replace('fullarray',prefix+'_array');(w/('graph-'+prefix+'.h')).write_text(text)
  if prefix!='atoolbar':
   dispath='/dis/emuinit.dis' if prefix=='boot' else '/dis/'+str(Path(path).relative_to('appl').with_suffix('.dis'))
   if prefix=='awmpanel':dispath='/dis/wm/awmpanel-keyboard.dis'
   modules.append('{&'+prefix+'_spec,'+prefix+'_variadics,'+str(nv)+',"'+dispath+'"}')
  summaries.append((prefix,count,len(imports),nv))
  if (prefix,name,path,count) in MODULES:
   ops.update(c[0]for r in rows for c in r['code'])
   alltables[prefix]=imports
  if prefix=='awm':awmglobals={x[0]:x[1]for x in rows[0]['globals']}
 (w/'full.h').unlink()
 if metadata is not None:
  headers=[(w/('graph-'+prefix+'.h')).read_text() for prefix,_,_,_ in selected]
  body='static const GraphSpec graph_specs[]={'+','.join(modules)+'};\n'
  metadata.write_text('/* Generated from the checked native64 AWM sources. */\n'+'\n'.join(headers)+body)
  return
 body='static const GraphSpec graph_specs[]={'+','.join(modules[:len(MODULES)])+'};\n'
 body+='static const uint awm_screen_offset='+str(awmglobals['screen'])+';\n'
 for name in ('Sys','Draw','Tk'):
  refs={}
  for prefix,tables in alltables.items():
   for ti,table in enumerate(tables):
    for j,e in enumerate(table):
     if signatures[name].get(e['name'])==e['signature']:refs[(e['name'],e['signature'])]=prefix+'_import_'+str(ti)+'+'+str(j)
  body+='static const Native64ModuleImport *graph_'+name+'[]={'+','.join(refs.values())+'};\n'
 ti=next(i for i,t in enumerate(alltables['boot'])if len(t)==1 and t[0]['name']=='init');assert frames[('init',alltables['boot'][ti][0]['signature'])][0]==80
 body+='static const Native64ModuleImport *graph_application=boot_import_'+str(ti)+';\n'
 # Interface tables are merged by module LOAD paths at runtime; linkmod remains the signature gate.
 (w/'graph-specs.h').write_text(body)
 h=headers['Draw'];geometry='typedef struct Draw_Point Draw_Point;typedef struct Draw_Rect Draw_Rect;\n'
 for n in('Draw_Point','Draw_Rect'):geometry+=re.search(r'struct '+n+r' \{.*?\n};',h,re.S)[0]+'\n'
 methods=[]
 for fn in re.findall(r'^struct F_Draw_((?:Point|Rect)_\w+) \{',h,re.M):
  methods.append(fn);geometry+='typedef struct F_Draw_'+fn+' F_Draw_'+fn+';\n'+re.search(r'struct F_Draw_'+fn+r' \{.*?\n};',h,re.S)[0]+'\n'
 (w/'geometry-layout.h').write_text(geometry)
 print('PASS startup graph:',len(selected),'source modules,',sum(x[1]for x in summaries),'bodies, independent source/call/map proofs and identical classic bytes',summaries,flush=True)
 return ops,methods

def unit(w,ops,methods):
 extract=CMD['NS']['S']['FILES']['FILES']['function'];src=ALT['unit'](w).split('\n#line 1 "native64-aprint-alt.c"')[0].replace('#define RuntimeBlocks 256','#define RuntimeBlocks 8192')
 vm=(ROOT/'libinterp/xec.c').read_text();strings=(ROOT/'libinterp/string.c').read_text();heap=(ROOT/'libinterp/heap.c').read_text()
 for owner,sig in((heap,'void\nfreestring('),(vm,'static uchar*\narrayaddress('),(vm,'static void\narrayindex('),(vm,'static uchar*\narrayspan('),(vm,'static void\nnewarray('),(vm,'static void\nbranchstring('),(vm,'static Inst*\ncasepc('),(vm,'static uvlong\ncaseccell('),(strings,'String*\nslicer(')):
  if sig not in src:src+='\n'+extract(owner,sig)+'\n'
 src=src.replace(extract(src,'Array*\narraycpy('),'void* arraycpy(Array*);',1)
 src+='\ntypedef struct ArrayCopy ArrayCopy;\n'+re.search(r'struct ArrayCopy \{.*?\n};',heap,re.S)[0]+'\n'+extract(heap,'static int\ncopyarray(')+'\n'+extract(heap,'void*\narraycpy(')+'\n'
 names={'load':'iload','goto':'igoto','send':'isend','recv':'irecv','exit':'iexit','raise':'iraise','case':'icase'}
 handlers=''
 for name,op in E['OPCODES'].items():
  if op not in ops:continue
  n=names.get(name[1:].lower(),name[1:].lower());sig='OP('+n+')'
  if sig not in src:src+='\n'+extract(strings if sig in strings else vm,sig)+'\n'
  handlers+='optab['+name+']=actual_'+n+';'
 src=src.replace('OP(newcw)','\n#define movw actual_movw\nOP(newcw)').replace('OP(newcp)','\n#define movp actual_movp\nOP(newcp)').replace('OP(newcl)','\n#define movl actual_movl\nOP(newcl)')
 src+='\n#include "geometry-layout.h"\n'
 geom=(ROOT/'libinterp/geom.c').read_text();geometrytable=[]
 for n in methods:
  sig='void\n'+n+'('
  if sig in geom:src+='\n#define F_'+n+' F_Draw_'+n+'\n'+extract(geom,sig)+'\n';geometrytable.append('{"'+n.replace('_','.',1)+'",'+n+'}')
 src+='\nstatic const struct {const char*name;void(*call)(void*);} geometry[]={'+','.join(geometrytable)+'};\n'
 return src+'\n#line 1 "native64-awm-startup.c"\n'+(ROOT/'utils/tests/native64-awm-startup.c').read_text().replace(' GRAPH_HANDLERS',handlers)

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--limbo',required=True,type=Path)
 p.add_argument('--android-ndk',type=Path,help='build the ARM64/Bionic fixture with NDK clang')
 p.add_argument('--panel',action='store_true',help='include Tkclient and the AWM desktop panel')
 p.add_argument('--metadata',type=Path,help='generate trusted startup metadata instead of running the fixture')
 a=p.parse_args()
 if not __debug__:p.error('source oracles require assertions')
 limbo=Path(shutil.which(str(a.limbo)) or a.limbo).resolve()
 if a.metadata:
  with tempfile.TemporaryDirectory(prefix='awm-metadata-') as directory:
   generate(limbo,Path(directory),a.metadata,a.panel)
  return
 runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
 arch=os.environ.get('NATIVE64_ARCH','amd64')
 compiler=shlex.split(os.environ.get('CC','cc'))
 platform_includes=('Linux/'+arch+'/include','Linux/386/include')
 default_flags='-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'
 if a.android_ndk:
  clang=a.android_ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
  if not clang.is_file():p.error('Missing NDK ARM64 clang: '+str(clang))
  arch='arm64'
  compiler=[str(clang)]
  platform_includes=('Android/arm64/include','Android/arm/include')
  default_flags='-O2 -g -DNDEBUG -fno-strict-aliasing -static'
 cc=compiler+shlex.split(os.environ.get('NATIVE64_CFLAGS',default_flags))
 if a.panel:cc+=['-DTEST_AWM_PANEL_METADATA=1']
 if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
 with tempfile.TemporaryDirectory(prefix='awm-startup-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](limbo,w,'Command',False,argv=True,local=True,pair=True,raises=True);ops,methods=generate(limbo,w,panel=a.panel);(w/'unit.c').write_text(unit(w,ops,methods))
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in platform_includes+('emu/port','include','libinterp')]
  command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+[str(ROOT/('lib9/'+n+'.c'))for n in('sprint','snprint','vsnprint','dofmt','fmt','fmtlock','fmtquote','errfmt','utflen','utfnlen','runestrlen','fltfmt','pow10','isnan-posix')]+CMD['FILES']['releaseflags'](cc)+['-pthread','-lm','-o',str(w/'unit')]
  subprocess.run(command,check=True);subprocess.run(runner+[str(w/'unit')],check=True,timeout=240)
  original=(w/'unit.c').read_text()
  # Reproduce the two integration failures; success must depend on real quoting
  # and on the exact checked Client.id read, not just module construction.
  for label in ('quoted input commands','Client.id array read'):
   if label=='quoted input commands':
    assert original.count('quotefmtinstall();')==1
    (w/'unit.c').write_text(original.replace('quotefmtinstall();',''))
    predicate='four finite startup workers completed'
   else:
    (w/'unit.c').write_text(original)
    for name in ('native64-sys-module.h','native64-typed-module.h'):(w/name).write_text((ROOT/'include'/name).read_text())
    p=w/'native64-typed-module.h';text=p.read_text();before='else if(step->op==IINDL){if(j+1>=f->count||f->code[j+1].returned!=255||f->code[j+1].op!=IMOVP)'
    assert text.count(before)==1
    p.write_text(text.replace(before,before[:-1]+'||USRC(f->code[j+1].mode)==(AFP|AIND))'))
    predicate='full Wmlib constructor'
   subprocess.run(command,check=True)
   result=subprocess.run(runner+[str(w/'unit')],capture_output=True,text=True,timeout=240)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(label,result.returncode,result.stderr[-2000:])
   for name in ('native64-sys-module.h','native64-typed-module.h'):
    if (w/name).exists():(w/name).unlink()
   print('PASS startup active control:',label,flush=True)

if __name__=='__main__':main()
