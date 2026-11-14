#!/usr/bin/env python3
"""Independent typed-body relocation oracle and real-source runtime fixture producer."""
import argparse
import json
import os
from pathlib import Path
import resource
import runpy
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
READ=runpy.run_path(str(ROOT/'utils/tests/native64-callsites.py'))
SOURCE='''implement Kernel;
include "sys.m";
include "draw.m";
Kernel: module {
    run: fn(ctxt: ref Draw->Context, argv: list of string, text: string, n: int);
    mix: fn(x: big, y: real, k: int);
    guard: fn(text: string, n: int);
};
answer: int;
result: string;
outbig: big;
outreal: real;
run(nil: ref Draw->Context, argv: list of string, text: string, n: int)
{
    total:=0;
    while(n>0){total+=n;n--;}
    answer=total+len text+len argv;
    result=text;
}
mix(x: big, y: real, k: int)
{
    outbig=x+big k;
    outreal=y+real k;
}
guard(text: string, n: int)
{
    answer=-1;
    if(n<0)return;
    answer=len text;
    result=text;
}
'''


def compile_source(limbo,work,source,filename=None):
    file=Path(filename) if filename else work/'typed-body.b'
    if filename is None:file.write_text(source)
    ordinary,traced=work/'plain.dis',work/'typed.dis'
    command=[str(limbo),'-I',str(ROOT/'module'),'-o']
    env=dict(os.environ);env.pop('LIMBO_DIS64_TYPED',None)
    READ['checked_run'](command+[str(ordinary),str(file)],env=env,check=True,capture_output=True)
    result=READ['checked_run'](command+[str(traced),str(file)],env=dict(env,LIMBO_DIS64_TYPED='1'),check=True,capture_output=True,text=True)
    assert ordinary.read_bytes()==traced.read_bytes()
    records=[json.loads(line[12:]) for line in result.stderr.splitlines() if line.startswith('DIS64_TYPED ')]
    assert records
    return traced.read_bytes(),records


def verify(data,record):
    header,code,types=READ['legacy_instructions'](data,with_types=True)
    if not record['accepted']:
        assert 'code' not in record and 'frame' not in record and 'reason' in record
        return
    first=record['first']; offsets={}
    for name in ('frame','data'):
        layout=record[name];size=layout['bytes']; expected=[0]*((size+63)//64);oldmap=set();mapping={};used=set();last=0
        oldsize,serializedmap=types[record['descriptor' if name=='frame' else 'data_descriptor']]
        assert layout['legacy_bytes']==oldsize
        assert 0<=size<=1024*1024 and size%8==0
        for old,new,oldbytes,width,kind in layout['leaves']:
            assert old>=last and oldbytes>0 and old+oldbytes<=oldsize and width>0 and new+width<=size
            assert new>=(64 if name=='frame' else 0) and not used.intersection(range(new,new+width))
            used.update(range(new,new+width));last=old+oldbytes
            root=oldbytes==4 and width==8 and kind>=0
            if root: oldmap.add(old//4);expected[new//64]|=0x80>>(new//8%8)
            for byte in range(oldbytes): mapping[old+byte]=(new+byte,kind,root,oldbytes-byte)
        assert expected==layout['map']
        assert {i*8+b for i,value in enumerate(serializedmap) for b in range(8) if value&(0x80>>b)}==oldmap
        offsets[name]=mapping
    branches=set(range(87,111))|set(range(129,135))
    for index,(op,mode,s,m,d) in enumerate(record['code']):
        legacy=code[first+index]
        assert op==legacy['op'] and mode==legacy['mode']
        for label,address,old,new in (('s',(mode>>3)&7,legacy['s'],s),('d',mode&7,legacy['d'],d)):
            if old is None: assert new==0
            elif op in branches|{13} and address==2 and label=='d':
                assert new==old[0]-first and 0<=new<len(record['code'])
            elif address==2: assert new==old[0]
            else: assert address in (0,1) and new==offsets['frame' if address else 'data'][old[0]][0]
        if mode&0xc0:
            mm=mode&0xc0
            assert m==(legacy['m'] if mm==0x40 else offsets['frame' if mm==0x80 else 'data'][legacy['m']][0])
        else: assert m==0


def generate(limbo,work,env=None):
    data,records=compile_source(limbo,work,SOURCE)
    assert {r['name'] for r in records}=={'run','mix','guard'} and all(r['accepted'] for r in records),records
    lines=['typedef struct Native64TypedStep { unsigned int op,mode; long s,m,d; } Native64TypedStep;']
    for record in records:
        verify(data,record);name=record['name']
        lines+=['static const Native64TypedStep typed_'+name+'[] = {']+['{'+','.join(map(str,row))+'},' for row in record['code']]+['};']
        for space in ('frame','data'):
            layout=record[space]
            lines+=['static const unsigned int typed_'+name+'_'+space+'_bytes = '+str(layout['bytes'])+';',
                    'static const uchar typed_'+name+'_'+space+'_map[] = {'+','.join(map(str,layout['map'] or [0]))+'};']
        lines+=['static const unsigned int typed_'+name+'_args[] = {'+','.join(str(a[0]) for a in record['args'])+'};']
        for binding in record['globals']:
            label,offset=binding[:2]
            if label in ('answer','result','outbig','outreal'):lines+=['static const unsigned int typed_'+name+'_'+label+' = '+str(offset)+';']
    assert records[0]['args']==[[64,12,4,8],[72,10,4,8],[80,13,4,8],[88,9,4,4]]
    for fault in ('target','root','offset','descriptor'):
        altered=json.loads(json.dumps(records[0]))
        if fault=='target': next(row for row in altered['code'] if row[0]==13)[4]=99999
        elif fault=='root': altered['frame']['map'][1]^=0x80
        elif fault=='offset': altered['code'][0][4]+=4
        else: altered['descriptor']=altered['data_descriptor']
        try:verify(data,altered)
        except (AssertionError,KeyError):pass
        else:raise RuntimeError('Typed oracle accepted '+fault)
    (work/'typed-body-native64.h').write_text('\n'.join(lines)+'\n')
    for flag in ('-64','-64l'):
        target=work/('refused'+flag+'.dis64')
        attempt=READ['checked_run']([str(limbo),flag,'-I',str(ROOT/'module'),'-o',str(target),str(work/'typed-body.b')],
                                   env=dict(os.environ,LIMBO_DIS64_TYPED='1'),capture_output=True,text=True)
        assert attempt.returncode==1 and not target.exists() and 'Dis64 ' in attempt.stderr
        attempted=[json.loads(line[12:]) for line in attempt.stderr.splitlines() if line.startswith('DIS64_TYPED ')]
        assert attempted==records
    for source in (
        'implement Nope; Nope: module { init: fn(); }; init(){a:=array[2]of int; a[0]=1;}',
        'implement Nope; Nope: module { f: fn(): int; }; f():int{return 1;}',
    ):
        refused,rows=compile_source(limbo,work,source)
        assert any(not r['accepted'] for r in rows)
        for r in rows: verify(refused,r)
    compound='''implement Params; Params: module { Pair: adt { text: string; n: int; }; noop: fn(a: chan of string, b: Params, c: Pair); }; noop(nil: chan of string, nil: Params, nil: Pair) {}'''
    compound_data,compound_rows=compile_source(limbo,work,compound)
    assert len(compound_rows)==1 and compound_rows[0]['accepted']
    verify(compound_data,compound_rows[0])
    assert compound_rows[0]['args']==[[64,6,4,8],[72,11,4,8],[80,1,8,16]],compound_rows
    print('PASS typed function lowering: managed parameters, numeric/length/root moves, complete loop branches, independent classic Dis relocation oracle and whole-body refusals',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',type=Path,required=True);parser.add_argument('--awm',action='store_true');parser.add_argument('--c-only',action='store_true');args=parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory(dir=ROOT,prefix='typed-body-') as temp:
        work=Path(temp)
        flags=shlex.split(os.environ.get('NATIVE64_CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        command=shlex.split(os.environ.get('CC','cc'))+flags+['-Dfree=typedfree','-ffunction-sections','-fdata-sections']
        command+=['-I'+str(ROOT/p) for p in ('Linux/386/include','include','limbo')]
        binary=work/'unit';core=ROOT/'limbo/dis64-typed.c';fixture=ROOT/'utils/tests/dis64-typed.c'
        sources=[ROOT/'limbo/dis64-layout.c',core,fixture]
        build=command+list(map(str,sources))+['-Wl,--gc-sections','-o',str(binary)]
        subprocess.run(build,check=True)
        runner=shlex.split(os.environ.get('NATIVE64_RUNNER',''))
        subprocess.run(runner+[str(binary)],check=True,timeout=30)
        print('PASS typed body IR boundaries, reference/scalar roles, branch owners, code limits and allocation retirement',flush=True)
        production=core.read_text();test=fixture.read_text()
        for before,after,marker in (
            ('in->branch->pc>=stop','0','BRANCH_OWNER'),
            ('sourcekind!=Tany && sourcekind!=destkind','0','REF_CLASS'),
            ('reference || width==8 ||','reference ||','WIDE_IMMEDIATE'),
        ):
            assert production.count(before)==1
            altered=work/(marker+'.c');modified=production.replace(before,after)
            if marker=='WIDE_IMMEDIATE':
                assert modified.count('case IMOVL: sk=dk=Tbig; immediate=0;')==1
                modified=modified.replace('case IMOVL: sk=dk=Tbig; immediate=0;','case IMOVL: sk=dk=Tbig; immediate=1;')
            altered.write_text(modified)
            line=next(i+1 for i,x in enumerate(test.splitlines()) if marker in x)
            subprocess.run([str(altered) if x==str(core) else x for x in build],check=True)
            result=subprocess.run(runner+[str(binary)],capture_output=True,text=True,timeout=30)
            assert result.returncode and 'Assertion' in result.stderr and f'dis64-typed.c:{line}:' in result.stderr,result.stderr
            assert 'runtime error:' not in result.stderr and 'AddressSanitizer' not in result.stderr,result.stderr
            print('PASS active typed-body '+marker+' predicate control',flush=True)
        if args.c_only:return
        generate(args.limbo.resolve(),work)
        if args.awm:
            awm=runpy.run_path(str(ROOT/'utils/tests/awm-native64-preflight.py'));accepted=refused=0
            from collections import Counter
            reasons=Counter()
            for file in awm['STARTUP']:
                data,rows=compile_source(args.limbo.resolve(),work,None,ROOT/file)
                for record in rows:
                    verify(data,record);accepted+=bool(record['accepted']);refused+=not record['accepted']
                    if record['accepted']:print(f'PASS complete AWM body {file}:{record["name"]}',flush=True)
                    else:reasons[record['reason']]+=1
            print('AWM body barriers: '+json.dumps(dict(sorted(reasons.items()))),flush=True)
            print(f'PASS AWM typed bodies: {accepted} accepted, {refused} whole-body refusals; unchanged legacy code, no AWM execution')


if __name__=='__main__':main()
