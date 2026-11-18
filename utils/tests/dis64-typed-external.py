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
OPCODES=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['OP'].CODES
RAISE=OPCODES['IRAISE']

def compile_source(limbo,work,source,interface=None,loads=False):
    (work/'provider.m').write_text(INTERFACE if interface is None else interface)
    file=work/'external.b';file.write_text(source);ordinary=work/'ordinary.dis';traced=work/'external.dis'
    env=dict(os.environ)
    for key in ('LIMBO_DIS64_TYPED','LIMBO_DIS64_TYPED_MULTI','LIMBO_DIS64_TYPED_TAIL','LIMBO_DIS64_TYPED_LOCAL','LIMBO_DIS64_TYPED_EXTERNAL','LIMBO_DIS64_TYPED_LOAD'):env.pop(key,None)
    cmd=[str(limbo),'-i','-I',str(work),'-o']
    BASE['READ']['checked_run'](cmd+[str(ordinary),str(file)],env=env,check=True,capture_output=True)
    env.update(LIMBO_DIS64_TYPED='1',LIMBO_DIS64_TYPED_EXTERNAL='1')
    if loads:env['LIMBO_DIS64_TYPED_LOAD']='1'
    result=BASE['READ']['checked_run'](cmd+[str(traced),str(file)],env=env,check=True,capture_output=True,text=True)
    assert ordinary.read_bytes()==traced.read_bytes()
    return traced.read_bytes(),[json.loads(x[12:])for x in result.stderr.splitlines()if x.startswith('DIS64_TYPED ')]

def verify(data,r,imports,loads=False,rows=None):
    assert r['accepted']and r['schema']in (('inferno-typed-external-v1','inferno-typed-load-v1','inferno-typed-body-v2')if loads else ('inferno-typed-external-v1',))and (loads or r.get('externals'))and (rows is not None or 'calls'not in r)
    _,old,types=BASE['READ']['legacy_instructions'](data,with_types=True)
    layout=json.loads(json.dumps(r));layout.update(schema='inferno-typed-body-v2');BASE['verify'](data,layout,validate_code=False)
    maps={space:{leaf[0]+i:leaf[1]+i for leaf in r[space]['leaves']for i in range(leaf[2])}for space in ('frame','data')}
    original=old[r['first']:r['first']+len(r['code'])];covered=set();inside=set();previous=-1
    for first,end,target,descriptor,offset,contract,inherited in r.get('calls',[]):
        assert rows is not None and previous<first<end<len(original);previous=end
        callee=next(x for x in rows if x['first']==target);assert callee['accepted']and callee['descriptor']==descriptor
        assert contract==(48 if callee.get('result_pair')==[11,10]else callee['result'][1]|(16 if callee['result'][0]in ROOTS else 0))
        at=first+1;preps=set()
        pending=original[first]['d'][0];assert original[first]['op']==5 and original[first]['s']==[descriptor]
        assert r['code'][first]==[5,17,descriptor,0,40,0]
        for arg in callee['args']:
            prep=False;o=original[at]
            if loads and arg[1]==13 and o['op']==41 and o['mode']&7==1:
                assert at+3<end and original[at+1]['op']==OPCODES['ISLICEA']and original[at+2]['op']==OPCODES['ICVTAC']
                view=o['d'];assert original[at+1]['d']==view and original[at+2]['s']==view
                clear=original[at+3];assert clear['op']==41 and clear['mode']==1 and clear['d']==view
                for addr in(o['s'],view):assert next(l for l in r['frame']['leaves']if l[0]==addr[0])[4]==3
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                preps.update((at,at+1,at+3));at+=2;o=original[at];prep=True
            sm=o['mode']>>3&7
            assert o['d']==[pending,next(x[0]for x in callee['frame']['leaves']if x[1]==arg[0])]
            if o['op']==OPCODES['ICVTAC']:
                assert loads and arg[1:]==[13,4,8]and sm in(0,1)and o['m']is None
                assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0])[4]==3
                assert r['code'][at]==[o['op'],o['mode'],maps['frame'if sm else'data'][o['s'][0]],0,arg[0],64]
            elif o['op']==OPCODES['IADDC']:
                mm=1 if o['mode']&192==128 else 0 if o['mode']&192==192 else -1
                assert loads and target==r['first']and inherited and arg[1:]==[13,4,8]and sm in(0,1)and mm in(0,1)
                assert r['code'][end+1][0]==OPCODES['IRET']
                assert r['code'][at]==[o['op'],o['mode'],maps['frame'if sm else'data'][o['s'][0]],maps['frame'if mm else'data'][o['m']],arg[0],64]
            else:
                assert r['code'][at]==[o['op'],o['mode'],o['s'][0]if sm==2 else maps['frame'if sm==1 else'data'][o['s'][0]],0,arg[0],64]
            at+=2 if prep else 1
        assert at+bool(contract)==end
        if contract:
            binding=original[end-1];assert binding['op']==39 and binding['d']==[pending,16]and binding['m']is None
            if inherited:
                assert binding['mode']==45 and binding['s']==[16,0]and offset==32
                assert r['code'][end-1]==[118,13,32,0,32,64]
            else:
                assert binding['mode']==13 and offset==maps['frame'][binding['s'][0]]
                assert r['code'][end-1]==[39,13,offset,0,32,128|contract]
        assert original[end]=={'op':4,'mode':10,'m':None,'s':[pending],'d':[target]}
        assert r['code'][end]==[4,10,40,0,target,32 if inherited else 0]
        covered.update(set(range(first,end+1))-preps);inside.update(range(first+1,end+1))
    previous=-1
    for v in r.get('variadics',[]):
        first,end=v['first'],v['end'];assert loads and previous<first<end<len(original) and not covered.intersection(range(first,end+1));previous=end
        op=v['operation'];assert op in(0,3) and v['contract']==(24 if op==0 else 4)
        choices=imports if imports and isinstance(imports[0],list)else[imports]
        assert any(v['binding']<len(t)and t[v['binding']]=={'name':v['name'],'signature':v['signature']}for t in choices)
        assert v['name']==('sprint'if op==0 else'fprint') and v['signature']==(0x4c0624b6 if op==0 else 0xf46486c8)
        o=original[first];assert o['op']==5 and o['mode']==17 and o['m']is None
        size,roots=types[o['s'][0]];count=v['count']+(1 if op==0 else 2);frame=v['frame']
        assert frame['legacy_bytes']==size and frame['bytes']==64+8*count
        leaves=frame['leaves'];assert len(leaves)==count
        assert leaves[:1 if op==0 else 2]==[[32+4*j,64+8*j,4,8,12 if op==3 and j==0 else 13]for j in range(1 if op==0 else 2)]
        rootwords={l[0]//4 for l in leaves if l[4]in(12,13)}
        assert {i*8+b for i,x in enumerate(roots)for b in range(8)if x&(128>>b)}==rootwords
        expected=[0]*((frame['bytes']+63)//64)
        for l in leaves:
            if l[4]in(12,13):expected[l[1]//64]|=128>>((l[1]//8)%8)
        assert frame['map']==expected and r['code'][first]==[5,17,o['s'][0],0,40,0]
        pending=o['d'][0];at=first+1;fd=None;oldend=32
        for j,(legacy,native,oldwidth,width,kind)in enumerate(leaves):
            assert kind in(4,7,9,12,13,16)and native==64+8*j and width==8 and oldwidth==(8 if kind in(4,7)else 4)and legacy>=oldend and legacy%4==0 and legacy+oldwidth<=size
            oldend=legacy+oldwidth
            oldop=41 if kind in(12,13)else 118 if kind==4 else 46 if kind==7 else 45
            newop=oldop if kind in(4,7,12,13)else OPCODES['ICVTWL']
            a=original[at];sm=a['mode']>>3&7;assert a['op']==oldop and sm in((0,1)if kind in(4,7,12,13)else(0,1,2))and a['m']is None and a['d']==[pending,legacy]
            source=a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]]
            assert r['code'][at]==[newop,a['mode'],source,0,native,64]
            if op==3 and j==0 and sm==1:fd=a['s'][0]
            at+=1
            if op==3 and j==0 and original[at]['mode']&7==1:
                clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[fd]
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][fd],0]
                assert not any(a['mode']>>3&7==1 and a['s']==[fd]for a in original[at+1:end-1]);at+=1
        assert at==end-1;binding=original[at];assert binding['op']==39 and binding['mode']in(13,45)and binding['m']is None and binding['d']==[pending,16]
        if v.get('inherited'):
            assert v['inherited']==1 and binding['mode']==45 and binding['s']==[16,0]and v['resultoffset']==32
            assert r['code'][at]==[118,13,32,0,32,128|v['contract']]
        else:
            assert binding['mode']==13 and v['resultoffset']==maps['frame'][binding['s'][0]]and r['code'][at]==[39,13,v['resultoffset'],0,32,128|v['contract']]
        call=original[end];mode,offset=v['module'];assert mode in(0,1)and call['op']==9 and call['mode']==8|64|mode and call['m']==v['binding']and call['s']==[pending]
        assert offset==maps['frame'if mode else'data'][call['d'][0]]and r['code'][end]==[9,8|64|mode,40,v['binding'],offset,32 if v.get('inherited')else 0]
        covered.update(range(first,end+1));inside.update(range(first+1,end+1))
    previous=-1
    for e in r.get('externals',[]):
        first,end=e['first'],e['end'];assert previous<first<end<len(original)and not covered.intersection(range(first,end+1));previous=end
        choices=imports if imports and isinstance(imports[0],list)else[imports]
        assert any(e['binding']<len(t)and t[e['binding']]=={'name':e['name'],'signature':e['signature']}for t in choices)
        o=original[first];mode,offset=e['module'];assert mode in (0,1)and o['op']==11 and o['mode']==(mode<<3)|64|1
        assert offset==maps['frame'if mode else 'data'][o['s'][0]]
        pending=o['d'][0];assert r['code'][first]==[11,o['mode'],offset,e['binding'],40,0]and o['m']==e['binding']
        oldoffset=32
        for at,(new,kind,oldbytes,width)in enumerate(e['args'],first+1):
            a=original[at];sm=a['mode']>>3&7
            alignment=8 if oldbytes==8 else 4;oldoffset=(oldoffset+alignment-1)&~(alignment-1)
            assert a['d']==[pending,oldoffset];oldoffset+=oldbytes
            if a['op']==OPCODES['ILENA']:
                assert loads and kind==9 and oldbytes==width==4 and sm in(0,1)and a['m']is None
                assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]==3
                assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm else'data'][a['s'][0]],0,new,64]
            elif a['op']==OPCODES['IADDC']:
                mm=1 if a['mode']&192==128 else 0 if a['mode']&192==192 else -1
                assert loads and kind==13 and oldbytes==4 and width==8 and sm in(0,1)and mm in(0,1)
                for space,old in (('frame'if sm else'data',a['s'][0]),('frame'if mm else'data',a['m'])):
                    assert any(leaf[0]==old and leaf[2:]==[4,8,13]for leaf in r[space]['leaves'])
                assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm else'data'][a['s'][0]],maps['frame'if mm else'data'][a['m']],new,64]
            else:
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
        o=original[pc]
        if loads and op==OPCODES['INEWAZ']and o['op']==OPCODES['INEWA']:
            assert m==0 and types[o['m']][0]==1 and not any(types[o['m']][1])
            o={**o,'op':op,'m':0}
        if op==OPCODES['IADDC']and not o['mode']&192:
            assert loads and o['mode']&7==1
            o={**o,'mode':o['mode']|128,'m':o['d'][0]}
        assert op==o['op']and mode==o['mode']
        if op==8:
            assert loads and mode&192==64 and 0<=m<64 and returned==0
            for address,offset,kind in ((mode>>3&7,s,13),(mode&7,d,11)):
                assert address in (0,1)
                leaves=r['frame'if address else'data']['leaves']
                assert any(leaf[1]==offset and leaf[3]==8 and leaf[4]==kind for leaf in leaves)
        if op==RAISE:
            assert loads and mode in(3,11)and m==d==returned==0
            address=mode>>3&7;leaves=r['frame'if address else'data']['leaves']
            assert any(leaf[1]==s and leaf[3:]==[8,13]for leaf in leaves)
        if r.get('result_pair'):
            assert r['result_pair']==[11,10]and r['result']==[14,16]
            if o['op']==41 and o['mode']&7==5:
                assert o['d']in([16,0],[16,4])and d==32+2*o['d'][1]and returned==(40 if o['d'][1]==0 else 56)
                address=mode>>3&7;assert address in(0,1)
                assert s==maps['frame'if address else'data'][o['s'][0]]
                continue
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
