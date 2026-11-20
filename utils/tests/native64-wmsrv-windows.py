#!/usr/bin/env python3
"""Execute unchanged Wmsrv window lookup, image collection and z-order bodies."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BUF=runpy.run_path(str(ROOT/'utils/tests/native64-bufio-library.py'));G,CMD,E=BUF['G'],BUF['CMD'],BUF['E']
EXTRACT=CMD['NS']['S']['FILES']['FILES']['function']
ORIGINAL=(ROOT/'appl/lib/wmsrv.b').read_text()
SELECTED=('Client.window','Client.image','clientimages','Client.top','Client.bottom','Client.remove','top','Client.hide','Client.unhide','findfid','addclient','delclient','Client.contains','find')
EXTRA='testfind:fn(a:array of ref Client,fid:int):ref Client;testadd:fn(a:array of ref Client,c:ref Client):array of ref Client;testdel:fn(a:array of ref Client,c:ref Client);testhit:fn(c:ref Client,x,y:int):int;testpick:fn(x,y:int):ref Client;testopen:fn();testclient:fn():ref Client;testwindow:fn(tag:string):ref Window;testorder:fn(c:ref Client);'
INTERFACE=(ROOT/'module/wmsrv.m').read_text();INTERFACE=INTERFACE[:INTERFACE.rindex('};')]+EXTRA+INTERFACE[INTERFACE.rindex('};'):]
def limbo_body(text):
 brace=text.index('{');depth=0
 for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|#[^\n]*|[{}]',text[brace:]):
  if token[0]=='{':depth+=1
  elif token[0]=='}':
   depth-=1
   if not depth:return text[:brace+token.end()]
 raise AssertionError('unterminated Limbo function')
SOURCE=ORIGINAL
for m in reversed(list(re.finditer(r'(?m)^([A-Za-z_]\w*(?:\.\w+)?)\(',ORIGINAL))):
 name=m[1]
 if name in SELECTED:continue
 body=limbo_body(ORIGINAL[m.start():]);replacement=body[:body.index('{')]+'{raise "unreached test stub";}'
 SOURCE=SOURCE[:m.start()]+replacement+SOURCE[m.start()+len(body):]
SOURCE=SOURCE.replace('include "wmsrv.m";',INTERFACE)+'''
testfind(a:array of ref Client,fid:int):ref Client{return findfid(a,fid);}
testadd(a:array of ref Client,c:ref Client):array of ref Client{return addclient(a,c);}
testdel(a:array of ref Client,c:ref Client){delclient(a,c);}
testhit(c:ref Client,x,y:int):int{return c.contains((x,y));}
testpick(x,y:int):ref Client{return find((x,y));}
testopen(){draw=load Draw Draw->PATH;}
testclient():ref Client{return ref Client;}
testwindow(tag:string):ref Window{w:=ref Window;w.tag=tag;return w;}
testorder(c:ref Client){zorder=c;}
'''
OP_NAMES=set()
def generate(limbo,w):
 m=w/'interfaces.m';m.write_text('include "sys.m";include "draw.m";'+INTERFACE)
 h=subprocess.check_output([str(limbo),'-H','-M','Wmsrv','-I',str(ROOT/'module'),str(m)],text=True)
 signatures={n.replace('Client_','Client.'):int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Wmsrv_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 dh=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);frames={}
 for fn,size in re.findall(r'^#define F_Draw_(\w+)_size (\d+)',dh,re.M):
  sig=re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Draw_'+fn+r' (0x[0-9a-f]+)U',dh,re.M)
  if sig:frames[(fn.replace('_','.'),int(sig[1],16))]=(int(size),[int(x,0)for x in re.search(r'^#define F_Draw_'+fn+r'_map \{([^}]*)\}',dh,re.M)[1].split(',')if x])
 rows=G['generate'](limbo,w,SOURCE,'Wmsrv',(26,1,0,'~0U'),import_frames=frames,export_signatures=signatures,source_label="Wmsrv window fixture (14 unchanged bodies, unused bodies raise)")
 global OP_NAMES
 OP_NAMES={name[1:].lower()for name,op in E['OPCODES'].items()if any(c[0]==op for r in rows for c in r['code'])}
 types=('Draw_Chans','Draw_Point','Draw_Rect','F_Draw_Rect_contains','Draw_Image','Draw_Screen','Wmsrv_Window','Wmsrv_Client');forward=types+('Draw_Display','Draw_Wmcontext')
 (w/'windows-layout.h').write_text('#ifndef WINDOWS_LAYOUT_H\n#define WINDOWS_LAYOUT_H\n'+''.join('typedef struct '+n+' '+n+';\n'for n in forward)+''.join(re.search(r'struct '+n+r' \{.*?\n};',dh if n.startswith('F_')else h,re.S)[0]+'\n'for n in types)+'\n#endif\n')
 p=w/'full.h';s=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','')
 s+='\nstruct LibraryFunction {uint fn,argc,kinds[3],offsets[3],result,contract;};\nstatic const struct LibraryFunction library_functions[]={\n'
 for i,r in enumerate(rows):
  if r['export_name']not in signatures:continue
  kind=r['result'][0];contract=72 if r.get('result_channels')else r['result'][1]|(16 if kind in E['ROOTS']else 0)
  s+='{'+f'{i},{len(r["args"])}'+',{'+','.join(str(a[1])for a in r['args'])+'},{'+','.join(str(a[0])for a in r['args'])+'},'+f'{kind},{contract}'+'},\n'
 s+='};\nstatic const uint exercised_functions[]={'+','.join(str(i)for i,r in enumerate(rows)if r['export_name']in SELECTED or r['export_name'].startswith('test'))+'};\n';p.write_text(s)
 # Confirm every selected body in the actual complete file, independently.
 data,full=E['compile_source'](limbo,w,ORIGINAL,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 chosen=[r for r in full if r.get('export_name')in SELECTED];assert len(chosen)==len(SELECTED)and all(r['accepted']for r in chosen),[(r['name'],r.get('reason'))for r in full if not r['accepted']]
 for r in chosen:CMD['verify_nilbranches'](data,r,imports,rows=full)
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in(111,141,142,143,153,154):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=100 if c[5]==154 and k==2 else 1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,ValueError,StopIteration):controls+=1
    else:raise AssertionError(('window oracle missed',r['export_name'],pc,k))
 print('PASS all 14 original registry/window/order bodies in complete Wmsrv:',controls,'independent mutations, unchanged classic bytes',flush=True)
def unit():
 BUF['unit'].__globals__['OP_NAMES']=OP_NAMES-{'beqc','bnec','raise'}
 original=BUF['unit']();prefix=original.split('\n#line 1 "native64-bufio-library.c"')[0]
 strings=(ROOT/'libinterp/xec.c').read_text()
 if 'static void\nbranchstring('not in prefix:
  helper=EXTRACT(strings,'static void\nbranchstring(')+'\n';prefix+=helper+'\n'+re.search(r'OP\(beqc\) [^\n]+',strings)[0]+'\n'+re.search(r'OP\(bnec\) [^\n]+',strings)[0]+'\n'
 prefix+='\n'+EXTRACT(strings,'OP(iraise)\n')+'\n' if 'OP(iraise)'not in prefix else ''
 start=original.rindex(' optab[IMOVW]=countmovw;');end=original.index(' metadata();construction();',start)
 prefix+='\n#include \"windows-layout.h\"\n#define F_Rect_contains F_Draw_Rect_contains\n'+EXTRACT((ROOT/'libinterp/geom.c').read_text(),'void\nRect_contains(')+'\n'
 return prefix+'\n#line 1 "native64-wmsrv-windows.c"\n'+(ROOT/'utils/tests/native64-wmsrv-windows.c').read_text().replace(' LIBRARY_HANDLERS',original[start:end]+' optab[IBEQC]=actual_beqc; optab[IBNEC]=actual_bnec; optab[IRAISE]=actual_iraise;')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 if not __debug__:p.error('source checks require assertions')
 with tempfile.TemporaryDirectory(prefix='wm-windows-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O0 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-typed-alt.h','native64-library-code.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
  header=(ROOT/'include/native64-library-layout.h').read_text();before='return id==6?0x90:0;';assert header.count(before)==1;(w/'native64-library-layout.h').write_text(header.replace(before,'return id==6?0x00:0;'))
  subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'complete Client allocation/map'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS window active control: lost Client trailing GC roots detected',flush=True)
  (w/'native64-library-layout.h').write_text(header)
  good=EXTRACT((ROOT/'libinterp/geom.c').read_text(),'void\nRect_contains(');before='memmove(f->ret, &result, sizeof(result));';assert good.count(before)==1
  bad=good.replace(before,'result ^= 1;'+before);runtime=unit();assert runtime.count(good)==1;(w/'unit.c').write_text(runtime.replace(good,bad))
  subprocess.run(command,check=True);result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
  assert result.returncode==10 and 'Window Rect half-open containment'in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print('PASS containment active control: inverted real Rect.contains result detected',flush=True)


if __name__=='__main__':main()
