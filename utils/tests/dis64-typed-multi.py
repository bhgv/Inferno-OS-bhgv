#!/usr/bin/env python3
"""Independent classic Dis oracle for complete typed bodies with multiple call sites."""
from pathlib import Path
import json
import runpy
ROOT=Path(__file__).resolve().parents[2]
LOCAL=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-local.py'));BASE=LOCAL['BASE'];KINDS=LOCAL['KINDS']
SOURCE='implement Multi;Multi:module {'+''.join(f'{p}_{name}:fn({arg}:{kind}):{kind};'for name,arg,kind,_ in KINDS for p in ('leaf','relay','chain'))+'leaf_void:fn(n:int);relay_void:fn(n:int);chain_void:fn(n:int);relay_forward:fn(n:int):int;chain_forward:fn(n:int):int;};\n'
SOURCE+='out:int;\n'
for name,arg,kind,body in KINDS:
    SOURCE+=f'leaf_{name}({arg}:{kind}):{kind}{{{body}}}\n'
    if name=='count':
        SOURCE+='relay_count(n:int):int{x:=n+1;a:=leaf_count(x);b:=leaf_count(n);return a+b;}\n'
        SOURCE+='chain_count(n:int):int{a:=relay_count(n);total:=0;while(n>0){b:=leaf_count(n);total+=b;n--;}return a+total;}\n'
    elif name in ('identity','items'):
        for prefix,target in (('relay','leaf'),('chain','relay')):
            SOURCE+=f'{prefix}_{name}({arg}:{kind}):{kind}{{old:={arg};first:={target}_{name}(old);second:=leaf_{name}(nil);return first;}}\n'
    else:
        for prefix,target in (('relay','leaf'),('chain','relay')):
            SOURCE+=f'{prefix}_{name}({arg}:{kind}):{kind}{{old:={arg};first:={target}_{name}(old);second:=leaf_{name}({arg});return first;}}\n'
SOURCE+='relay_forward(n:int):int{a:=leaf_count(n);return leaf_count(a);}chain_forward(n:int):int{a:=relay_forward(n);return leaf_count(a);}'
SOURCE+='leaf_void(n:int){out=n;}relay_void(n:int){x:=n+1;leaf_void(x);leaf_void(n);}chain_void(n:int){relay_void(n);x:=n+2;leaf_void(x);}\n'
ROOTS=(3,6,10,11,12,13)
BRANCHES=set(range(87,111))|set(range(129,135))|{13}

def verify(data,r,rows):
    if not r['accepted']:
        assert 'code'not in r and 'frame'not in r and 'calls'not in r and 'reason'in r;return
    if 'calls'not in r:LOCAL['verify'](data,r,rows);return
    assert r['schema']=='inferno-typed-multi-v1' and r['calls']
    _,old,_=BASE['READ']['legacy_instructions'](data,with_types=True);code=r['code'];original=old[r['first']:r['first']+len(code)]
    layout=json.loads(json.dumps(r));layout['schema']='inferno-typed-body-v2';layout['code']=[];BASE['verify'](data,layout)
    maps={space:{leaf[0]+i:leaf[1]+i for leaf in r[space]['leaves']for i in range(leaf[2])}for space in ('frame','data')}
    covered=set();interiors=set();previous=-1
    for first,end,target,descriptor,offset,contract,inherited in r['calls']:
        assert first>previous and first<end<len(code);previous=end
        callee=next(x for x in rows if x['first']==target);assert callee['accepted']and callee['descriptor']==descriptor
        assert contract==(callee['result'][1]|(16 if callee['result'][0]in ROOTS else 0))
        assert end-first==len(callee['args'])+1+bool(contract)
        pending=original[first]['d'][0];assert original[first]['op']==5 and original[first]['s']==[descriptor]
        assert code[first]==[5,17,descriptor,0,40,0]
        for at,arg in enumerate(callee['args'],first+1):
            o=original[at];sm=(o['mode']>>3)&7
            assert o['d']==[pending,next(x[0]for x in callee['frame']['leaves']if x[1]==arg[0])]
            assert code[at]==[o['op'],o['mode'],o['s'][0]if sm==2 else maps['frame'if sm==1 else 'data'][o['s'][0]],0,arg[0],64]
        if contract:
            binding=original[end-1];assert binding['op']==39 and binding['d']==[pending,16]and binding['m']is None
            if inherited:
                assert binding['mode']==45 and binding['s']==[16,0]and offset==32
                assert code[end-1]==[118,13,32,0,32,64]
            else:
                assert binding['mode']==13 and offset==maps['frame'][binding['s'][0]]
                assert code[end-1]==[39,13,offset,0,32,128|contract]
        assert original[end]=={'op':4,'mode':10,'m':None,'s':[pending],'d':[target]}
        assert code[end]==[4,10,40,0,target,32 if inherited else 0]
        covered.update(range(first,end+1));interiors.update(range(first+1,end+1))
    for pc,(op,mode,s,m,d,returned)in enumerate(code):
        if pc in covered:continue
        o=original[pc];assert op==o['op']and mode==o['mode']
        for label,address,value,new in (('s',(mode>>3)&7,o['s'],s),('d',mode&7,o['d'],d)):
            if value is None:assert new==0
            elif label=='d'and op in BRANCHES and address==2:assert new==value[0]-r['first']and 0<=new<len(code)and new not in interiors
            elif address==2:assert new==value[0]
            elif address==5:
                assert label=='d'and value==[16,0]and new==32 and returned==(r['result'][1]|(16 if r['result'][0]in ROOTS else 0))
            else:assert address in (0,1)and new==maps['frame'if address else 'data'][value[0]]
        mm=mode&0xc0
        assert m==(o['m']if mm==0x40 else maps['frame'if mm==0x80 else 'data'][o['m']])if mm else m==0

def generate(limbo,work):
    data,rows=BASE['compile_source'](limbo,work,SOURCE,multi=True)
    assert len(rows)==23 and all(r['accepted']for r in rows),[(r['name'],r.get('reason'))for r in rows if not r['accepted']]
    for r in rows:verify(data,r,rows)
    assert sum(len(r.get('calls',[]))for r in rows)==32
    first={r['first']:i for i,r in enumerate(rows)};names={r['name']:i for i,r in enumerate(rows)}
    lines=['#include "native64-typed-multi.h"','typedef struct MultiSite{unsigned int first,end,target,contract,inherited;}MultiSite;']
    for r in rows:
        name=r['name'];lines+=['static const Native64TypedStep tail_'+name+'[]={']+['{'+','.join(map(str,x))+'},'for x in r['code']]+['};','static const uchar tail_'+name+'_map[]={'+','.join(map(str,r['frame']['map']))+'};']
        lines+=['static const MultiSite sites_'+name+'[]={']+['{'+','.join(map(str,(a,b,first[t],c,h)))+'},'for a,b,t,d,o,c,h in r.get('calls',[])]+(['{0},']if not r.get('calls')else [])+['};']
    lines+=['typedef struct TailBody{const Native64TypedStep*code;unsigned int count,bytes;const uchar*map;const MultiSite*sites;unsigned int nsites;}TailBody;','static const TailBody tail_bodies[]={']
    for r in rows:lines+=['{tail_'+r['name']+','+str(len(r['code']))+','+str(r['frame']['bytes'])+',tail_'+r['name']+'_map,sites_'+r['name']+','+str(len(r.get('calls',[])))+'},']
    lines+=['};','static const int tail_entries[8][2]={']+['{'+str(names['relay_'+name])+','+str(names['chain_'+name])+'},'for name in [x[0]for x in KINDS]+['void','forward']]+['};']
    data_layout=rows[0]['data'];assert all(r['data']==data_layout for r in rows)
    lines+=['static const unsigned int multi_data_bytes='+str(data_layout['bytes'])+';','static const uchar multi_data_map[]={'+','.join(map(str,data_layout['map']or[0]))+'};','static const unsigned int multi_out='+str(next(x[1]for x in rows[0]['globals']if x[0]=='out'))+';']
    (work/'typed-multi-native64.h').write_text('\n'.join(lines)+'\n')
    for fault in ('target','contract','overlap','suffix'):
        altered=json.loads(json.dumps(rows));r=next(x for x in altered if 'calls'in x)
        if fault=='target':r['calls'][0][2]=999999
        elif fault=='contract':r['calls'][0][5]^=16
        elif fault=='overlap':r['calls'][1][0]=r['calls'][0][0]
        else:r['code'][-2][4]+=8
        try:verify(data,r,altered)
        except (AssertionError,KeyError,StopIteration):pass
        else:raise AssertionError(fault)
    for body,header in (
        ('f(n:int):real{v:=g(n);return real v;}g(n:int):int{return n;}','f:fn(n:int):real;g:fn(n:int):int;'),
        ('f(x:string):int{v:=g(x);return len v;}g(x:string):string{return x;}','f:fn(x:string):int;g:fn(x:string):string;'),
    ):
        extra,rr=BASE['compile_source'](limbo,work,'implement Convert;Convert:module{'+header+'};'+body,multi=True)
        assert all(x['accepted']for x in rr),rr
        for x in rr:verify(extra,x,rr)
    for n in (40,66):
        header='implement Dag;Dag:module{'+''.join(f'f{i}:fn(n:int):int;'for i in range(n))+'};'
        definitions=''.join(f'f{i}(n:int):int{{'+(f'a:=f{i+1}(n);b:=f{i+1}(n);return a+b;'if n==40 else f'return f{i+1}(n);')+'}'for i in range(n-1))+f'f{n-1}(n:int):int{{return n;}}'
        stress,rr=BASE['compile_source'](limbo,work,header+definitions,multi=True)
        if n==40:assert all(x['accepted']for x in rr)
        else:assert not next(x for x in rr if x['name']=='f0')['accepted']and next(x for x in rr if x['name']=='f2')['accepted']
        for x in rr:verify(stress,x,rr)
    cycle='implement Cycle;Cycle:module{f:fn(n:int):int;g:fn(n:int):int;};f(n:int):int{a:=g(n);return a;}g(n:int):int{a:=f(n);return a;}'
    _,rr=BASE['compile_source'](limbo,work,cycle,multi=True);assert all(not x['accepted']for x in rr)
    print('PASS multi-call boundaries: mixed result kinds, shared 40-function DAG, exact 64-function depth boundary and mutual-cycle refusal',flush=True)
    print('PASS multiple-call source oracle: 23 bodies, 32 sites, prefix computation, saved results, nil replacement, loop reentry, void calls and four corrupted-report controls',flush=True)
    return rows
