#!/usr/bin/env python3
"""Compile numeric case/goto bodies and execute relocated tables through real module calls."""
import argparse
import json
import os
import re
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
OP={name:i for i,name in enumerate(re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT/'include/isa.h').read_text().split('};',1)[0], re.M))}
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-body.py'))
SOURCE='''implement Switches;
Switches:module{pad:fn(n:int):int;sparse:fn(n:int):int;wide:fn(n:big):int;dense:fn(n:int):int;};
seed:int=41;
pad(n:int):int{return n+seed;}
sparse(n:int):int{case n{-100 to -90=>return 11;5=>return 22;1000 to 1010=>return 33;2147483647=>return 44;* =>return 55;}}
wide(n:big):int{case n{big -1099511627776=>return 66;big 1099511627776=>return 77;big 9223372036854775807=>return 88;* =>return 99;}}
dense(n:int):int{case n{-2=>return 101;-1=>return 102;0=>return 103;1=>return 104;2=>return 105;3=>return 106;4=>return 107;5=>return 108;* =>return 109;}}
'''


def generate(limbo,work):
    data,rows=BASE['compile_source'](limbo,work,SOURCE)
    assert len(rows)==4 and all(r['accepted'] for r in rows),[(r['name'],r.get('reason')) for r in rows]
    header,code,types,initial,module,exports=BASE['READ']['legacy_instructions'](data,with_exports=True)
    exports={e['name']:e for e in exports}
    lines=['#include "native64-typed-module.h"']; spans=[]; ops=set()
    for r in rows:
        BASE['verify'](data,r)
        name=r['name']; tables=r.get('tables',[])
        lines += [f'static const Native64TypedStep code_{name}[]={{']+['{'+','.join(map(str,x))+'},' for x in r['code']]+['};',
                  f'static const uchar map_{name}[]={{'+','.join(map(str,r['frame']['map']))+'};']
        for j,t in enumerate(tables):
            pc=r['first']+t['pc'];old=code[pc];ops.add(t['op'])
            assert old['op']==t['op'] and t['pc']<len(r['code'])
            goto=t['op']==OP['IGOTO'];big=t['op']==OP['ICASEL']
            oldbase=old['d'][0]-(4 if goto else 0)
            oldcount=initial[oldbase]['low']
            assert oldcount==len(t['rows']) or goto and oldcount==len(t['rows'])+1
            h=8 if big else 4;stride=4 if goto else 24 if big else 12
            nativebytes=h+len(t['rows'])*stride+(0 if goto else 4)
            # The classic goto serializer may retain one unused default cell;
            # its source bounds guards make it unreachable. Do not copy an old PC.
            spans.append((t['offset'],t['offset']+nativebytes+(4 if goto and oldcount>len(t['rows']) else 0)))
            for k,(low,high,target) in enumerate(t['rows']):
                at=oldbase+h+k*stride
                assert 0<=target<len(r['code'])
                if not goto:
                    mask=(1<<(64 if big else 32))-1;maximum=(1<<(63 if big else 31))-1
                    assert ((initial[at]['high']<<32)|initial[at]['low'])==low&mask
                    assert ((initial[at+(8 if big else 4)]['high']<<32)|initial[at+(8 if big else 4)]['low'])==(high if high==maximum else high+1)&mask
                assert initial[at+(0 if goto else 16 if big else 8)]['low']==r['first']+target
            if not goto: assert initial[oldbase+h+len(t['rows'])*stride]['low']==r['first']+t['fallback']
            lines += [f'static const Native64ModuleCaseRow rows_{name}_{j}[]={{']+['{'+f'{low}LL,{high}LL,{target}'+'},' for low,high,target in t['rows']]+['};']
        if tables:
            lines += [f'static const Native64ModuleCase tables_{name}[]={{']+['{'+f'{t["pc"]},{t["offset"]},{t["op"]},{len(t["rows"])},'+('~0U' if t['fallback']<0 else str(t['fallback']))+f',rows_{name}_{j}'+'},' for j,t in enumerate(tables)]+['};']
    assert ops=={OP['IGOTO'],OP['ICASE'],OP['ICASEL']},ops
    assert rows[0]['name']=='pad' and all(r['first']>0 for r in rows if r.get('tables'))
    records=[]
    for old,cell in sorted(initial.items()):
        width={'word':4,'long':8}[cell['kind']]
        leaf=next(l for l in rows[0]['data']['leaves'] if l[0]<=old and old+width<=l[0]+l[2])
        at=leaf[1]+old-leaf[0]
        if any(lo<=at<hi for lo,hi in spans): continue
        assert leaf[4]>=0 and leaf[2]==leaf[3]
        records.append((at,width,(cell['high']<<32)|cell['low']))
    lines += ['static const Native64TypedStep prefix[]={{IRET,SRC(AXXX)|DST(AXXX),0,0,0,0}};',
              'static const uchar prefixmap[]={0};',
              'static const Native64ModuleFunction functions[]={',
              '{"private_prefix",0,0,prefix,1,64,prefixmap,1,nil,0,nil,0},']
    for r in rows:
        n=r['name'];t=r.get('tables',[]);e=exports[n]
        assert (e['pc'],e['descriptor'])==(r['first'],r['descriptor'])
        lines += ['{'+f'"{n}",{e["signature"]}U,1,code_{n},{len(r["code"])},{r["frame"]["bytes"]},map_{n},{len(r["frame"]["map"])},nil,0,'+(f'tables_{n}' if t else 'nil')+f',{len(t)}'+'},']
    lines += ['};','static const Native64ModuleInit initial[]={']+['{'+f'{a},{w},{v}ULL'+'},' for a,w,v in records]+['};',
              'static const uchar datamap[]={'+','.join(map(str,rows[0]['data']['map']))+'};',
              'static const Native64ModuleSpec spec={"Switches","trusted/Switches",functions,5,~0U,'+f'{rows[0]["data"]["bytes"]},datamap,{len(rows[0]["data"]["map"])},initial,{len(records)}'+'};',
              'static const unsigned legacy_first[]={'+','.join(str(r['first']) for r in rows)+'};',
              'static const unsigned args[]={'+','.join(str(r['args'][0][0]) for r in rows)+'};']
    (work/'typed-tables.h').write_text('\n'.join(lines)+'\n')
    print('PASS real Limbo case/long-case/computed-goto: exact classic bounds, targets, exports, frame/data maps and shifted function ownership',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',type=Path,required=True);a=parser.parse_args()
    if not __debug__:parser.error('independent classic/typed oracles require Python assertions')
    with tempfile.TemporaryDirectory(prefix='typed-tables-',dir=ROOT) as td:
        work=Path(td);generate(a.limbo.resolve(),work)
        text=runpy.run_path(str(ROOT/'utils/tests/native64-typed-module.py'))['unit']()
        text+='\nHeap*heaparray(Type*t,int n){USED(t);USED(n);require(0);return nil;}\n'
        extract=runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['function']
        xec=(ROOT/'libinterp/xec.c').read_text()
        text+='\n'+extract(xec,'static Inst*\ncasepc(')+'\n'+'\n'.join(extract(xec,'OP('+n+')') for n in ('icase','casel','igoto'))+'\n'
        text+=(ROOT/'utils/tests/native64-typed-tables.c').read_text()
        source=work/'test.c';source.write_text(text)
        cc=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        arch=os.environ.get('NATIVE64_ARCH','amd64')
        if arch in ('arm64','riscv64'):cc+=['-DDIS64_TEST_'+arch.upper()+'=1','-DDIS64_TEST_COMPILER="comp-'+arch+'.c"']
        elif arch!='amd64':parser.error(arch)
        cc+=['-DDIS_WORD32=1','-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        binary=work/'test';command=cc+[str(source),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)]
        subprocess.run(command,check=True);subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],check=True,timeout=180)
        production=(ROOT/'include/native64-typed-tables.h').read_text()
        # The owning header uses a quoted include; place both beside each other
        # so deliberate mutations cannot silently fall back to the repo copy.
        (work/'native64-typed-module.h').write_text((ROOT/'include/native64-typed-module.h').read_text())
        for label,before,after,marker in (
            ('row-owner','r->target>=f->count','0','table metadata refusal removed: case=0'),
            ('relocate','offsets[i]+t->rows[k].target','t->rows[k].target','typed table predicate'),
        ):
            assert before in production
            (work/'native64-typed-tables.h').write_text(production.replace(before,after))
            subprocess.run(command,check=True)
            result=subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER',''))+[str(binary)],capture_output=True,text=True,timeout=180)
            assert result.returncode==10 and marker in result.stderr and 'Sanitizer' not in result.stderr and 'runtime error:' not in result.stderr,result.stderr
            print('PASS active table control:',arch,label,flush=True)


if __name__=='__main__':main()
