#!/usr/bin/env python3
"""Execute selected Draw payload roots and field arguments with controlled native Draw services."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
G=runpy.run_path(str(ROOT/'utils/tests/native64-emuinit-full.py'));CMD=G['CMD'];E=G['E']
PREFIX='include "sys.m";include "draw.m";draw:Draw;Image,Screen,Display:import draw;'
PROFILES=[('im','Image',[('display','ref Display'),('screen','ref Screen'),('iname','string')]),('sc','Screen',[('image','ref Image'),('fill','ref Image'),('display','ref Display')]),('di','Display',[(n,'ref Image')for n in('image','white','black','opaque','transparent')])]
def source():
 declarations=['open:fn(path:string);','color:fn(obj:ref Image):ref Image;','allocate:fn(obj:ref Image):ref Screen;']+[n+':fn(obj:ref Image);'for n in('globalcolor','globalallocate','localcolor','localallocate','drawrect','drawclip','drawmin','drawmax','clipmin','clipmax')];globals=['image_result:ref Image;','screen_result:ref Screen;','screen:ref Screen;'];bodies=[]
 for prefix,owner,fields in PROFILES:
  names=[prefix+'_'+n for n,_ in fields];globals +=[name+':'+kind+';'for name,(_,kind)in zip(names,fields)]+[prefix+'_saved:ref '+owner+';']
  for action in('capture','publish','captureglobal','publishglobal','forget','forgetglobal','roundtrip'):
   fn=prefix+'_'+action;local=not action.endswith('global');argument='obj:ref '+owner if local else'';declarations.append(fn+':fn('+argument+');');base='obj'if local else prefix+'_saved';body=''
   for i,((field,kind),name)in enumerate(zip(fields,names)):
    if action.startswith('capture'):body+=(name+'='+base+'.'+field+';'if kind=='string'else'if('+base+'.'+field+'==nil)'+name+'=nil;else '+name+'='+base+'.'+field+';')
    elif action.startswith('publish'):body+=base+'.'+field+'='+name+';'
    elif action.startswith('forget'):body+=base+'.'+field+'=nil;'
   if action=='roundtrip':body=''.join('v'+str(i)+':='+base+'.'+f+';'for i,(f,k)in enumerate(fields))+''.join(base+'.'+f+'=v'+str(i)+';'+names[i]+'=v'+str(i)+';'for i,(f,k)in enumerate(fields))
   bodies.append(fn+'('+argument+'){'+body+'}')
 declarations.append('makescreen:fn(obj:ref Image):ref Screen;')
 return 'implement DrawFields;'+PREFIX+'DrawFields:module{'+''.join(declarations)+'};'+''.join(globals)+'open(path:string){draw=load Draw path;}color(obj:ref Image):ref Image{return obj.display.color(123);}allocate(obj:ref Image):ref Screen{return Screen.allocate(obj,obj.display.color(123),0);}'+''.join(bodies)+'globalcolor(obj:ref Image){image_result=obj.display.color(123);}globalallocate(obj:ref Image){screen_result=Screen.allocate(obj,obj.display.color(123),0);}localcolor(obj:ref Image){image_result=color(obj);}localallocate(obj:ref Image){screen_result=allocate(obj);}drawrect(obj:ref Image){obj.draw(obj.r,nil,nil,(0,0));}drawclip(obj:ref Image){obj.draw(obj.clipr,nil,nil,(0,0));}drawmin(obj:ref Image){obj.draw(obj.r,nil,nil,obj.r.min);}drawmax(obj:ref Image){obj.draw(obj.r,nil,nil,obj.r.max);}clipmin(obj:ref Image){obj.draw(obj.r,nil,nil,obj.clipr.min);}clipmax(obj:ref Image){obj.draw(obj.r,nil,nil,obj.clipr.max);}' + re.search(r'Background: con[^;]+;', (ROOT/'appl/wm/awm.b').read_text())[0] + re.search(r'makescreen\(img: ref Image\): ref Screen\s*\{.*?\n\}', (ROOT/'appl/wm/awm.b').read_text(),re.S)[0]
SOURCE=source()
def generate(limbo,w):
 m=w/'drawfields.m';m.write_text(PREFIX+re.search(r'DrawFields:module\{.*?\};',SOURCE)[0]);h=subprocess.check_output([str(limbo),'-H','-M','DrawFields','-I',str(ROOT/'module'),str(m)],text=True)
 canonical={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_DrawFields_(\w+) (0x[0-9a-f]+)U',h,re.M)}
 h=subprocess.check_output([str(limbo),'-H','-M','Draw','-I',str(ROOT/'module'),str(m)],text=True);(w/'draw-native.h').write_text(h);frames={}
 for n in('Display_color','Screen_allocate','Image_draw'):
  size=int(re.search(r'^#define F_Draw_'+n+r'_size (\d+)',h,re.M)[1]);mapping=[int(v,0)for v in re.search(r'^#define F_Draw_'+n+r'_map \{([^}]*)\}',h,re.M)[1].split(',')if v];signature=int(re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_Draw_'+n+r' (0x[0-9a-f]+)U',h,re.M)[1],16);frames[(n.replace('_','.'),signature)]=(size,mapping)
 rows=G['generate'](limbo,w,SOURCE,'DrawFields',(35,1,0,'~0U'),import_frames=frames,export_signatures=canonical);f=w/'full.h';f.write_text(f.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n',''))
 with f.open('a')as out:
  globals={g[0]:g[1]for g in rows[0]['globals']};out.write('static const uint draw_result_offsets[]={'+str(globals['image_result'])+','+str(globals['screen_result'])+'};\n');out.write('static const uint draw_module_offset='+str(globals['draw'])+';\n')
  for prefix,owner,fields in PROFILES:out.write('static const uint draw_offsets_'+prefix+'[]={'+','.join(str(globals[prefix+'_'+n])for n,k in fields)+','+str(globals[prefix+'_saved'])+'};\n')
  out.write('static const uint*draw_offsets[]={draw_offsets_im,draw_offsets_sc,draw_offsets_di};\n')
 data,records=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1];controls=0
 for r in records:
  for pc,c in enumerate(r['code']):
   if c[5]not in range(113,127)and c[5]not in range(160,167)and not(c[0]==39 and c[1]==5):continue
   for k in range(6):
    bad=copy.deepcopy(r);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,imports,rows=records)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('Draw oracle missed',r['name'],pc,k))
 print('PASS Draw source oracle:',controls,'field/argument/clear record corruptions; canonical headers/hashes and classic bytes preserved',flush=True)
def refusals(limbo,w):
 for declaration,owner,expression,kind in (
  ('Other:adt{display:ref Display;};','Other','obj.display','ref Display'),
  ('Fake:module{Image:adt{display:ref Draw->Display;};};','Fake->Image','obj.display','ref Display'),
  ('','Image','obj.r','Draw->Rect')):
  text='implement Closed;'+PREFIX+declaration+'Closed:module{run:fn(obj:ref '+owner+');};value:'+kind+';run(obj:ref '+owner+'){value='+expression+';}'
  _,rows=E['compile_source'](limbo,w,text,interface='',loads=True)
  assert len(rows)==1 and not rows[0]['accepted'],rows
 print('PASS Draw compiler boundary: foreign owner, wrong namespace and aggregate field refused; classic bytes preserved',flush=True)
 text='implement FieldLocal;'+PREFIX+'FieldLocal:module{run:fn(obj:ref Image);};value:ref Display;sink(v:ref Display){value=v;}run(obj:ref Image){sink(obj.display);}'
 data,rows=E['compile_source'](limbo,w,text,interface='',loads=True);imports=E['BASE']['READ']['legacy_instructions'](data,with_imports=True)[-1]
 assert all(r['accepted']for r in rows)and any(c[5]==125 for r in rows for c in r['code']),rows
 for r in rows:CMD['verify_nilbranches'](data,r,imports,rows=rows)
 print('PASS Draw local field argument: original classic call/operand oracle',flush=True)
def result_refusals(limbo,w):
 for text in (
  'implement Closed;Sink:module{value:fn():int;};Closed:module{run:fn(s:Sink);};value:int;run(s:Sink){value=s->value();}',
  'implement Closed;Closed:module{run:fn();};value:int;word():int{return 123;}run(){value=word();}'):
  _,rows=E['compile_source'](limbo,w,text,interface='',loads=True)
  assert not next(r for r in rows if r['name']=='run')['accepted'],rows
 print('PASS global result boundary: imported/local Word globals remain refused; classic bytes preserved',flush=True)
def unit():
 src=CMD['unit']().replace('int main(int argc,char **argv)','int previous_command_main(int argc,char **argv)');extract=CMD['NS']['S']['FILES']['FILES']['function'];src+='\n'+extract((ROOT/'libinterp/xec.c').read_text(),'OP(movm)')+'\n';start=src.index(' optab[IMOVW]=countmovw;');end=src.index(' require(prepare64calls(',start)
 return src+'\n#line 1 "native64-draw-fields.c"\n'+(ROOT/'utils/tests/native64-draw-fields.c').read_text().replace(' DRAW_HANDLERS',src[start:end])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',required=True,type=Path);a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='draw-fields-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](a.limbo.resolve(),w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(a.limbo.resolve(),w);refusals(a.limbo.resolve(),w);result_refusals(a.limbo.resolve(),w);(w/'unit.c').write_text(unit());arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')];binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
  for name,path,before,after,predicate in (
   ('field-displacement','native64-typed-code.h','p->s.i.s=s->m;','p->s.i.s=s->m==48?56:s->m;','exact MP field value'),
   ('field-argument','native64-typed-multi.h','p->s.i.f=s->s;p->s.i.s=s->m;p->d.i.f=40;','p->s.i.f=s->s;p->s.i.s=s->m==48?56:s->m;p->d.i.f=40;','actual Display.color prefix/receiver'),
   ('temporary-clear','native64-typed-multi.h','else if(s->returned==Native64TypedArgClear){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}',
    'else if(s->returned==Native64TypedArgClear){checkstep=*s;checkstep.returned=0;checkstep.mode=SRC(AFP)|DST(AFP);checkstep.s=checkstep.d;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}', 'temporary fill cleared before allocate'),
   ('global-result-address','native64-typed-multi.h','else{p->s.imm=s->s;p->d.i.f=40;','else{p->s.imm=s->s+(s->op==ILEA && USRC(s->mode)==AMP?8:0);p->d.i.f=40;','result replacement/failure preserved'),
   ('global-result-rootmap','native64-local-bind.h','if(!!(bit/8<','if(USRC(instruction.add)!=AMP && !!(bit/8<','field argument execution completed'),
   ('geometry-temporary-clear','native64-typed-multi.h','else if(s->returned==Native64TypedArgClear){checkstep=*s;checkstep.returned=0;native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}',
    'else if(s->returned==Native64TypedArgClear){checkstep=*s;checkstep.returned=0;if(j>i && steps[j-1].op==IMOVM){checkstep.mode=SRC(AFP)|DST(AFP);checkstep.s=checkstep.d;}native64typedcode(p,1,&checkstep,1,fpbytes,mpbytes);}', 'geometry temporary retired before draw'),
   ('image-rect-offset','native64-typed-multi.h','p->s.i.s=native64imagegeometryoffset(s->returned);','p->s.i.s=native64imagegeometryoffset(s->returned)^16;','exact Image Rect argument')):
   original=(ROOT/'include'/path).read_text();assert original.count(before)==1
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-local-bind.h'):(w/parent).write_text((ROOT/'include'/parent).read_text())
   (w/path).write_text(original.replace(before,after,1));subprocess.run(command,check=True)
   r=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
   assert r.returncode==10 and predicate in r.stderr and 'Sanitizer'not in r.stderr and 'runtime error:'not in r.stderr,(name,r.returncode,r.stdout,r.stderr)
   for parent in('native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-local-bind.h'):(w/parent).unlink()
   print('PASS Draw active control:',name,flush=True)
if __name__=='__main__':main()
