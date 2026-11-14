#!/usr/bin/env python3
"""Classic Dis oracle for one local-result call and its typed continuation."""
from pathlib import Path
import json
import runpy
ROOT=Path(__file__).resolve().parents[2]
TAIL=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-tail.py'));BASE=TAIL['BASE'];KINDS=TAIL['KINDS']
SOURCE='implement Local; Local: module {\n'+''.join(f'{prefix}_{name}:fn({arg}:{kind}):{kind};\n'for name,arg,kind,_ in KINDS for prefix in ('leaf','relay','chain'))+'};\n'+''.join(f'leaf_{name}({arg}:{kind}):{kind}{{{body}}}\nrelay_{name}({arg}:{kind}):{kind}{{v:=leaf_{name}({arg});'+('if(n<0)return v-1;return v+1;'if name=='count'else 'return v;')+f'}}\nchain_{name}({arg}:{kind}):{kind}{{v:=relay_{name}({arg});return v'+('+2'if name=='count'else '')+';}\n'for name,arg,kind,body in KINDS)

def verify(data,r,rows):
    if 'local'not in r:TAIL['verify'](data,r,rows);return
    assert r['accepted']and r['schema']=='inferno-typed-local-v1'
    end,offset,contract=r['local'];code=r['code'];callee=next(x for x in rows if x['first']==r['tail'][0])
    assert callee['descriptor']==r['tail'][1]and callee['result']==r['result']and callee['accepted']
    _,old,_=BASE['READ']['legacy_instructions'](data,with_types=True);original=old[r['first']:r['first']+len(code)]
    maps={space:{x[0]:x[1]for x in r[space]['leaves']}for space in ('frame','data')}
    assert original[0]['op']==5 and original[0]['s']==[callee['descriptor']]
    pending=original[0]['d'][0];assert code[0]==[5,17,callee['descriptor'],0,40,0]
    assert end==len(callee['args'])+2
    for i,arg in enumerate(callee['args'],1):
        o=original[i];sm=(o['mode']>>3)&7;new=code[i]
        assert o['d'][0]==pending and o['d'][1]==next(x[0]for x in callee['frame']['leaves']if x[1]==arg[0])
        assert new==[o['op'],o['mode'],o['s'][0]if sm==2 else maps['frame'if sm==1 else 'data'][o['s'][0]],0,arg[0],64]
    binding=original[end-1]
    assert binding['op']==39 and binding['mode']==13 and binding['d']==[pending,16]and binding['m']is None
    assert offset==maps['frame'][binding['s'][0]]
    assert contract==(r['result'][1]|(16 if r['result'][0]in (3,6,10,11,12,13)else 0))
    assert code[end-1]==[39,13,offset,0,32,128|contract]
    assert original[end]=={'op':4,'mode':10,'m':None,'s':[pending],'d':[callee['first']]}
    assert code[end]==[4,10,40,0,callee['first'],0]
    suffix=json.loads(json.dumps(r));suffix['first']+=end+1;suffix['schema']='inferno-typed-body-v2';suffix['code']=suffix['code'][end+1:]
    for row in suffix['code']:
        if row[0]in set(range(87,111))|set(range(129,135))|{13}:row[4]-=end+1
    BASE['verify'](data,suffix)

def generate(limbo,work):
    data,rows=BASE['compile_source'](limbo,work,SOURCE,local=True)
    assert len(rows)==18 and all(r['accepted']for r in rows),rows
    for r in rows:verify(data,r,rows)
    assert sum('local'in r for r in rows)==12
    lines=['#include "native64-typed-code.h"']
    for r in rows:
        name=r['name'];lines+=['static const Native64TypedStep tail_'+name+'[]={']+['{'+','.join(map(str,x))+'},'for x in r['code']]+['};','static const uchar tail_'+name+'_map[]={'+','.join(map(str,r['frame']['map']))+'};']
    first={r['first']:i for i,r in enumerate(rows)};names={r['name']:i for i,r in enumerate(rows)}
    lines+=['typedef struct TailBody {const Native64TypedStep *code;unsigned int count,bytes;const uchar *map;int target;unsigned int end;}TailBody;','static const TailBody tail_bodies[]={']
    for r in rows:lines+=['{tail_'+r['name']+','+str(len(r['code']))+','+str(r['frame']['bytes'])+',tail_'+r['name']+'_map,'+str(first[r['tail'][0]]if 'tail'in r else -1)+','+str(r['local'][0]if 'local'in r else 0)+'},']
    lines+=['};','static const int tail_entries[6][2]={']+['{'+str(names['relay_'+name])+','+str(names['chain_'+name])+'},'for name,*_ in KINDS]+['};']
    (work/'typed-local-native64.h').write_text('\n'.join(lines)+'\n')
    for fault in ('target','offset','contract','suffix'):
        altered=json.loads(json.dumps(rows));r=next(x for x in altered if 'local'in x)
        if fault=='target':r['tail'][0]=999999
        elif fault=='offset':r['code'][r['local'][0]-1][2]+=8
        elif fault=='contract':r['local'][2]^=16
        else:r['code'][-2][4]+=8
        try:verify(data,r,altered)
        except (AssertionError,KeyError,StopIteration):pass
        else:raise AssertionError(fault)
    branch='implement Branch; Branch:module {f:fn(n:int):int;g:fn(n:int):int;};f(n:int):int{v:=g(n);if(n<0)return v-1;return v+1;}g(n:int):int{return n;}'
    data,br=BASE['compile_source'](limbo,work,branch,local=True);assert all(r['accepted']for r in br),br
    for r in br:verify(data,r,br)
    for body in ('f(n:int):int{v:=g(n);return g(v);}g(n:int):int{return n;}','f(n:int):int{v:=f(n);return v+1;}','f(n:int):int{if(n>0)return g(n);return g(0);}g(n:int):int{return n;}'):
        header='implement Refuse;Refuse:module {f:fn(n:int):int;'+('g:fn(n:int):int;'if 'g('in body else '')+'};'
        data,ref=BASE['compile_source'](limbo,work,header+body,local=True);assert not next(r for r in ref if r['name']=='f')['accepted']
    print('PASS local-result source oracle: 18 bodies, 12 fresh bindings, typed continuations/branches, four corruption controls and three closed-boundary refusals',flush=True)
    return rows
