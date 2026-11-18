#!/usr/bin/env python3
"""Execute checked Wmclient.Window FP/MP root-field transfers in the real C/JIT VM."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD']
SOURCE='implement Windows;include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";wmclient:Wmclient;Window:import wmclient;Windows:module{capture:fn(win:ref Window);publish:fn(win:ref Window);captureglobal:fn();publishglobal:fn();forget:fn(win:ref Window);forgetglobal:fn();roundtrip:fn(win:ref Window);};display:ref Draw->Display;image:ref Draw->Image;ctxt:ref Draw->Wmcontext;ctl:chan of string;titlebar:ref Tk->Toplevel;screen:ref Draw->Screen;saved:ref Window;capture(win:ref Window){if(win.display==nil)display=nil;else display=win.display;if(win.image!=nil)image=win.image;else image=nil;if(win.ctxt==nil)ctxt=nil;else ctxt=win.ctxt;if(win.ctl!=nil)ctl=win.ctl;else ctl=nil;if(win.titlebar==nil)titlebar=nil;else titlebar=win.titlebar;if(win.screen!=nil)screen=win.screen;else screen=nil;}captureglobal(){if(saved.display==nil)display=nil;else display=saved.display;if(saved.image!=nil)image=saved.image;else image=nil;if(saved.ctxt==nil)ctxt=nil;else ctxt=saved.ctxt;if(saved.ctl!=nil)ctl=saved.ctl;else ctl=nil;if(saved.titlebar==nil)titlebar=nil;else titlebar=saved.titlebar;if(saved.screen!=nil)screen=saved.screen;else screen=nil;}publish(win:ref Window){win.display=display;win.image=image;win.ctxt=ctxt;win.ctl=ctl;win.titlebar=titlebar;win.screen=screen;}publishglobal(){saved.display=display;saved.image=image;saved.ctxt=ctxt;saved.ctl=ctl;saved.titlebar=titlebar;saved.screen=screen;}forget(win:ref Window){win.display=nil;win.image=nil;win.ctxt=nil;win.ctl=nil;win.titlebar=nil;win.screen=nil;}forgetglobal(){saved.display=nil;saved.image=nil;saved.ctxt=nil;saved.ctl=nil;saved.titlebar=nil;saved.screen=nil;}roundtrip(win:ref Window){v0:=win.display;v1:=win.image;v2:=win.ctxt;v3:=win.ctl;v4:=win.titlebar;v5:=win.screen;win.display=v0;display=v0;win.image=v1;image=v1;win.ctxt=v2;ctxt=v2;win.ctl=v3;ctl=v3;win.titlebar=v4;titlebar=v4;win.screen=v5;screen=v5;}'
FIELDS=['display', 'image', 'ctxt', 'ctl', 'titlebar', 'screen']

def generate(limbo,w):
 interface=w/'windows.m';interface.write_text(SOURCE[:SOURCE.index('};')+2].replace('implement Windows;','',1))
 signatures=subprocess.check_output([str(limbo),'-H','-M','Windows','-I',str(ROOT/'module'),str(interface)],text=True)
 canonical={n:int(value,16)for n,value in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Windows_(\w+) (0x[0-9a-f]+)U',signatures,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Windows',(7,0,0,'~0U'),export_signatures=canonical)
 types=w/'window-types.m';types.write_text('include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";')
 header=subprocess.check_output([str(limbo),'-H','-M','Wmclient','-I',str(ROOT/'module'),str(types)],text=True);(w/'window-native.h').write_text(header)
 probe='implement Probe;'+types.read_text()+'wmclient:Wmclient;Window:import wmclient;Probe:module{make:fn():ref Window;};make():ref Window{return ref Window;}'
 data,_=G['E']['compile_source'](limbo,w,probe,interface='',loads=True)
 descriptors=G['E']['BASE']['READ']['legacy_instructions'](data,with_types=True)[2]
 assert any(size==104 and {i*8+b for i,x in enumerate(mapping)for b in range(8)if x&(128>>b)}=={0,5,10,13,14,21}for size,mapping in descriptors.values())
 print('PASS Window classic payload descriptor: 104 bytes, six exact roots; generated native C payload checked at execution',flush=True)
 (w/'full.h').write_text((w/'full.h').read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 globals={g[0]:g[1]for g in rows[0]['globals']}
 with(w/'full.h').open('a')as out:
  for n in FIELDS+['saved']:out.write(f'static const uint window_{n}={globals[n]};\n')
 data,reports=G['E']['compile_source'](limbo,w,SOURCE,interface='',loads=True)
 controls=0
 for row in reports:
  for pc,step in enumerate(row['code']):
   if step[5]not in range(100,107):continue
   for field in(0,1,2,3,4,5):
    bad=copy.deepcopy(row);bad['code'][pc][field]+=1
    try:G['E']['verify'](data,bad,[],loads=True,rows=reports)
    except(AssertionError,KeyError,IndexError,StopIteration):controls+=1
    else:raise AssertionError(('field oracle missed mutation',row['name'],pc,field))
 print('PASS Window source oracle:',controls,'field record mutations detected, classic bytes unchanged',flush=True)
 return rows

def refusals(limbo,w):
 for declaration,owner,expression in(('Other:adt{image:ref Draw->Image;};','Other','c.image'),('Fake:module{Window:adt{image:ref Draw->Image;};};','Fake->Window','c.image'),('','Window','c.r')):
  source='implement Closed;include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";wmclient:Wmclient;Window:import wmclient;'+declaration+'Closed:module{run:fn(c:ref '+owner+');};'+('value:Draw->Rect;'if expression=='c.r'else'value:ref Draw->Image;')+'run(c:ref '+owner+'){value='+expression+';}'
  _,rows=G['E']['compile_source'](limbo,w,source,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted'],rows
 print('PASS Window compiler boundary: other ADT, wrong namespace and aggregate field remain closed; ordinary bytes preserved',flush=True)
 source='implement Owners;include "sys.m";include "draw.m";include "tk.m";include "wmclient.m";wmclient:Wmclient;Window:import wmclient;Other:adt{'+''.join('n'+str(i)+':int;'for i in range(21))+'screen:ref Draw->Screen;};Owners:module{bad:fn(w:ref Window,o:ref Other);good:fn(w:ref Window);};display:ref Draw->Display;screen:ref Draw->Screen;getw(w:ref Window):ref Window{return w;}geto(o:ref Other):ref Other{return o;}bad(w:ref Window,o:ref Other){display=getw(w).display;screen=geto(o).screen;}good(w:ref Window){display=getw(w).display;}'
 data,rows=G['E']['compile_source'](limbo,w,source,interface='',loads=True)
 assert next(r for r in rows if r['name']=='good')['accepted']and not next(r for r in rows if r['name']=='bad')['accepted'],rows
 row=next(r for r in rows if r['name']=='bad');end=min([r['first']for r in rows if r['first']>row['first']]+[len(G['E']['BASE']['READ']['legacy_instructions'](data)[1])])
 fields=[i for i in G['E']['BASE']['READ']['legacy_instructions'](data)[1][row['first']:end]if i['op']==41 and i['mode']>>3&7==5 and i['s'][1]in(0,84)]
 assert len(fields)==2 and fields[0]['s'][0]==fields[1]['s'][0],fields
 print('PASS original owner provenance: same classic scratch reused by Window/Other; valid returned Window admitted and foreign owner refused',flush=True)


def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)')
 start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-window-fields.c"\n'+(ROOT/'utils/tests/native64-window-fields.c').read_text().replace(' WINDOW_HANDLERS',src[start:end])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='window-fields-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);refusals(a.limbo.resolve(),w)
  (w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64')
  cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)]
  subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('field-displacement','native64-typed-code.h','p->s.i.s=s->m;','p->s.i.s=0;','exact MP field value'),
   ('branch-displacement','native64-typed-code.h','p->s.i.s=native64windowfieldoffset(s->returned-Native64TypedWindowNil);','p->s.i.s=0;','exact MP field value'),
   ('mapped-roots','native64-typed-module.h','if(!(map[bit/8] & (128 >> (bit%8))))return 0;','if(0)return 0;','malformed Window metadata refused')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   header=w/path;header.write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h'):(w/parent).unlink()
   print('PASS Window active control:',name,flush=True)

if __name__=='__main__':main()
