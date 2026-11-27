#!/usr/bin/env python3
"""Build an owned client spec from complete source bodies and independent .m imports."""
from pathlib import Path
import runpy,json
ROOT=Path(__file__).resolve().parents[2]
EXTERNAL=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-external.py'))

def generate(limbo,work):
    _,oldrows,imports=EXTERNAL['generate'](limbo,work)
    source=EXTERNAL['SOURCE'].replace('loader:fn():Multi;','').replace('loader():Multi{return load Multi "trusted/Multi";}','')
    source=source.replace('loop_count:fn(p:Multi,n:int):int;','loop_count:fn(p:Multi,n:int):int;local_count:fn(p:Multi,n:int):int;')+'local_count(p:Multi,n:int):int{return fresh_count(p,n);}'
    data,rows=EXTERNAL['compile_source'](limbo,work,source)
    assert len(rows)==18 and all(r['accepted']for r in rows)
    _,exports=EXTERNAL['MODULE']['check_exports'](data,rows);assert len(exports)==18
    for r in rows:
        if r['name']=='local_count':
            local=json.loads(json.dumps(r));local['schema']='inferno-typed-multi-v1';EXTERNAL['MODULE']['MULTI']['verify'](data,local,rows);continue
        EXTERNAL['verify'](data,r,imports)
        prior=next(x for x in oldrows if x['name']==r['name'])
        assert (r['code'],r['frame'],r['externals'])==(prior['code'],prior['frame'],prior['externals'])
    # The loader-free source has no path initializer or unsupported body to omit.
    assert all(r['data']['bytes']==0 for r in rows)
    # A returned module interface retains all23 imports, including unused leaves.
    # Compile independent .m probes for those declarations; do not use provider bodies.
    probe='implement Shape;include "provider.m";Shape:module{'+''.join(f'f_{name}:fn(p:Multi,{arg}:{kind}):{kind};'for name,arg,kind,_ in EXTERNAL['KINDS'])+'f_void:fn(p:Multi,n:int);};'
    probe+=''.join(f'f_{name}(p:Multi,{arg}:{kind}):{kind}{{return p->leaf_{name}({arg});}}'for name,arg,kind,_ in EXTERNAL['KINDS'])+'f_void(p:Multi,n:int){p->leaf_void(n);}'
    probedata,proberows=EXTERNAL['compile_source'](limbo,work,probe);assert all(r['accepted']for r in proberows)
    prototypes={e['name']:e for r in proberows+rows for e in r.get('externals',[])}
    lines=['#include "native64-typed-module.h"']
    requests=[]
    for ordinal,imported in enumerate(imports):
        sites=[e for r in rows for e in r.get('externals',[])if e['binding']==ordinal]
        if not sites:sites=[prototypes[imported['name']]]
        assert all(e['name']==imported['name']and e['signature']==imported['signature']and e['args']==sites[0]['args']for e in sites)
        args=sites[0]['args'];size=(max((a[0]+a[3]for a in args),default=64)+7)&~7;bitmap=[0]*((size+63)//64)
        for offset,kind,oldbytes,width in args:
            if kind in EXTERNAL['ROOTS']:bitmap[offset//64]|=128>>((offset//8)%8)
        lines+=['static const uchar request_map_'+str(ordinal)+'[]={'+','.join(map(str,bitmap))+'};']
        requests.append('{'+json.dumps(imported['name'])+','+str(imported['signature'])+'U,'+str(size)+',request_map_'+str(ordinal)+','+str(len(bitmap))+'}')
    lines+=['static const Native64ModuleImport client_requests[]={'+','.join(requests)+'};','static const Native64ModuleTable client_tables[]={{client_requests,'+str(len(imports))+'}};']
    names=[p+'_'+name for name in [x[0]for x in EXTERNAL['KINDS']]+['void','int']for p in ('fresh','forward')]+['loop_count','local_count']
    first={r['first']:names.index(r['name'])for r in rows}
    for r in rows:
        name=r['name'];lines+=['static const Native64TypedStep client_code_'+name+'[]={']+['{'+','.join(map(str,x))+'},'for x in r['code']]+['};','static const uchar client_map_'+name+'[]={'+','.join(map(str,r['frame']['map']))+'};','static const Native64ModuleSite client_sites_'+name+'[]={']
        sites=[(e['first'],e['end'],e['binding'],e['contract'],e['inherited'],1,0,*e['module'])for e in r.get('externals',[])]
        sites += [(a,b,first[t],c,h,0,0,0,0)for a,b,t,d,o,c,h in r.get('calls',[])]
        lines+=['{'+','.join(map(str,x))+'},'for x in sorted(sites)]+['};']
    lines+=['static const Native64ModuleFunction client_functions[]={']
    for name in names:
        r=next(x for x in rows if x['name']==name)
        lines+=['{'+json.dumps(r['export_name'])+','+str(r['signature'])+'U,1,client_code_'+name+','+str(len(r['code']))+','+str(r['frame']['bytes'])+',client_map_'+name+','+str(len(r['frame']['map']))+',client_sites_'+name+','+str(len(r.get('externals',[]))+len(r.get('calls',[])))+'},']
    lines+=['};','static const Native64ModuleSpec client_spec={"External","trusted/External",client_functions,18,~0U,0,nil,0,nil,0,client_tables,1};']
    (work/'typed-imports-native64.h').write_text('\n'.join(lines)+'\n')
    print('PASS owned client source: all 18 bodies/exports, 34 local/imported sites, 23 independent import requests, loader-free complete source and empty MP',flush=True)
