#!/usr/bin/env python3
"""Independent classic-Dis oracle for canonical source forwarding calls."""
import json
from pathlib import Path
import runpy
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-body.py'))
KINDS=[('count','n','int','total:=0;while(n>0){total+=n;n--;}return total;'),('identity','x','string','return x;'),('wide','x','big','return x;'),('fraction','x','real','return x;'),('octet','x','byte','return x;'),('items','x','list of string','return x;')]
SOURCE='implement Tail; Tail: module {\n'+''.join(f'{prefix}_{name}: fn({arg}: {kind}): {kind};\n' for name,arg,kind,_ in KINDS for prefix in ('leaf','relay','chain'))+'};\n'+''.join(f'leaf_{name}({arg}: {kind}): {kind}{{{body}}}\nrelay_{name}({arg}: {kind}): {kind}{{return leaf_{name}({arg});}}\nchain_{name}({arg}: {kind}): {kind}{{return relay_{name}({arg});}}\n' for name,arg,kind,body in KINDS)

def verify(data,r,records):
    if 'tail' not in r:
        BASE['verify'](data,r);return
    assert r['schema']=='inferno-typed-tail-v1' and r['accepted']
    _,old,types=BASE['READ']['legacy_instructions'](data,with_types=True)
    targets=[x for x in records if x['first']==r['tail'][0]];assert len(targets)==1
    callee=targets[0];assert callee['accepted'] and callee['descriptor']==r['tail'][1] and callee['result']==r['result']
    original=old[r['first']:r['first']+len(r['code'])];code=r['code'];assert len(code)==len(callee['args'])+4
    assert original[0]['op']==5 and original[0]['s']==[callee['descriptor']]
    pending=original[0]['d'][0];assert code[0]==[5,17,callee['descriptor'],0,40,0]
    # Reuse the independent full storage/map oracle after removing only this code.
    layout=json.loads(json.dumps(r));layout.pop('tail');layout['schema']='inferno-typed-body-v2';layout['code']=[]
    BASE['verify'](data,layout)
    mappings={space:{x[0]:x[1] for x in r[space]['leaves']} for space in ('frame','data')}
    for index,arg in enumerate(callee['args'],1):
        oldarg=original[index];new=code[index];assert oldarg['mode']&7==5 and oldarg['d'][0]==pending
        assert new[0]==oldarg['op'] and new[1]==oldarg['mode'] and new[3]==0 and new[4]==arg[0] and new[5]==64
        match=[x for x in callee['frame']['leaves'] if x[1]==arg[0]];assert len(match)==1 and oldarg['d'][1]==match[0][0]
        sm=(oldarg['mode']>>3)&7
        assert new[2]==(oldarg['s'][0] if sm==2 else mappings['frame' if sm==1 else 'data'][oldarg['s'][0]])
    assert original[-3]=={'op':39,'mode':45,'m':None,'s':[16,0],'d':[pending,16]}
    assert code[-3]==[118,13,32,0,32,64]
    assert original[-2]=={'op':4,'mode':10,'m':None,'s':[pending],'d':[callee['first']]}
    assert code[-2]==[4,10,40,0,callee['first'],0] and code[-1]==[12,27,0,0,0,0]

def generate(limbo,work):
    data,rows=BASE['compile_source'](limbo,work,SOURCE,tail=True)
    assert len(rows)==18 and all(r['accepted'] for r in rows),rows
    for r in rows:verify(data,r,rows)
    assert sum('tail' in r for r in rows)==12
    lines=['#include "native64-typed-code.h"']
    for r in rows:
        name=r['name'];lines+=['static const Native64TypedStep tail_'+name+'[]={']+['{'+','.join(map(str,x))+'},'for x in r['code']]+['};']
        lines+=['static const uchar tail_'+name+'_map[]={'+','.join(map(str,r['frame']['map']))+'};']
    first={r['first']:i for i,r in enumerate(rows)};names={r['name']:i for i,r in enumerate(rows)}
    lines+=['typedef struct TailBody {const Native64TypedStep *code;unsigned int count,bytes;const uchar *map;int target;} TailBody;','static const TailBody tail_bodies[]={']
    for r in rows:lines+=['{tail_'+r['name']+','+str(len(r['code']))+','+str(r['frame']['bytes'])+',tail_'+r['name']+'_map,'+str(first[r['tail'][0]] if 'tail'in r else -1)+'},']
    lines+=['};','static const int tail_entries[6][2]={']+['{'+str(names['relay_'+name])+','+str(names['chain_'+name])+'},'for name,*_ in KINDS]+['};']
    (work/'typed-tail-native64.h').write_text('\n'.join(lines)+'\n')
    for fault in ('target','pointer','argument'):
        copied=json.loads(json.dumps(rows));r=next(x for x in copied if 'tail'in x)
        if fault=='target':r['tail'][0]=999999
        elif fault=='pointer':r['code'][-3][2]=16
        else:r['code'][1][4]+=8
        try:verify(data,r,copied)
        except (AssertionError,KeyError):pass
        else:raise AssertionError(fault)
    extra="""implement Mixed; Mixed: module {
        leaf: fn(s:string,n:int,b:big,r:real,k:byte):string;
        relay: fn(s:string,n:int,b:big,r:real,k:byte):string;
    };
    leaf(s:string,n:int,nil:big,nil:real,nil:byte):string{while(n>0)n--;return s;}
    relay(s:string,n:int,b:big,r:real,k:byte):string{return leaf(s,n,b,r,k);}
    """
    mixed,mixedrows=BASE['compile_source'](limbo,work,extra,tail=True)
    assert all(r['accepted']for r in mixedrows)
    for r in mixedrows:verify(mixed,r,mixedrows)
    assert next(r for r in mixedrows if 'tail'in r)['args']==[[64,13,4,8],[72,9,4,4],[80,4,8,8],[88,7,8,8],[96,5,1,1]],mixedrows
    for body in (
        'f(n:int):int{return f(n);}',
        'f(n:int):int{return g(n);}g(n:int):int{return f(n);}',
        'f(n:int):int{return g(n)+1;}g(n:int):int{return n;}',
        'f(n:int):int{v:=g(n);return v;}g(n:int):int{return n;}',
        'f(n:int):int{if(n>0)return g(n);return g(0);}g(n:int):int{return n;}',
    ):
        header='implement Refuse; Refuse: module {f:fn(n:int):int;'+('g:fn(n:int):int;'if 'g('in body else '')+'};'
        refused,refusedrows=BASE['compile_source'](limbo,work,header+body,tail=True)
        f=next(r for r in refusedrows if r['name']=='f');assert not f['accepted'],f
        for r in refusedrows:verify(refused,r,refusedrows)
    print('PASS forwarding source boundaries: five mixed-width arguments and five recursive/nonforwarding/branched refusals',flush=True)
    print('PASS source forwarding oracle: 18 bodies, 12 call islands, exact classic target/descriptor/argument/result relocation and three corrupted-report controls',flush=True)
    return rows
