#!/usr/bin/env python3
"""Execute mixed Word32/ref64 String tables from real Limbo, with owned MP and JIT neighbours."""
import argparse
import json
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-body.py'))
DATA=runpy.run_path(str(ROOT/'utils/tests/awm-native64-data.py'))
SOURCE='''implement Strings;
Strings:module{pad:fn(n:int):int;choose:fn(s:string):int;global:fn(s:string):int;size:fn(n:int):int;};
current:string;fallback:string="fallback🚀";
pad(n:int):int{return n+7;}
choose(s:string):int{case s{"" =>return 1;"\\0"=>return 6;"a" to "c"=>return 2;"x\\0z"=>return 7;"Привет"=>return 3;"🚀"=>return 4;* =>return 5;}}
global(s:string):int{current=s;case current{"" =>return 1;"\\0"=>return 6;"a" to "c"=>return 2;"x\\0z"=>return 7;"Привет"=>return 3;"🚀"=>return 4;* =>return 5;}}
size(n:int):int{return n+len fallback;}
'''

def raw(lines,name,text):
    value=text.encode('utf-8') if isinstance(text,str) else bytes(text)
    lines.append('static const uchar '+name+'[]={'+','.join(map(str,value or b'\0'))+'};')
    return name,len(value)

def generate(limbo,work):
    data,rows=BASE['compile_source'](limbo,work,SOURCE)
    assert len(rows)==4 and all(r['accepted'] for r in rows),[(r['name'],r.get('reason')) for r in rows]
    header,code,types,initial,module,exports=BASE['READ']['legacy_instructions'](data,with_exports=True)
    exports={e['name']:e for e in exports};lines=['#include "native64-typed-module.h"'];oldspans=[]
    for r in rows:
        BASE['verify'](data,r);name=r['name'];tables=r.get('tables',[])
        lines += [f'static const Native64TypedStep code_{name}[]={{']+['{'+','.join(map(str,x))+'},' for x in r['code']]+['};',
                  f'static const uchar map_{name}[]={{'+','.join(map(str,r['frame']['map']))+'};']
        for j,t in enumerate(tables):
            assert t['op']==144
            oldbase=code[r['first']+t['pc']]['d'][0];oldspans.append((oldbase,oldbase+8+12*len(t['rows'])))
            cells=[]
            for k,(low,high,target) in enumerate(t['rows']):
                a,na=raw(lines,f'text_{name}_{j}_{k}_low',low)
                b,nb=('nil',0) if high is None else raw(lines,f'text_{name}_{j}_{k}_high',high)
                cells.append('{'+f'0,0,{target},{a},{b},{na},{nb}'+'},')
            lines += [f'static const Native64ModuleCaseRow rows_{name}_{j}[]={{']+cells+['};']
        if tables:
            lines += [f'static const Native64ModuleCase tables_{name}[]={{']+['{'+f'{t["pc"]},{t["offset"]},{t["op"]},{len(t["rows"])},{t["fallback"]},rows_{name}_{j}'+'},' for j,t in enumerate(tables)]+['};']
    strings=[];scalars=[]
    for old,cell in sorted(initial.items()):
        if any(a<=old<b for a,b in oldspans):continue
        width={'string':4,'word':4,'long':8,'real':8,'byte':1}[cell['kind']]
        leaf=next(l for l in rows[0]['data']['leaves'] if l[0]<=old and old+width<=l[0]+l[2]);at=leaf[1]+old-leaf[0]
        if cell['kind']=='string':
            assert leaf[4]==13 and leaf[2:4]==[4,8]
            text,n=raw(lines,'literal_'+str(old),cell['bytes']);strings.append('{'+f'{at},{n},{text}'+'},')
        else:
            assert leaf[4]>=0 and leaf[2]==leaf[3]
            scalars.append('{'+f'{at},{width},{(cell["high"]<<32)|cell["low"]}ULL'+'},')
    lines += ['static const Native64TypedStep prefix[]={{IRET,SRC(AXXX)|DST(AXXX),0,0,0,0}};',
              'static const uchar prefixmap[]={0};','static const Native64ModuleFunction functions[]={',
              '{"private_prefix",0,0,prefix,1,64,prefixmap,1,nil,0,nil,0},']
    for r in rows:
        n=r['name'];t=r.get('tables',[]);e=exports[n];assert (e['pc'],e['descriptor'])==(r['first'],r['descriptor'])
        lines += ['{'+f'"{n}",{e["signature"]}U,1,code_{n},{len(r["code"])},{r["frame"]["bytes"]},map_{n},{len(r["frame"]["map"])},nil,0,'+(f'tables_{n}' if t else 'nil')+f',{len(t)}'+'},']
    lines += ['};','static const Native64ModuleInit initial[]={']+(scalars or ['{0},'])+['};',
              'static const Native64ModuleString literals[]={']+strings+['};',
              'static const uchar datamap[]={'+','.join(map(str,rows[0]['data']['map']))+'};',
              'static const Native64ModuleSpec spec={"Strings","trusted/Strings",functions,5,~0U,'+f'{rows[0]["data"]["bytes"]},datamap,{len(rows[0]["data"]["map"])},initial,{len(scalars)},nil,0,c2string,literals,{len(strings)}'+'};',
              'static const unsigned args[]={'+','.join(str(r['args'][0][0]) for r in rows)+'};',
              'static const unsigned legacy_first[]={'+','.join(str(r['first']) for r in rows)+'};',
              'static const unsigned current_offset='+str(next(g[1] for g in rows[0]['globals'] if g[0]=='current'))+';']
    (work/'typed-string-tables.h').write_text('\n'.join(lines)+'\n')
    assert len(oldspans)==2 and strings
    print('PASS Limbo String tables: exact classic UTF-8, NUL, singleton/range/default targets, roots, global literal and shifted function coordinates',flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',type=Path,required=True);a=parser.parse_args()
    if not __debug__:parser.error('independent compiler/legacy oracles require Python assertions')
    with tempfile.TemporaryDirectory(prefix='typed-string-tables-',dir=ROOT) as td:
        work=Path(td);generate(a.limbo.resolve(),work)
        text=DATA['unit']().split('/* Actual constructor, String factory and mapped MP cloning; no AWM code body. */')[0]
        extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
        text+='\n'+extract((ROOT/'libinterp/string.c').read_text(),'OP(lenc)')+'\n'
        text+=(ROOT/'utils/tests/native64-typed-string-tables.c').read_text();source=work/'test.c';source.write_text(text)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':parser.error(arch)
        cc+=['-DDIS_WORD32=1','-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'test';command=cc+[str(source),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)]
        subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
        for name in ('native64-typed-module.h','native64-typed-tables.h','native64-string-tables.h'):
            (work/name).write_text((ROOT/'include'/name).read_text())
        original=(work/'native64-string-tables.h').read_text()
        body=extract(original,'static int\nnative64moduletext(')
        (work/'native64-string-tables.h').write_text(original.replace(body,'static int\nnative64moduletext(const uchar *p,unsigned int n){return p!=nil && p!=H && n<=4096;}',1))
        subprocess.run(command,check=True)
        result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
        assert result.returncode==10 and 'String metadata refusal removed: case=3' in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result.stderr
        print('PASS active String table control:',arch,'UTF-8 validation',flush=True)
        (work/'native64-string-tables.h').write_text(original)
        text=source.read_text();body=extract(text,'void\nnewmp(');assert body.count('h->ref++;')==1
        source.write_text(text.replace(body,body.replace('h->ref++;',';'),1));subprocess.run(command,check=True)
        result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
        assert result.returncode==10 and 'String table predicate' in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result.stderr
        print('PASS active String table control:',arch,'MP clone retain',flush=True)

if __name__=='__main__':main()
