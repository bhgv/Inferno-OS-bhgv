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

def field_argument(original,step,new,maps):
    sm=original['mode']>>3&7;oldfields={0:0,4:8,8:16,12:24,16:32,20:24,40:48,44:48,48:56,52:64,56:72,84:104}
    assert original['op']==41 and sm in(4,5)and original['mode']==(sm<<3)|5 and original['m']is None and len(original['s'])==2
    base,field=original['s'];assert field in oldfields
    assert step==[41,original['mode'],maps['frame'if sm==5 else'data'][base],oldfields[field],new,125]

def verify(data,r,imports,loads=False,rows=None):
    assert r['accepted']and r['schema']in (('inferno-typed-external-v1','inferno-typed-load-v1','inferno-typed-body-v2')if loads else ('inferno-typed-external-v1',))and (loads or r.get('externals'))and (rows is not None or 'calls'not in r)
    if r.get('result')==[14,24]:assert r.get('result_channels')==[6,6,6]
    _,old,types=BASE['READ']['legacy_instructions'](data,with_types=True)
    layout=json.loads(json.dumps(r));layout.update(schema='inferno-typed-body-v2');BASE['verify'](data,layout,validate_code=False)
    maps={space:{leaf[0]+i:leaf[1]+i for leaf in r[space]['leaves']for i in range(leaf[2])}for space in ('frame','data')}
    original=old[r['first']:r['first']+len(r['code'])];covered=set();inside=set();previous=-1
    for first,end,target,descriptor,offset,contract,inherited in r.get('calls',[]):
        assert rows is not None and previous<first<end<len(original);previous=end
        callee=next(x for x in rows if x['first']==target);assert callee['accepted']and callee['descriptor']==descriptor
        assert contract==(72 if callee.get('result_channels')==[6,6,6]else 48 if callee.get('result_pair')==[11,10]else callee['result'][1]|(16 if callee['result'][0]in ROOTS else 0))
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
            if r['code'][at][5]==125:
                assert loads and arg[1]in ROOTS;field_argument(o,r['code'][at],arg[0],maps)
            elif o['op']==OPCODES['ICVTAC']:
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
            if at<end and r['code'][at][5]==182:
                clear=original[at];assert arg[1]in ROOTS and o['op']==41 and sm==1 and clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==o['s']
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][clear['d'][0]],182];at+=1
        assert at+bool(contract)==end
        if contract:
            binding=original[end-1];assert binding['op']==39 and binding['d']==[pending,16]and binding['m']is None
            if inherited:
                assert binding['mode']==45 and binding['s']==[16,0]and offset==32
                assert r['code'][end-1]==[118,13,32,0,32,64]
            else:
                assert binding['mode']in(5,13)and (binding['mode']!=5 or contract==24)and offset==maps['frame'if binding['mode']==13 else'data'][binding['s'][0]]
                assert r['code'][end-1]==[39,binding['mode'],offset,0,32,128|contract]
        spawn=original[end]['op']==OPCODES['ISPAWN']
        if spawn:assert loads and not contract and not inherited and callee['result']==[0,0]
        opcode=OPCODES['ISPAWN']if spawn else 4
        assert original[end]=={'op':opcode,'mode':10,'m':None,'s':[pending],'d':[target]}
        assert r['code'][end]==[opcode,10,40,0,target,186 if spawn else 32 if inherited else 0]
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
        at=first+1;aggregates={a['index']:a['layout']for a in e.get('aggregate_args',[])}
        assert len(aggregates)==len(e.get('aggregate_args',[]))
        for j,(new,kind,oldbytes,width)in enumerate(e['args']):
            listclear=None
            if at<end and r['code'][at][5]==112:
                assert loads and kind==10 and oldbytes==4 and width==8
                temp=None;count=0
                while at<end and r['code'][at][5]==112:
                    a=original[at];sm=a['mode']>>3&7
                    assert a['m']is None and a['mode']&7==1 and sm in(0,1)
                    target=maps['frame'][a['d'][0]]
                    assert next(l for l in r['frame']['leaves']if l[0]==a['d'][0])[2:4]==[4,8]
                    if temp is None:
                        assert a['op']==41 and sm==0 and next(l for l in r['data']['leaves']if l[0]==a['s'][0])[4]==20
                        temp=target;listclear=(temp,maps['data'][a['s'][0]])
                    else:
                        assert a['op']==OPCODES['ICONSP']and target==temp
                        assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]in(13,20)
                        count+=1
                    assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm else'data'][a['s'][0]],0,target,112];at+=1
                assert 0<count<=256 and original[at]['s']==[next(old for old,v in maps['frame'].items()if v==temp)]and original[at]['mode']==13
            stringprep=None
            if at<end and r['code'][at][5]==180:
                assert loads and kind==13;oldtemp=original[at]['d'][0];temp=maps['frame'][oldtemp];nprep=0
                while at<end and r['code'][at][5]==180:
                    a=original[at];sm=a['mode']>>3&7;assert a['d']==[oldtemp]and a['mode']&7==1
                    if not nprep:
                        assert a['op']==OPCODES['ICVTWC']and a['m']is None and sm in(0,1,2)
                        source=a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]]
                        if sm!=2:assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[2:]==[4,4,9]
                        assert r['code'][at]==[a['op'],a['mode'],source,0,temp,180]
                    else:
                        assert a['op']==OPCODES['IADDC']and sm in(0,1)
                        mm=1 if a['mode']&192 in(0,128)else 0;middle=oldtemp if a['m']is None else a['m']
                        assert (sm==1 and a['s']==[oldtemp])or(mm==1 and middle==oldtemp)
                        assert r['code'][at]==[a['op'],(sm<<3)|(128 if mm else 192)|1,maps['frame'if sm else'data'][a['s'][0]],maps['frame'if mm else'data'][middle],temp,180]
                    nprep+=1;at+=1
                assert 1<=nprep<=9;stringprep=(temp,oldtemp)
            a=original[at];sm=a['mode']>>3&7
            alignment=8 if oldbytes==8 and kind!=1 else 4;oldoffset=(oldoffset+alignment-1)&~(alignment-1)
            if kind==1:
                assert loads and j in aggregates
                layout=aggregates.pop(j);n=2 if oldbytes==8 else 4 if oldbytes==16 else 0
                assert n and width==oldbytes and layout=={'legacy_bytes':oldbytes,'bytes':width,'map':[0],'leaves':[[4*k,4*k,4,4,9]for k in range(n)]}
                if r['code'][at][5]==166:
                    assert a['op']==41 and sm in(4,5)and a['mode']==(sm<<3)|1 and a['m']is None and a['s'][1]==8
                    temp=maps['frame'][a['d'][0]]
                    assert r['code'][at]==[41,a['mode'],maps['frame'if sm==5 else'data'][a['s'][0]],16,temp,166]
                    at+=1;a=original[at];sm=a['mode']>>3&7;assert sm==5 and maps['frame'][a['s'][0]]==temp
                if a['op']==OPCODES['IMOVM']and sm in(4,5):
                    assert oldbytes in(8,16)and a['mode']==(sm<<3)|64|5 and a['m']==oldbytes and a['d']==[pending,oldoffset]and a['s'][1]in((0,16)if oldbytes==16 else(0,8,16,24))
                    assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm==5 else'data'][a['s'][0]],oldbytes,new,(160+a['s'][1]//16)if oldbytes==16 else(162+a['s'][1]//8)];at+=1
                elif a['op']==OPCODES['IMOVM']:
                    assert sm in(0,1)and a['mode']==(sm<<3)|64|5 and a['m']==oldbytes and a['d']==[pending,oldoffset]
                    source=maps['frame'if sm else'data'][a['s'][0]]
                    for k in range(n):
                        assert maps['frame'if sm else'data'][a['s'][0]+4*k]==source+4*k
                        assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0]+4*k)[2:]==[4,4,9]
                    assert r['code'][at]==[a['op'],a['mode'],source,width,new,64];at+=1
                else:
                    for k in range(n):
                        a=original[at];sm=a['mode']>>3&7
                        assert a['op']==OPCODES['IMOVW']and a['mode']==(sm<<3)|5 and sm in(0,1,2)and a['m']is None and a['d']==[pending,oldoffset+4*k]
                        source=a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]]
                        assert r['code'][at]==[a['op'],a['mode'],source,0,new+4*k,64];at+=1
                if at<end and r['code'][at][5]==126:
                    clear=original[at];assert sm==5 and a['op']==OPCODES['IMOVM']and clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[a['s'][0]]
                    assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                    assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][clear['d'][0]],126];at+=1
                oldoffset+=oldbytes;continue
            assert j not in aggregates
            assert a['d']==[pending,oldoffset];oldoffset+=oldbytes
            if r['code'][at][5]==125:
                assert loads and kind in ROOTS;field_argument(a,r['code'][at],new,maps)
            elif a['op']==OPCODES['ILENA']:
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
            at+=1
            if stringprep is not None:
                temp,oldtemp=stringprep;assert a['op']==OPCODES['IADDC']and ((sm==1 and a['s']==[oldtemp])or(a['mode']&192==128 and a['m']==oldtemp))
                clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[oldtemp]
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,temp,181];at+=1
            if listclear is not None:
                temp,nilroot=listclear;clear=original[at]
                assert clear['op']==41 and clear['mode']==1 and clear['m']is None
                assert maps['data'][clear['s'][0]]==nilroot and maps['frame'][clear['d'][0]]==temp
                assert r['code'][at]==[41,1,nilroot,0,temp,112];at+=1
            if at<end and r['code'][at][5]==126:
                assert loads and kind in ROOTS and a['op']==41 and sm==1
                clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==a['s']
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][clear['d'][0]],126];at+=1
        assert not aggregates
        contract=e['contract'];assert at+bool(contract)==end
        assert (contract==72)==(e.get('result_channels')==[6,6,6])
        if contract:
            b=original[end-1];assert b['op']==39 and b['d']==[pending,16]
            if e['inherited']:assert b['s']==[16,0]and r['code'][end-1]==[118,13,32,0,32,64]
            else:
                assert b['mode']in(5,13)and (b['mode']!=5 or contract==24)
                assert r['code'][end-1]==[39,b['mode'],maps['frame'if b['mode']==13 else'data'][b['s'][0]],0,32,128|contract]
        assert original[end]=={'op':9,'mode':8|64|mode,'m':e['binding'],'s':[pending],'d':o['s']}
        assert r['code'][end]==[9,8|64|mode,40,e['binding'],offset,32 if e['inherited']else 0]
        covered.update(range(first,end+1));inside.update(range(first+1,end+1))
    for pc,(op,mode,s,m,d,returned)in enumerate(r['code']):
        if pc in covered:continue
        o=original[pc]
        if o['op']==OPCODES['INEWCMP']:
            assert loads and op==o['op']==OPCODES['INEWCMP']and mode==o['mode']and returned in(183,185)
            size,mapping=types[o['s'][0]];n=size//4;assert n in(2,3)and size==4*n and mapping==bytes([192 if n==2 else 224])
            mm=mode&192;dm=mode&7;middle=0 if mm==0 else o['m']if mm==64 else maps['frame'if mm==128 else'data'][o['m']]
            if returned==185:assert o['d']==[16,0]and r['result']==[6,8]
            else:assert next(l for l in r['frame'if dm==1 else'data']['leaves']if l[0]==o['d'][0])[4]==6
            assert r['code'][pc]==[op,mode,n,middle,32 if returned==185 else maps['frame'if dm==1 else'data'][o['d'][0]],returned]
            continue
        if op in [OPCODES[n]for n in('INEWCB','INEWCW','INEWCL','INEWCF','INEWCP')]:
            assert loads and op==o['op']and mode==o['mode']and o['s']is None and returned in(0,24)
            mm=mode&192;dm=mode&7;middle=0 if mm==0 else o['m']if mm==64 else maps['frame'if mm==128 else'data'][o['m']]
            if returned:assert o['d']==[16,0]and r['result']==[6,8]
            else:assert next(l for l in r['frame'if dm==1 else'data']['leaves']if l[0]==o['d'][0])[4]==6
            assert r['code'][pc]==[op,mode,0,middle,32 if returned else maps['frame'if dm==1 else'data'][o['d'][0]],returned]
            continue
        if returned==173:
            sm=o['mode']>>3&7;dm=o['mode']&7
            assert loads and o['op']==41 and sm in(4,5)and dm in(4,5)and o['m']is None and o['s'][1]==o['d'][1]and o['s'][1]in(0,4,8)
            assert r['code'][pc]==[41,o['mode'],maps['frame'if sm==5 else'data'][o['s'][0]],2*o['s'][1],maps['frame'if dm==5 else'data'][o['d'][0]],173]
            continue
        if returned in(172,174):
            assert loads and op==o['op']and op in(OPCODES['INEW'],OPCODES['INEWZ'])and mode==o['mode']==(17 if returned==172 else 21)and o['m']is None
            assert types[o['s'][0]]==(12,bytes([224]))
            if returned==174:assert o['d']==[16,0]and r['result']==[12,8]
            assert r['code'][pc]==[op,mode,0,0,maps['frame'][o['d'][0]]if returned==172 else 32,returned]
            continue
        if 113<=returned<125:
            assert loads
            oldfields=[44,48,52,4,8,12,0,4,8,12,16];newfields=[48,56,64,8,16,24,0,8,16,24,32];mapping=dict(zip(oldfields,newfields))
            if returned==113:
                assert op==o['op']==41 and mode==o['mode']and o['m']is None
                sm=mode>>3&7;dm=mode&7;read=sm in(4,5)
                assert (read and dm in(0,1))or(not read and sm in(0,1)and dm in(4,5))
                field=o['s']if read else o['d'];direct=o['d']if read else o['s'];assert len(field)==2 and field[1]in mapping
                bm=(sm if read else dm)&1;vm=dm if read else sm
                assert m==mapping[field[1]]and (s if read else d)==maps['frame'if bm else'data'][field[0]]and (d if read else s)==maps['frame'if vm else'data'][direct[0]]
                assert next(l for l in r['frame'if bm else'data']['leaves']if l[0]==field[0])[2:4]==[4,8]
                leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==direct[0]);assert leaf[2:4]==[4,8]and (leaf[4]==(13 if field[1]==52 else 12)or(not read and vm==0 and leaf[4]==20))
            else:
                index=returned-114;assert index!=2
                sm=o['mode']>>3&7;field=o['s'];assert sm in(4,5)and field[1]==oldfields[index]and o['mode']==(sm<<3)|192|2 and mode==o['mode']
                assert o['op']in(93,94)and op==(OPCODES['IBEQL']if o['op']==93 else OPCODES['IBNEL'])
                assert s==maps['frame'if sm==5 else'data'][field[0]]and m==maps['data'][o['m']]and d==o['d'][0]-r['first']and d not in inside
            continue
        if 100<=returned<107:
            assert loads
            oldfields=[0,20,40,52,56,84];newfields=[0,24,48,64,72,104]
            if returned==100:
                assert op==o['op']==41 and mode==o['mode']and o['m']is None
                sm=mode>>3&7;dm=mode&7;read=sm in(4,5)
                assert (read and dm in(0,1))or(not read and sm in(0,1)and dm in(4,5))
                field=o['s']if read else o['d'];direct=o['d']if read else o['s'];assert len(field)==2
                index=oldfields.index(field[1]);bm=(sm if read else dm)&1;vm=dm if read else sm
                assert m==newfields[index]and (s if read else d)==maps['frame'if bm else'data'][field[0]]and (d if read else s)==maps['frame'if vm else'data'][direct[0]]
                assert next(l for l in r['frame'if bm else'data']['leaves']if l[0]==field[0])[2:]==[4,8,12]
                leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==direct[0]);assert leaf[2:4]==[4,8]and (leaf[4]==(6 if index==3 else 12)or(not read and vm==0 and leaf[4]==20))
            else:
                sm=o['mode']>>3&7;field=o['s'];assert sm in(4,5)and len(field)==2
                index=oldfields.index(field[1]);assert returned==101+index and o['mode']==(sm<<3)|192|2 and mode==o['mode']
                assert o['op']in(93,94)and op==(OPCODES['IBEQL']if o['op']==93 else OPCODES['IBNEL'])
                assert s==maps['frame'if sm==5 else'data'][field[0]]and m==maps['data'][o['m']]and d==o['d'][0]-r['first']and d not in inside
                assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==field[0])[2:]==[4,8,12]
                assert next(l for l in r['data']['leaves']if l[0]==o['m'])[4]==20
            continue
        if 97<=returned<100:
            assert loads and o['op']in(93,94)and op==(OPCODES['IBEQL']if o['op']==93 else OPCODES['IBNEL'])and mode&192==192 and mode&7==2
            sm=o['mode']>>3&7;left=o['s'];right=[o['m']]
            assert o['mode']==(sm<<3)|192|2 and sm in(4,5)and len(left)==2 and left[1]in(0,4,8)
            assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==left[0])[2:]==[4,8,12]
            assert returned==97+left[1]//4 and mode==(sm<<3)|192|2
            assert s==maps['frame'if sm==5 else'data'][left[0]]and m==maps['data'][right[0]]and d==o['d'][0]-r['first']and d not in inside
            assert next(l for l in r['data']['leaves']if l[0]==right[0])[4]==20
            continue
        if returned==96:
            # Independently prove exact classic root-field relocation. Context
            # nominal identity is a compiler premise, native offsets are not.
            assert loads and op==o['op']==41 and mode==o['mode']and o['m']is None
            sm=mode>>3&7;dm=mode&7;read=sm in(4,5)
            assert (read and dm in(0,1))or(not read and sm in(0,1)and dm in(4,5))
            indirect=o['s']if read else o['d'];direct=o['d']if read else o['s']
            assert len(indirect)==2 and indirect[1]in(0,4,8)and m==2*indirect[1]
            bm=(sm if read else dm)&1;vm=dm if read else sm
            assert (s if read else d)==maps['frame'if bm else'data'][indirect[0]]
            assert (d if read else s)==maps['frame'if vm else'data'][direct[0]]
            for space,old,kind in(('frame'if bm else'data',indirect[0],12),('frame'if vm else'data',direct[0],6 if indirect[1]==8 else 12)):
                leaf=next(l for l in r[space]['leaves']if l[0]==old)
                assert leaf[2:4]==[4,8]and (leaf[4]==kind or (not read and space=='data'and leaf[4]==20))
            continue
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
        if r.get('result_channels'):
            assert r['result_channels']==[6,6,6]and r['result']==[14,24]
            if o['op']==43:
                assert o['mode']==77 and o['d']==[16,0]and types[o['m']]==(12,bytes([224]))
                base=maps['frame'][o['s'][0]]
                assert r['code'][pc]==[43,77,base,0,32,171]
                for k in range(3):assert maps['frame'][o['s'][0]+4*k]==base+8*k
                continue
            if o['op']==41 and o['mode']&7==5:
                assert o['d']in([16,0],[16,4],[16,8])and d==32+2*o['d'][1]and returned==168+o['d'][1]//4
                address=mode>>3&7;assert address in(0,1)and o['m']is None
                assert s==maps['frame'if address else'data'][o['s'][0]]
                leaf=next(l for l in r['frame'if address else'data']['leaves']if l[0]==o['s'][0]);assert leaf[4]in(6,20)and leaf[3]==8
                assert r['code'][pc]==[41,o['mode'],s,0,32+2*o['d'][1],168+o['d'][1]//4]
                continue
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
