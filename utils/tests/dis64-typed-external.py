#!/usr/bin/env python3
"""Independent classic Dis oracle for source-derived imported call bodies."""
from pathlib import Path
import os,json,runpy,subprocess
ROOT=Path(__file__).resolve().parents[2]
MODULE=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-module.py'));BASE=MODULE['MULTI']['BASE'];KINDS=MODULE['MULTI']['KINDS']
INTERFACE=MODULE['SOURCE'][MODULE['SOURCE'].index('Multi:module'):MODULE['SOURCE'].index('};')+2]
SOURCE='implement External;include "provider.m";External:module{'+''.join(f'{prefix}_{name}:fn(p:Multi,{arg}:{kind}):{kind};'for name,arg,kind,_ in KINDS for prefix in ('fresh','forward'))+'fresh_void:fn(p:Multi,n:int);forward_void:fn(p:Multi,n:int);fresh_int:fn(p:Multi,n:int):int;forward_int:fn(p:Multi,n:int):int;loop_count:fn(p:Multi,n:int):int;loader:fn():Multi;};'
for name,arg,kind,_ in KINDS:
    SOURCE+=f'fresh_{name}(p:Multi,{arg}:{kind}):{kind}{{a:=p->relay_{name}({arg});b:=p->chain_{name}({arg});return a;}}'
    SOURCE+=f'forward_{name}(p:Multi,{arg}:{kind}):{kind}{{a:=p->relay_{name}({arg});return p->chain_{name}(a);}}'
SOURCE+='fresh_void(p:Multi,n:int){p->relay_void(n);p->chain_void(n);}forward_void(p:Multi,n:int){p->chain_void(n);p->relay_void(n);}'
SOURCE+='fresh_int(p:Multi,n:int):int{a:=p->relay_forward(n);b:=p->chain_forward(n);return a;}forward_int(p:Multi,n:int):int{a:=p->relay_forward(n);return p->chain_forward(a);}'
SOURCE+='loop_count(p:Multi,n:int):int{total:=0;while(n>0){a:=p->relay_count(n);total+=a;n--;}return total;}'
SOURCE+='loader():Multi{return load Multi "trusted/Multi";}'
BRANCHES=MODULE['MULTI']['BRANCHES'];ROOTS=MODULE['MULTI']['ROOTS']

def compile_source(limbo,work,source):
    (work/'provider.m').write_text(INTERFACE)
    file=work/'external.b';file.write_text(source);ordinary=work/'ordinary.dis';traced=work/'external.dis'
    env=dict(os.environ)
    for key in ('LIMBO_DIS64_TYPED','LIMBO_DIS64_TYPED_MULTI','LIMBO_DIS64_TYPED_TAIL','LIMBO_DIS64_TYPED_LOCAL','LIMBO_DIS64_TYPED_EXTERNAL'):env.pop(key,None)
    cmd=[str(limbo),'-i','-I',str(work),'-o']
    BASE['READ']['checked_run'](cmd+[str(ordinary),str(file)],env=env,check=True,capture_output=True)
    env.update(LIMBO_DIS64_TYPED='1',LIMBO_DIS64_TYPED_EXTERNAL='1')
    result=BASE['READ']['checked_run'](cmd+[str(traced),str(file)],env=env,check=True,capture_output=True,text=True)
    assert ordinary.read_bytes()==traced.read_bytes()
    return traced.read_bytes(),[json.loads(x[12:])for x in result.stderr.splitlines()if x.startswith('DIS64_TYPED ')]

def verify(data,r,imports):
    assert r['accepted']and r['schema']=='inferno-typed-external-v1'and r['externals']and 'calls'not in r
    _,old,types=BASE['READ']['legacy_instructions'](data,with_types=True)
    layout=json.loads(json.dumps(r));layout.update(schema='inferno-typed-body-v2',code=[]);BASE['verify'](data,layout)
    maps={space:{leaf[0]+i:leaf[1]+i for leaf in r[space]['leaves']for i in range(leaf[2])}for space in ('frame','data')}
    original=old[r['first']:r['first']+len(r['code'])];covered=set();inside=set();previous=-1
    for e in r['externals']:
        first,end=e['first'],e['end'];assert previous<first<end<len(original);previous=end
        assert imports[e['binding']]=={'name':e['name'],'signature':e['signature']}
        o=original[first];mode,offset=e['module'];assert mode in (0,1)and o['op']==11 and o['mode']==(mode<<3)|64|1
        assert offset==maps['frame'if mode else 'data'][o['s'][0]]
        pending=o['d'][0];assert r['code'][first]==[11,o['mode'],offset,e['binding'],40,0]and o['m']==e['binding']
        oldoffset=32
        for at,(new,kind,oldbytes,width)in enumerate(e['args'],first+1):
            a=original[at];sm=a['mode']>>3&7
            alignment=8 if oldbytes==8 else 4;oldoffset=(oldoffset+alignment-1)&~(alignment-1)
            assert a['d']==[pending,oldoffset];oldoffset+=oldbytes
            assert r['code'][at]==[a['op'],a['mode'],a['s'][0]if sm==2 else maps['frame'if sm==1 else 'data'][a['s'][0]],0,new,64]
        contract=e['contract'];assert end-first==len(e['args'])+1+bool(contract)
        if contract:
            b=original[end-1];assert b['op']==39 and b['d']==[pending,16]
            if e['inherited']:assert b['s']==[16,0]and r['code'][end-1]==[118,13,32,0,32,64]
            else:assert r['code'][end-1]==[39,13,maps['frame'][b['s'][0]],0,32,128|contract]
        assert original[end]=={'op':9,'mode':8|64|mode,'m':e['binding'],'s':[pending],'d':o['s']}
        assert r['code'][end]==[9,8|64|mode,40,e['binding'],offset,32 if e['inherited']else 0]
        covered.update(range(first,end+1));inside.update(range(first+1,end+1))
    for pc,(op,mode,s,m,d,returned)in enumerate(r['code']):
        if pc in covered:continue
        o=original[pc];assert op==o['op']and mode==o['mode']
        for label,address,value,new in (('s',mode>>3&7,o['s'],s),('d',mode&7,o['d'],d)):
            if value is None:assert new==0
            elif label=='d'and op in BRANCHES:assert new==value[0]-r['first']and new not in inside
            elif address==2:assert new==value[0]
            elif address==5:assert label=='d'and value==[16,0]and new==32 and returned==(r['result'][1]|(16 if r['result'][0]in ROOTS else 0))
            else:assert new==maps['frame'if address else 'data'][value[0]]
        mm=mode&192;assert m==(o['m']if mm==64 else maps['frame'if mm==128 else 'data'][o['m']])if mm else m==0

def generate(limbo,work):
    data,rows=compile_source(limbo,work,SOURCE)
    for profile in ('-64m','-64l'):
        output=work/(profile[1:]+'.dis64')
        result=subprocess.run([str(limbo),profile,'-i','-I',str(work),'-o',str(output),str(work/'external.b')],capture_output=True,text=True)
        assert result.returncode and 'Dis64 managed trial:'in result.stderr,result.stderr
        assert not output.exists()or not output.stat().st_size
        assert not any(x in result.stderr for x in ('AddressSanitizer','runtime error:','UndefinedBehaviorSanitizer'))
    print('PASS imported-call profile boundary: both managed file profiles still refuse the source client',flush=True)
    tables=BASE['READ']['legacy_instructions'](data,with_imports=True)[-1];assert len(tables)==1;imports=tables[0]
    accepted=[r for r in rows if r['accepted']];assert len(accepted)==17 and len(rows)==18,[(r['name'],r.get('reason'))for r in rows]
    assert not next(r for r in rows if r['name']=='loader')['accepted']
    for r in accepted:verify(data,r,imports)
    for fault in ('binding','module','contract','suffix'):
        r=json.loads(json.dumps(accepted[0]))
        if fault=='binding':r['externals'][0]['binding']^=1
        if fault=='module':r['externals'][0]['module'][1]+=8
        if fault=='contract':r['externals'][0]['contract']^=16
        if fault=='suffix':r['code'][-2][4]+=8
        try:verify(data,r,imports)
        except (AssertionError,KeyError,IndexError):pass
        else:raise AssertionError(fault)
    lines=['typedef struct ExternalSite{unsigned first,end,binding,mode,offset,contract,inherited;}ExternalSite;']
    for r in accepted:
        name=r['name'];lines+=['static const Native64TypedStep ext_code_'+name+'[]={']+['{'+','.join(map(str,x))+'},'for x in r['code']]+['};','static const uchar ext_map_'+name+'[]={'+','.join(map(str,r['frame']['map']))+'};','static const ExternalSite ext_sites_'+name+'[]={']+['{'+','.join(map(str,(e['first'],e['end'],e['binding'],*e['module'],e['contract'],e['inherited'])))+'},'for e in r['externals']]+['};']
    lines+=['typedef struct ExternalBody{const Native64TypedStep*code;unsigned count,bytes;const uchar*map;unsigned mapbytes;const ExternalSite*sites;unsigned nsites;}ExternalBody;','static const ExternalBody ext_bodies[]={']
    for name in [p+'_'+name for name in [x[0]for x in KINDS]+['void','int']for p in ('fresh','forward')]+['loop_count']:
        r=next(x for x in accepted if x['name']==name);lines+=['{ext_code_'+name+','+str(len(r['code']))+','+str(r['frame']['bytes'])+',ext_map_'+name+','+str(len(r['frame']['map']))+',ext_sites_'+name+','+str(len(r['externals']))+'},']
    lines+=['};','static const struct{const char*name;unsigned signature;}ext_imports[]={']+['{'+json.dumps(e['name'])+','+str(e['signature'])+'U},'for e in imports]+['};']
    (work/'typed-external-native64.h').write_text('\n'.join(lines)+'\n')
    extra='implement Extra;P:module{combine:fn(a:byte,b:int,c:big,d:string):int;};Extra:module{f:fn(p:P,a:byte,b:int,c:big,d:string):int;loader:fn():P;};f(p:P,a:byte,b:int,c:big,d:string):int{return p->combine(a,b,c,d);}loader():P{return load P "trusted/P";}'
    mixed,mixedrows=compile_source(limbo,work,extra);mixedimports=BASE['READ']['legacy_instructions'](mixed,with_imports=True)[-1][0]
    mixedfn=next(r for r in mixedrows if r['name']=='f');verify(mixed,mixedfn,mixedimports)
    assert [a[0]for a in mixedfn['externals'][0]['args']]==[64,68,72,80]
    print('PASS imported-call source geometry: mixed byte/int/big/string arguments and loop reentry',flush=True)
    print('PASS external source oracle: 17 whole bodies, 33 imported sites, .m import hashes, fresh/inherited results, void calls, refused loader and four corrupt-report controls',flush=True)
    return data,rows,imports
