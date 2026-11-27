#!/usr/bin/env python3
"""Execute generic nested ADT Word/root fields with actual heap-Type guards."""
import argparse,copy,os,re,runpy,shlex,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/native64-mixed-arrays.py'));G,CMD,E=BASE['G'],BASE['CMD'],BASE['E']
KINDS=('ref Box','string','array of byte','list of string','chan of int','Records')
FIELDS=('inner.link','inner.text','ary','seq','pipe','mod')
TYPES='Box:adt{v:int;};Inner:adt{number:int;text:string;link:ref Box;};Record:adt{pad:int;inner:Inner;ary:array of byte;seq:list of string;pipe:chan of int;mod:Records;left,right,ending:int;};'
ARGS='r:ref Record,x:ref Box,s:string,a:array of byte,l:list of string,c:chan of int,m:Records,n:int'
NAMES=('run',)+tuple('get'+str(k)for k in range(6))+('clear','globalrun')
DECL='Records:module{'+TYPES+''.join(name+':fn('+ARGS+'):'+ (KINDS[int(name[-1])]if name.startswith('get')else'int')+';'for name in NAMES)+'};'
SOURCE='implement Records;'+DECL+'saved:ref Record;out:ref Box;lens,lena,lenl,first:int;'
for name in NAMES:
 owner='saved'if name=='globalrun'else'r';field=lambda n:owner+'.'+n
 body='saved=r;'+field('inner.number')+'=n;'+field('left')+'=17;'+field('right')+'=-7;'+field('ending')+'=-31;'
 for f,v in zip(FIELDS,('x','s','a','l','c','m')):body+=field(f)+'='+('nil'if name=='clear'else v)+';'
 if name!='clear':body+=field('inner.link')+'=nil;'+field('inner.link')+'=x;'
 body+=field('left')+'+='+field('right')+';'+field('left')+'*=3;'+field('left')+'/=2;'+field('left')+'%=11;'+field('left')+'|=16;'+field('left')+'&=31;'+field('left')+'^=3;'+field('left')+'<<=1;'+field('left')+'>>=1;'+field('ending')+'--;'
 body+='if('+field('inner.link')+'!=nil)'+field('left')+'++;if('+field('inner.text')+'==nil)'+field('right')+'=29;'
 body+='out='+field('inner.link')+';lens=len '+field('inner.text')+';lena=len '+field('ary')+';lenl=len '+field('seq')+';first=-1;if(lens>0)first='+field('inner.text')+'[0];'
 body+='return '+field(FIELDS[int(name[-1])]if name.startswith('get')else'inner.number')+';'
 SOURCE+=name+'('+ARGS+'):'+(KINDS[int(name[-1])]if name.startswith('get')else'int')+'{'+body+'}'

def generate(limbo,w):
 path=w/'records.m';path.write_text(DECL)
 header=subprocess.check_output([str(limbo),'-H','-M','Records',str(path)],text=True);(w/'record-native.h').write_text(header)
 signatures={n:int(v,16)for n,v in re.findall(r'^#define LIMBO_NATIVE64_SIGNATURE_Records_(\w+) (0x[0-9a-f]+)U',header,re.M)}
 rows=G['generate'](limbo,w,SOURCE,'Records',(len(NAMES),0,0,'~0U'),export_signatures=signatures)
 p=w/'full.h';text=p.read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','');gl={v[0]:v[1]for v in rows[0]['globals']}
 text+='\nstatic const uint record_globals[]={'+','.join(str(gl[n])for n in('saved','out','lens','lena','lenl','first'))+'};\n'
 text+='static const uint record_functions[]={'+','.join(str(next(i for i,r in enumerate(rows)if r['name']==n))for n in NAMES)+'};\n';p.write_text(text)
 data,reports=E['compile_source'](limbo,w,SOURCE,interface='',loads=True);controls=0
 for row in reports:
  for pc,c in enumerate(row['code']):
   if c[5]!=72:continue
   for k in range(6):
    bad=copy.deepcopy(row);bad['code'][pc][k]+=1
    try:CMD['verify_nilbranches'](data,bad,[],rows=reports)
    except(AssertionError,KeyError,IndexError,StopIteration,ValueError):controls+=1
    else:raise AssertionError(('record field oracle missed',row['name'],pc,k))
 print('PASS record operand/recursive geometry oracle:',controls,'mutations; exact classic bytes and natural C field witnesses',flush=True)
 # Layout-tree geometry is checked independently from the instruction offsets.
 for row in reports:
  for i,tree in enumerate(row['record_types']):
   bad=copy.deepcopy(row);bad['record_types'][i][1]-=1
   try:CMD['verify_nilbranches'](data,bad,[],rows=reports)
   except(AssertionError,KeyError,IndexError,StopIteration,ValueError):pass
   else:raise AssertionError('record extent mutation missed')

def frontier(limbo,w):
 f=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'))['checked_frontier'](limbo,'appl/cmd/sh/sh.b',w)
 assert next(r for r in f['functions']if r['name']=='ungetc')['accepted']
 get=next(r for r in f['functions']if r['name']=='getc');assert get['accepted']or get['pc']>get['first']+1,get
 print('PASS unchanged YYLEX.ungetc; getc frontier:',get.get('pc'),get.get('reason'),flush=True)

def boundaries(limbo,w):
 for ty in('big','real','byte'):
  source='implement Closed;Closed:module{Record:adt{value:'+ty+';};run:fn(r:ref Record):'+ty+';};run(r:ref Record):'+ty+'{return r.value;}'
  _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 fields=','.join('f'+str(i)for i in range(1026))
 source='implement Closed;Closed:module{Record:adt{'+fields+':int;};run:fn(r:ref Record):int;};run(r:ref Record):int{return r.f1025;}'
 _,rows=E['compile_source'](limbo,w,source,interface='',loads=True);assert not rows[0]['accepted'],rows
 print('PASS generic field boundaries: byte/big/real field operations and offsets beyond 4096 remain closed',flush=True)

def lexer(limbo,w):
 sh=(ROOT/'appl/cmd/sh/sh.b').read_text();extract=CMD['NS']['S']['FILES']['FILES']['function']
 decl=''.join(re.search(r'(?m)^'+n+r':\s*adt\s*\{.*?^\};',sh,re.S).group(0)for n in('YYSTYPE','YYLEX'))
 # Isolate the two complete methods; omit declarations of unused methods.
 # All data fields and both executable bodies remain byte-for-byte original.
 for name in('initstring','initfile','lex','getc'):decl=re.sub(r'(?m)^\s*'+name+r':\s*fn\([^;]+;','',decl)
 interface='include "sys.m";include "draw.m";include "bufio.m";include "sh.m";Node:type Sh->Node;Redir:type Sh->Redir;'+decl+'Lexer:module{run:fn(lex:ref YYLEX,e:string);};'
 source='implement Lexer;'+interface+'run(lex:ref YYLEX,e:string){lex.error(e);lex.ungetc();}'+extract(sh,'YYLEX.error(')+extract(sh,'YYLEX.ungetc(')
 sub=w/'lexer';sub.mkdir();path=sub/'lexer.m';path.write_text(interface)
 header=subprocess.check_output([str(limbo),'-H','-M','Lexer',str(path)],text=True);(w/'lexer-native.h').write_text(header)
 sig=int(re.search(r'#define LIMBO_NATIVE64_SIGNATURE_Lexer_run (0x[0-9a-f]+)U',header).group(1),16)
 G['generate'](limbo,sub,source,'Lexer',(3,0,0,'~0U'),export_signatures={'run':sig})
 text=(sub/'full.h').read_text().replace('static const Native64ModuleVariadic full_variadics[]={};\n','').replace('full_','lexer_')
 (w/'lexer-full.h').write_text(text)
 print('PASS lexer source fixture: unchanged error/ungetc bodies and data declarations, separate local-call wrapper',flush=True)

def unit():
 src=BASE['unit']().split('\n#line 1 "native64-mixed-arrays.c"')[0]
 extract=CMD['NS']['S']['FILES']['FILES']['function'];vm=(ROOT/'libinterp/xec.c').read_text()
 for name in('static void\nbranchstring(','OP(beqc)','OP(bnec)'):src+='\n'+extract(vm,name)
 src+='\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(indc)')
 at=src.index(' optab[IMOVW]=countmovw;');handlers=src[at:src.index(' require(prepare64calls(',at)]
 return src+'\n#line 1 "native64-record-fields.c"\n'+(ROOT/'utils/tests/native64-record-fields.c').read_text().replace('RECORD_HANDLERS',handlers)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);a=p.parse_args();limbo=a.limbo.resolve()
 if not __debug__:p.error('source oracle requires assertions')
 with tempfile.TemporaryDirectory(prefix='record-fields-',dir=ROOT)as td:
  w=Path(td);CMD['generate'](limbo,w,'Command',False,argv=True,local=True,pair=True,raises=True);generate(limbo,w);boundaries(limbo,w);frontier(limbo,w);lexer(limbo,w);(w/'unit.c').write_text(unit())
  arch=os.environ.get('NATIVE64_ARCH','amd64');cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
  if arch in('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
  cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1','-DNAMESPACE_HEADER="provider.h"','-DNAMESPACE_ORDINAL=LIMBO_NATIVE64_IMPORT_Library_mid','-I'+str(w)]+['-I'+str(ROOT/x)for x in('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
  binary=w/'unit';command=cc+[str(w/'unit.c'),str(ROOT/'libinterp/dec.c')]+CMD['FILES']['releaseflags'](cc)+['-lm','-o',str(binary)];runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)]
  subprocess.run(command,check=True);subprocess.run(runner,check=True,timeout=180)
  parents=('native64-local-bind.h','native64-typed-module.h','native64-typed-multi.h','native64-typed-code.h','native64-raw-array-runtime.h','native64-record-runtime.h','native64-record-code.h','native64-record-module.h')
  for name,header,before,after,predicate in (
   ('field extent','native64-record-runtime.h','width>(unsigned int)t->size-off','0','heap extent rejected before opcode dispatch'),
   ('field GC role','native64-record-runtime.h','if(!!(bit/8<(unsigned int)t->np&&(t->map[bit/8]&(128>>(bit%8))))!=root)error(exBounds);','','heap GC role rejected before opcode dispatch'),
   ('field displacement','native64-record-code.h','p->s.i.s=(unsigned long)s->s>>16;','p->s.i.s=0;','exact record completion/error')):
   for parent in parents:(w/parent).write_text((ROOT/'include'/parent).read_text())
   path=w/header;text=path.read_text();assert text.count(before)==1;path.write_text(text.replace(before,after,1));subprocess.run(command,check=True);result=subprocess.run(runner,capture_output=True,text=True,timeout=180)
   assert result.returncode==10 and predicate in result.stderr and 'Sanitizer'not in result.stderr and 'runtime error:'not in result.stderr,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
   for parent in parents:(w/parent).unlink()
   print('PASS record field active control:',name,flush=True)
if __name__=='__main__':main()
