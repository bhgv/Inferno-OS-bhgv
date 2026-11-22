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

WINDOW_WORD_FIELDS=dict(zip((4,8,12,16,24,28,32,36,44,48,60,64,68,72,76,80,88,92,96,100),(8,12,16,20,32,36,40,44,56,60,80,84,88,92,96,100,112,116,120,124)))
WINDOW_GEOMETRY_FIELDS={16:{4:8,24:32,68:88},8:{4:8,12:16,24:32,32:40,60:80,68:88,76:96,96:120}}
def window_geometry(o,c,maps,r,pending=None,oldoffset=None,new=None):
 op,mode,source,middle,dest,tag=c;sm=o['mode']>>3&7;dm=o['mode']&7
 assert op==o['op']and mode==o['mode']and tag in(81,82,83,84)
 width={96:8,112:16}.get(r.get('contract',0),0)if tag==81 else o['m'];assert width in(8,16)
 side='d'if tag==82 else's';hm=dm if side=='d'else sm;assert hm in(4,5)and len(o[side])==2 and o[side][1]in WINDOW_GEOMETRY_FIELDS[width]
 space='frame'if hm==5 else'data';base=maps[space][o[side][0]];field=WINDOW_GEOMETRY_FIELDS[width][o[side][1]]
 assert next(l for l in r[space]['leaves']if l[0]==o[side][0])[3:]==[8,12]and r[space]['map'][base//64]&(128>>(base//8%8))
 if tag==81:assert op==39 and o['m']is None and o['d']==[pending,16]and c==[39,45,base,field,32,81];return
 assert op==42 and o['mode']&192==64
 if tag==84:assert o['d']==[pending,oldoffset]and c==[42,o['mode'],base,(field<<8)|width,new,84];return
 vm=sm if tag==82 else dm;part=o['s']if tag==82 else o['d'];assert vm in(0,1)and len(part)==1
 raw=maps['frame'if vm else'data'][part[0]]
 for i in range(0,width,4):assert next(l for l in r['frame'if vm else'data']['leaves']if l[0]==part[0]+i)==[part[0]+i,raw+i,4,4,9]
 assert c==[42,o['mode'],raw if tag==82 else base,(field<<8)|width,base if tag==82 else raw,tag]

def window_word(o,c,maps,first,pending=None,oldoffset=None,new=None):
 op,mode,source,middle,dest,tag=c;sm=o['mode']>>3&7;dm=o['mode']&7;arm=o['mode']&192
 assert op==o['op']
 if tag==88:
  assert op==45 and o['m']is None and arm==0 and dm in(4,5)and sm in(0,1,2)and o['d'][1]in WINDOW_WORD_FIELDS
  assert c==[45,o['mode'],o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]],WINDOW_WORD_FIELDS[o['d'][1]],maps['frame'if dm==5 else'data'][o['d'][0]],88];return
 assert tag in(86,89)and sm in(4,5)and o['s'][1]in WINDOW_WORD_FIELDS
 base=maps['frame'if sm==5 else'data'][o['s'][0]];assert source==base|(WINDOW_WORD_FIELDS[o['s'][1]]<<16)
 if tag==86:assert o['d']==[pending,oldoffset]and dest==new and dm==5
 else:assert dest==(o['d'][0]-first if dm==2 else maps['frame'if dm else'data'][o['d'][0]])
 if arm==0 and op!=45:
  assert tag==89 and dm in(0,1)and mode==o['mode']|(128 if dm else 192)and middle==dest
 else:
  assert mode==o['mode']and middle==(0 if arm==0 else o['m']if arm==64 else maps['frame'if arm==128 else'data'][o['m']])

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
    base,field=original['s']
    if step[5]==233:
        assert field==20 and step==[41,original['mode'],maps['frame'if sm==5 else'data'][base],40,new,233]
        return
    assert field in oldfields
    assert step==[41,original['mode'],maps['frame'if sm==5 else'data'][base],oldfields[field],new,125]

def inthead_argument(original,code,at,maps,pending,oldoffset,offset,r):
    temp=None;tails=0
    while original[at]['op']==OPCODES['ITAIL']:
        a=original[at];sm=a['mode']>>3&7;assert sm in(0,1)and a['mode']==(sm<<3)|1 and a['m']is None
        source=maps['frame'if sm else'data'][a['s'][0]];dest=maps['frame'][a['d'][0]]
        if tails:assert sm==1 and source==dest==temp
        assert code[at]==[a['op'],a['mode'],source,0,dest,253];temp=dest;tails+=1;at+=1
    assert tails<=32
    a=original[at];sm=a['mode']>>3&7;assert a['op']==OPCODES['IHEADP']and sm in(0,1)and a['mode']==(sm<<3)|1 and a['m']is None
    source=maps['frame'if sm else'data'][a['s'][0]];dest=maps['frame'][a['d'][0]]
    if tails:assert sm==1 and source==dest==temp
    assert code[at]==[a['op'],a['mode'],source,0,dest,253];temp=dest;at+=1
    a=original[at];assert a['op']==OPCODES['ICVTCW']and a['mode']==13 and a['m']is None and maps['frame'][a['s'][0]]==temp and a['d']==[pending,oldoffset]
    assert code[at]==[a['op'],13,temp,0,offset,253];at+=1
    a=original[at];assert a['op']==41 and a['mode']==1 and a['m']is None and maps['frame'][a['d'][0]]==temp and next(l for l in r['data']['leaves']if l[0]==a['s'][0])[4]==20
    assert code[at]==[41,1,maps['data'][a['s'][0]],0,temp,253]
    return at+1

def word_argument(original,step,maps,pending,oldoffset,offset,row=None):
    sm=original['mode']>>3&7;arm=original['mode']&192;mm=2 if arm==64 else 1 if arm==128 else 0
    assert original['op']in[OPCODES[n]for n in('IADDW','ISUBW','IMULW','IDIVW','IMODW','IANDW','IORW','IXORW','ISHLW','ISHRW','IINDC')]
    assert sm in(0,1,2)and arm in(64,128,192)and original['d']==[pending,oldoffset]
    source=original['s'][0]if sm==2 else maps['frame'if sm else'data'][original['s'][0]]
    middle=original['m']if mm==2 else maps['frame'if mm else'data'][original['m']]
    if original['op']==OPCODES['IINDC']:
        assert row is not None and sm in(0,1)and next(l for l in row['frame'if sm else'data']['leaves']if l[0]==original['s'][0])[4]==13
    assert step==[original['op'],original['mode'],source,middle,offset,137 if original['op']==OPCODES['IINDC']else 130]

def array_argument(original,code,at,maps,pending,oldoffset,offset):
    a=original[at];b=original[at+1];sm=a['mode']>>3&7;dm=a['mode']&7
    assert a['op']==OPCODES['IINDW']and a['mode']==(sm<<3)|128|dm and sm in(0,1)and dm in(0,1,2)
    source=maps['frame'if sm else'data'][a['s'][0]];index=a['d'][0]if dm==2 else maps['frame'if dm else'data'][a['d'][0]]
    assert code[at]==[OPCODES['IINDL'],a['mode'],source,56,index,127]
    assert b==dict(op=41,mode=45,m=None,s=[a['m'],0],d=[pending,oldoffset])
    assert code[at+1]==[41,45,56,0,offset,127]
    return at+2

# Independent closed layout table: legacy and native field offsets.
LIBRARY_FIELDS={15:([0,4,8,12],[0,8,16,24]),16:([0,4,8,12],[0,8,16,24]),17:([0,4],[0,8]),18:([0,4,8],[0,8,16]),14:([0,4],[0,8]),13:([0,4],[0,8]),12:(list(range(0,28,4)),[0,4,8,16,24,32,40]),10:(list(range(0,32,4)),[0,8,16,24,32,36,40,44]),11:(list(range(0,104,4)),[0,8,12,16,20,24,32,36,40,44,48,56,60,64,72,80,84,88,92,96,100,104,112,116,120,124]),8:(list(range(0,28,4)),list(range(0,56,8))),9:([0,4],[0,4]),1:([0,4,8,12,16,24,32,40,44],[0,8,16,20,24,32,40,48,52]),2:([0,4,8],[0,8,16]),3:([0,4,8],[0,8,16]),4:([0,4,8],[0,8,16]),5:([0,4,8],[0,8,16]),6:(list(range(0,52,4)),[0,8,16,24,32,40,48,56,64,72,76,80,88]),7:([0,4,8,12,16,20],[0,8,12,16,20,24])}
def library_operand(value,mode,record,maps,result=False):
    if mode in(4,5):
        if not record:assert result and mode==5 and value==[16,0];return 32
        old,native=LIBRARY_FIELDS[record];assert len(value)==2 and value[1]in old
        return maps['frame'if mode==5 else'data'][value[0]]|(native[old.index(value[1])]<<16)
    assert not record and mode in(0,1,2)
    return value[0]if mode==2 else maps['frame'if mode else'data'][value[0]]
def library_argument(o,c,maps,pending,oldoffset,offset):
    sid=c[3]>>24;sm=o['mode']>>3&7
    assert c[5]==155 and (sid in LIBRARY_FIELDS or(sid==0 and sm==1 and o['op']==OPCODES['IMOVP'])) and o['m']is None and o['d']==[pending,oldoffset]and o['mode']==(sm<<3)|5 and o['op']in(OPCODES[x]for x in('IMOVP','IMOVW','IMOVL','IMOVF'))
    assert c==[o['op'],o['mode'],library_operand(o['s'],sm,sid,maps),sid<<24,offset,155]
def library_step(o,c,maps,first,types):
    op,mode,source,middle,dest,tag=c;sm=o['mode']>>3&7;dm=o['mode']&7;arm=o['mode']&192
    if tag==154:
        assert source in LIBRARY_FIELDS and op==OPCODES['INEWZ']and o['op']in(OPCODES['INEW'],OPCODES['INEWZ'])and o['m']is None and not middle
        size,mapping=types[o['s'][0]];assert size==({1:48,6:52,7:24,8:28,9:8,10:32,11:104,12:28,13:8,14:8,15:16,16:16,17:8,18:12}.get(source,12))and list(mapping)==([240]if source in(15,16) else[128]if source==17 else[192]if source==18 else[30]if source==12 else[132,38,4]if source==11 else[240]if source==10 else[254]if source==8 else[]if source==9 else[247,136]if source==6 else[132]if source==7 else[192 if source<=3 or source in(13,14) else 224])
        assert mode==o['mode']and dest==library_operand(o['d'],dm,0,maps,True);return
    assert tag==153 and mode==o['mode'];sid=middle>>24;did=(middle>>16)&255;assert sid or did or(op==OPCODES['ILENA']and sm==1 and dm==1)
    expected=OPCODES['INEWAZ']if o['op']in(OPCODES['INEWA'],OPCODES['INEWAZ'])else OPCODES['IBEQL']if o['op']==OPCODES['IBEQW']and op==OPCODES['IBEQL']else OPCODES['IBNEL']if o['op']==OPCODES['IBNEW']and op==OPCODES['IBNEL']else o['op'];assert op==expected
    assert source==library_operand(o['s'],sm,sid,maps)
    branch=dm==2 and op in BRANCHES
    assert dest==(o['d'][0]-first if branch else library_operand(o['d'],dm,did,maps,True))
    if op==OPCODES['INEWAZ']:assert types[o['m']][0]==1 and not any(types[o['m']][1]);mid=0
    else:mid=0 if not arm else o['m']if arm==64 else maps['frame'if arm==128 else'data'][o['m']]
    assert middle==(sid<<24)|(did<<16)|(mid&65535)

def stringarray_argument(original,code,at,maps,pending,oldoffset,new,role):
    a,b=original[at:at+2];sm=a['mode']>>3&7;dm=a['mode']&7
    assert role in(74,76)and a['op']==OPCODES['IINDW']and a['mode']==(sm<<3)|128|dm and sm in(0,1)and dm in(0,1,2)
    source=maps['frame'if sm else'data'][a['s'][0]];index=a['d'][0]if dm==2 else maps['frame'if dm else'data'][a['d'][0]]
    assert code[at]==[OPCODES['IINDL'],a['mode'],source,48,index,role]and b['s']==[a['m'],0]and b['d']==[pending,oldoffset]
    if role==74:assert b['op']==41 and b['mode']==45 and b['m']is None;assert code[at+1]==[41,45,48,0,new,74]
    else:
        mm=1 if b['mode']&192==128 else 0 if b['mode']&192==192 else -1
        assert b['op']==OPCODES['IADDC']and b['mode']==40|(128 if mm else 192)|5 and mm in(0,1)
        assert code[at+1]==[b['op'],b['mode'],48,maps['frame'if mm else'data'][b['m']],new,76]
    return at+2

def rootpath_argument(original,code,at,maps,pending,oldoffset,new,r):
    fields={1:{0:0,20:24,40:48,52:64,56:72,84:104},2:dict(zip(range(0,28,4),range(0,56,8))),3:{0:0,4:8,8:16},4:dict(zip(range(0,20,4),range(0,40,8))),5:{0:0,4:8,8:16,12:24}}
    slot=original[at]['d'][0];temp=maps['frame'][slot];n=0
    while code[at][5]==61:
        o=original[at];c=code[at];sm=o['mode']>>3&7;kind=c[3]>>16
        assert n<8 and o['op']==41 and o['mode']==(sm<<3)|1 and sm in(4,5)and o['m']is None and o['d']==[slot]
        if n:assert sm==5 and o['s'][0]==slot
        assert c==[41,o['mode'],maps['frame'if sm==5 else'data'][o['s'][0]],(kind<<16)|fields[kind][o['s'][1]],temp,61]
        at+=1;n+=1
    o=original[at];assert o['s'][0]==slot and o['mode']==45
    if code[at][5]==155:library_argument(o,code[at],maps,pending,oldoffset,new)
    else:field_argument(o,code[at],new,maps);assert o['d']==[pending,oldoffset]
    at+=1;o=original[at];assert o['op']==41 and o['mode']==1 and o['m']is None and o['d']==[slot]and next(l for l in r['data']['leaves']if l[0]==o['s'][0])[4]==20
    assert code[at]==[41,1,maps['data'][o['s'][0]],0,temp,66]
    return at+1

def rawarray_index(original,code,pc,maps,row):
    o=original[pc];op,mode,s,m,d,role=code[pc];sm=o['mode']>>3&7;dm=o['mode']&7
    assert role==80 and op==OPCODES['IINDX']and o['op']in(OPCODES['IINDX'],OPCODES['IINDW'])and sm==1 and o['mode']==8|128|dm and mode==o['mode']and o['m']is not None
    assert m>>8 in(1,2,3)and m&255 in(48,56)and(o['op']!=OPCODES['IINDW']or m>>8==1)
    assert s==maps['frame'][o['s'][0]]and d==(o['d'][0]if dm==2 else maps['frame'if dm else'data'][o['d'][0]])
    assert next(l for l in row['frame']['leaves']if l[0]==o['s'][0])[4]in(3,20)

def rawarray_use(original,code,pc,maps,row,pending=None,oldoffset=None,new=None):
    o=original[pc];op,mode,s,m,d,role=code[pc];sm=o['mode']>>3&7;dm=o['mode']&7;arm=o['mode']&192;so=do=0
    assert role==65 and op==o['op']and mode==o['mode']
    assert sm==5 or(dm==5 and new is None)
    n=o['m']if op in(OPCODES['IMOVM'],OPCODES['IHEADM'])else 4
    assert n in(4,8,12,16)
    def borrow(addr):
        earlier=[j for j in range(pc)if code[j][5]==80 and original[j]['m']==addr[0]];assert earlier
        at=earlier[-1];cl=code[at][3]>>8;assert addr[1]%4==0 and addr[1]+n<={1:4,2:12,3:16}[cl]
        return code[at][3]&255
    if sm==5:assert s==borrow(o['s']);so=o['s'][1]
    else:assert sm in(0,1,2)and s==(o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]])
    if new is not None:
        assert pending is not None and dm==5 and o['d']==[pending,oldoffset]and d==new
    elif dm==5:assert d==borrow(o['d']);do=o['d'][1]
    elif dm==2:assert op in BRANCHES and d==o['d'][0]-row['first']
    else:assert dm in(0,1)and d==maps['frame'if dm else'data'][o['d'][0]]
    middle=0 if arm==0 else o['m']if arm==64 else maps['frame'if arm==128 else'data'][o['m']]
    assert m==(so<<24)|(do<<16)|(middle&65535)
    if op==OPCODES['IHEADM']:assert sm in(0,1)and n==16
    for addr,space,wire in((o['s'],'frame'if sm else'data',sm),(o['d'],'frame'if dm else'data',dm)):
        if wire not in(0,1)or(op==OPCODES['IHEADM']and addr is o['s']):continue
        for k in range(0,n,4):assert next(l for l in row[space]['leaves']if l[0]==addr[0]+k)[2:]==[4,4,9]

def rawarray_operation(o,c,maps,types,row):
    op,mode,s,m,d,role=c;sm=o['mode']>>3&7;dm=o['mode']&7;arm=o['mode']&192
    assert role==79 and dm==1 and mode==o['mode']and d==maps['frame'][o['d'][0]]
    if o['op']in(OPCODES['INEWA'],OPCODES['INEWAZ']):
        assert op==OPCODES['INEWAZ']and arm==64 and m in(1,2,3)and types[o['m']][0]=={1:4,2:12,3:16}[m]and not any(types[o['m']][1])
    else:
        assert op==o['op']and op in(OPCODES['ISLICEA'],OPCODES['ISLICELA'])and m>>24 in(1,2,3)
        mid=o['m']if arm==64 else maps['frame'if arm==128 else'data'][o['m']];assert m==((m>>24)<<24)|(mid&65535)
    assert sm in(0,1,2)and s==(o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]])

def string_head_argument(original, code, at, end, maps, pending, oldoffset, new, kind, r):
    temp=None;tails=0
    while at<end and original[at]['op']==OPCODES['ITAIL']and original[at]['mode']&7==1:
        a=original[at];sm=a['mode']>>3&7;assert a['mode']==(sm<<3)|1 and sm in(0,1)and a['m']is None
        dst=maps['frame'][a['d'][0]];src=maps['frame'if sm else'data'][a['s'][0]]
        if temp is not None:assert src==temp==dst and sm==1
        temp=dst;assert code[at]==[a['op'],a['mode'],src,0,dst,252];tails+=1;at+=1
    assert 1<=tails<=32
    a=original[at]
    assert a['op']==OPCODES['IHEADP'if kind==13 else'ITAIL']and a['mode']==13 and a['m']is None and maps['frame'][a['s'][0]]==temp
    assert a['d']==[pending,oldoffset]
    assert code[at]==[a['op'],a['mode'],temp,0,new,252];at+=1
    a=original[at];assert a['op']==41 and a['mode']==1 and a['m']is None and next(l for l in r['data']['leaves']if l[0]==a['s'][0])[4]==20 and maps['frame'][a['d'][0]]==temp
    assert code[at]==[41,1,maps['data'][a['s'][0]],0,temp,252];at+=1
    return at


def verify(data,r,imports,loads=False,rows=None):
    assert r['accepted']and r['schema']in (('inferno-typed-external-v1','inferno-typed-load-v1','inferno-typed-body-v2')if loads else ('inferno-typed-external-v1',))and (loads or r.get('externals'))and (rows is not None or 'calls'not in r)
    if r.get('result')==[14,24]and not r.get('result_aggregate'):assert r.get('result_channels')==[6,6,6]
    _,old,types=BASE['READ']['legacy_instructions'](data,with_types=True)
    layout=json.loads(json.dumps(r));layout.update(schema='inferno-typed-body-v2');BASE['verify'](data,layout,validate_code=False)
    maps={space:{leaf[0]+i:leaf[1]+i for leaf in r[space]['leaves']for i in range(leaf[2])}for space in ('frame','data')}
    original=old[r['first']:r['first']+len(r['code'])];covered=set();inside=set();previous=-1
    for first,end,target,descriptor,offset,contract,inherited in r.get('calls',[]):
        assert rows is not None and previous<first<end<len(original);previous=end
        callee=next(x for x in rows if x['first']==target);assert callee['accepted']and callee['descriptor']==descriptor
        assert contract==(callee['result_aggregate']if callee.get('result_aggregate')else 96 if callee.get('result_geometry')==8 else 112 if callee.get('result_geometry')==16 else 72 if callee.get('result_channels')==[6,6,6]else 12 if callee.get('result_words')else 16 if callee.get('result_mixed')else 48 if callee.get('result_pair')else callee['result'][1]|(16 if callee['result'][0]in ROOTS else 0))
        at=first+1;preps=set()
        pending=original[first]['d'][0];assert original[first]['op']==5 and original[first]['s']==[descriptor]
        assert r['code'][first]==[5,17,descriptor,0,40,0]
        for arg in callee['args']:
            prep=False;screenprep=None;idprep=None;o=original[at]
            while loads and r['code'][at][5]==80:
                rawarray_index(original,r['code'],at,maps,r);at+=1;o=original[at]
            if loads and r['code'][at][5]==65:
                assert arg[1]in(1,9)and arg[3]==(original[at]['m']if original[at]['op']==OPCODES['IMOVM']else 4)
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);rawarray_use(original,r['code'],at,maps,r,pending,oldoffset,arg[0]);at+=1;continue
            if loads and arg[1:]==[1,12,12]and o['op']==45:
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0])
                for k in range(3):
                    a=original[at];sm=a['mode']>>3&7;assert a['op']==45 and a['mode']==(sm<<3)|5 and a['m']is None and a['d']==[pending,oldoffset+4*k]
                    src=a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]];assert r['code'][at]==[45,a['mode'],src,0,arg[0]+4*k,64];at+=1
                continue
            if loads and arg[1:]==[1,16,16]and o['op']==OPCODES['IHEADM']:
                sm=o['mode']>>3&7;oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);assert sm in(0,1)and o['mode']==(sm<<3)|64|5 and o['m']==16 and o['d']==[pending,oldoffset]
                assert r['code'][at]==[o['op'],o['mode'],maps['frame'if sm else'data'][o['s'][0]],16,arg[0],64];at+=1;continue
            if loads and r['code'][at][5]==74:
                assert arg[1:]==[13,4,8];oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);at=stringarray_argument(original,r['code'],at,maps,pending,oldoffset,arg[0],74);continue
            if loads and r['code'][at][5]==75:
                assert arg[1:]==[13,4,8]and arg==callee['args'][-1]and at+1==end-bool(contract)
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);sm=o['mode']>>3&7;mm=1 if o['mode']&192==128 else 0 if o['mode']&192==192 else -1
                assert o['op']==OPCODES['IADDC']and o['d']==[pending,oldoffset]and sm in(0,1)and mm in(0,1)
                assert r['code'][at]==[o['op'],o['mode'],maps['frame'if sm else'data'][o['s'][0]],maps['frame'if mm else'data'][o['m']],arg[0],75];at+=1;continue
            if loads and r['code'][at][5]==61:
                assert arg[1]in ROOTS;oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);at=rootpath_argument(original,r['code'],at,maps,pending,oldoffset,arg[0],r);continue
            if loads and arg[1]==13 and r['code'][at][5]==153:
                a,b,z=original[at:at+3];temp=maps['frame'][a['d'][0]];oldtarget=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0])
                assert a['op']==41 and a['mode']==41 and a['s'][1]==16;library_step(a,r['code'][at],maps,r['first'],types);assert r['code'][at][3]==12<<24
                assert b['op']==OPCODES['IADDC']and b['mode']==133 and b['m']==a['d'][0]and b['d']==[pending,oldtarget]
                assert r['code'][at+1]==[b['op'],133,maps['data'][b['s'][0]],temp,arg[0],64]
                assert z['op']==41 and z['mode']==1 and z['m']is None and z['d']==a['d']and next(l for l in r['data']['leaves']if l[0]==z['s'][0])[4]==20
                assert r['code'][at+2]==[41,1,maps['data'][z['s'][0]],0,temp,181];at+=3;continue
            if loads and arg[1]==13 and r['code'][at][5]==44:
                a,b,z=original[at:at+3];oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);sm=a['mode']>>3&7;temp=maps['frame'][a['d'][0]]
                assert a['op']==OPCODES['IHEADP']and a['mode']==(sm<<3)|1 and sm in(0,1)and a['m']is None and next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]==10
                assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm else'data'][a['s'][0]],0,temp,44]
                bs=b['mode']>>3&7;bm=b['mode']&192;assert b['op']==OPCODES['IADDC']and b['d']==[pending,oldoffset]and bs in(0,1)and bm in(128,192)
                source=maps['frame'if bs else'data'][b['s'][0]];middle=maps['frame'if bm==128 else'data'][b['m']];assert temp in(source,middle)
                assert r['code'][at+1]==[b['op'],b['mode'],source,middle,arg[0],64]
                assert z['op']==41 and z['mode']==1 and z['m']is None and z['d']==a['d']and next(l for l in r['data']['leaves']if l[0]==z['s'][0])[4]==20 and r['code'][at+2]==[41,1,maps['data'][z['s'][0]],0,temp,181]
                at+=3;continue
            if loads and r['code'][at][5]==155:
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);library_argument(o,r['code'][at],maps,pending,oldoffset,arg[0]);at+=1;continue
            if loads and r['code'][at][5]==84:
                assert arg[1]==1 and arg[2]==arg[3]and arg[3]in(8,12,16)
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);window_geometry(original[at],r['code'][at],maps,r,pending,oldoffset,arg[0]);at+=1;continue
            if loads and r['code'][at][5]==152:
                assert arg[1:]==[7,8,8]and o['op']==OPCODES['ICVTWF']and o['m']is None and o['d'][0]==pending
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);assert o['d'][1]==oldoffset
                sm=o['mode']>>3&7;assert sm in(0,1,2)
                source=o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]]
                assert r['code'][at]==[o['op'],o['mode'],source,0,arg[0],152];at+=1;continue
            if loads and arg[1]==13 and r['code'][at][5]==252:
                old=next(x[0]for x in callee['frame']['leaves']if x[1]==arg[0])
                at=string_head_argument(original,r['code'],at,end,maps,pending,old,arg[0],13,r)
                continue
            if loads and arg[1]==9 and r['code'][at][5]==46:
                a=original[at];assert a['op']==OPCODES['IADDW']and a['mode']==145 and a['s']==[1]
                source=maps['frame'][a['m']];dest=maps['frame'][a['d'][0]]
                assert r['code'][at]==[a['op'],145,1,source,dest,46]and r['code'][at+1][5]==137 and r['code'][at+1][3]==dest
                for off in(a['m'],a['d'][0]):assert next(l for l in r['frame']['leaves']if l[0]==off)[2:]==[4,4,9]
                at+=1;o=original[at]
            if loads and (r['code'][at][5]==127 or r['code'][at][5]in(130,137)and arg[1]==9):
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0])
                if r['code'][at][5]==127:
                    assert arg[1]==12 and callee['frame']['map'][arg[0]//64]&(128>>(arg[0]//8%8))
                    at=array_argument(original,r['code'],at,maps,pending,oldoffset,arg[0]);continue
                assert arg[1]==9;word_argument(original[at],r['code'][at],maps,pending,oldoffset,arg[0],r);at+=1;continue
            if loads and arg[1]==9 and r['code'][at][5]==253:
                oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]);assert arg[2:]==[4,4]
                assert not(callee['frame']['map'][arg[0]//64]&(128>>(arg[0]//8%8)))
                at=inthead_argument(original,r['code'],at,maps,pending,oldoffset,arg[0],r);continue
            if loads and arg[1]==1 and arg[3]!=4 and (o['op']!=OPCODES['IMOVM']or o['m']!=arg[3])and r['code'][at][5]!=235:
                assert arg[2]==arg[3]and arg[3]in(8,12,16)
                k=0
                while k<arg[3]//4:
                    oldoffset=next(l[0]for l in callee['frame']['leaves']if l[1]==arg[0]+4*k)
                    assert next(l for l in callee['frame']['leaves']if l[0]==oldoffset)==[oldoffset,arg[0]+4*k,4,4,9]
                    assert not(callee['frame']['map'][(arg[0]+4*k)//64]&(128>>((arg[0]+4*k)//8%8)))
                    if r['code'][at][5]==253:at=inthead_argument(original,r['code'],at,maps,pending,oldoffset,arg[0]+4*k,r);k+=1
                    elif r['code'][at][5]==130:word_argument(original[at],r['code'][at],maps,pending,oldoffset,arg[0]+4*k);at+=1;k+=1
                    else:
                        x=original[at];sm=x['mode']>>3&7;assert sm in(0,1,2)and x['d']==[pending,oldoffset]
                        source=x['s'][0]if sm==2 else maps['frame'if sm==1 else'data'][x['s'][0]]
                        if x['op']==OPCODES['IMOVM']:
                            n=x['m'];assert n in(8,16)and 4*k+n<=arg[3]and sm in(0,1)and x['mode']==(sm<<3)|64|5
                            for v in range(0,n,4):
                                assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==x['s'][0]+v)==[x['s'][0]+v,source+v,4,4,9]
                                assert next(l for l in callee['frame']['leaves']if l[0]==oldoffset+v)==[oldoffset+v,arg[0]+4*k+v,4,4,9]
                            assert r['code'][at]==[x['op'],x['mode'],source,n,arg[0]+4*k,64];k+=n//4
                        else:
                            assert x['op']==OPCODES['IMOVW']and x['mode']==(sm<<3)|5 and x['m']is None
                            assert r['code'][at]==[x['op'],x['mode'],source,0,arg[0]+4*k,64];k+=1
                        at+=1
                continue
            if loads and arg[1]==13 and r['code'][at][5]==237:
                sm=o['mode']>>3&7;assert o['op']==OPCODES['ICVTWC']and sm in(4,5)and o['mode']==(sm<<3)|1 and o['m']is None and o['s'][1]==36
                owner=maps['frame'if sm==5 else'data'][o['s'][0]];temp=maps['frame'][o['d'][0]]
                assert r['code'][at]==[o['op'],o['mode'],owner,72,temp,237]
                for space,root_at in(('frame'if sm==5 else'data',owner),('frame',temp)):assert r[space]['map'][root_at//64]&(128>>(root_at//8%8))
                idprep=(o['d'][0],temp);at+=1;o=original[at]
            if loads and arg[1]==13 and o['op']==41 and o['mode']&7==1:
                assert at+3<end and original[at+1]['op']==OPCODES['ISLICEA']and original[at+2]['op']==OPCODES['ICVTAC']
                view=o['d'];assert original[at+1]['d']==view and original[at+2]['s']==view
                clear=original[at+3];assert clear['op']==41 and clear['mode']==1 and clear['d']==view
                for addr in(o['s'],view):assert next(l for l in r['frame']['leaves']if l[0]==addr[0])[4]==3
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                preps.update((at,at+1,at+3));at+=2;o=original[at];prep=True
            if loads and arg[1]==1 and r['code'][at][5]==235:
                sm=o['mode']>>3&7;assert o['op']==41 and sm in(4,5)and o['mode']==(sm<<3)|1 and o['m']is None and o['s'][1]==4
                owner=next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0]);temp=next(l for l in r['frame']['leaves']if l[0]==o['d'][0])
                assert owner[2:4]==[4,8]and owner[4]in set(ROOTS)|{20}and temp[2:4]==[4,8]and temp[4]in set(ROOTS)|{20}
                for space,root_at in(('frame'if sm==5 else'data',owner[1]),('frame',temp[1])):assert r[space]['map'][root_at//64]&(128>>(root_at//8%8))
                assert r['code'][at]==[41,o['mode'],owner[1],8,temp[1],235]
                screenprep=(o['d'][0],temp[1]);at+=1;o=original[at];assert o['s'][0]==screenprep[0]and o['mode']>>3&7==5
            sm=o['mode']>>3&7
            assert o['d']==[pending,next(x[0]for x in callee['frame']['leaves']if x[1]==arg[0])]
            if arg[1:]==[1,4,4]:
                assert loads and sm in(0,1,2)and ((o['op']==45 and o['m']is None and o['mode']==(sm<<3)|5)or(o['op']==42 and o['m']==4 and sm in(0,1)and o['mode']==(sm<<3)|64|5))
                assert next(l for l in callee['frame']['leaves']if l[1]==arg[0])[2:]==[4,4,9]
                source=o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]]
                if sm!=2:assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0])[2:]==[4,4,9]
                assert r['code'][at]==[45,(sm<<3)|5,source,0,arg[0],64]
            elif arg[1]in(1,14):
                assert loads and arg[2]==arg[3]and arg[3]in(8,12,16)
                assert o['op']==OPCODES['IMOVM']and o['mode']==(sm<<3)|64|5 and o['m']==arg[3]
                width=arg[3];role=r['code'][at][5]
                oldtarget=o['d'][1]
                for i in range(0,width,4):
                    assert next(l for l in callee['frame']['leaves']if l[0]==oldtarget+i)==[oldtarget+i,arg[0]+i,4,4,9]
                    assert not(callee['frame']['map'][(arg[0]+i)//64]&(128>>((arg[0]+i)//8%8)))
                if sm in(0,1):
                    source=maps['frame'if sm else'data'][o['s'][0]];assert role==64
                    for i in range(0,width,4):assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0]+i)==[o['s'][0]+i,source+i,4,4,9]
                else:
                    assert sm in(4,5);field=o['s'][1];source=maps['frame'if sm==5 else'data'][o['s'][0]]
                    if field%8==4:
                        assert field in((4,)if width==16 else(4,12));assert role==(217 if width==16 else 218+(field-4)//8)
                    else:
                        assert field in((0,16)if width==16 else(0,8,16,24));assert role==(160+field//16 if width==16 else 162+field//8)
                    assert r['frame'if sm==5 else'data']['map'][source//64]&(128>>(source//8%8))
                assert r['code'][at]==[o['op'],o['mode'],source,width,arg[0],role]
            elif r['code'][at][5]in(125,233):
                assert loads and arg[1]in ROOTS and (r['code'][at][5]!=233 or arg[1]==13);field_argument(o,r['code'][at],arg[0],maps)
            elif o['op']in(OPCODES['ICVTAC'],OPCODES['ICVTCA']):
                assert loads and arg[1:]==([13,4,8]if o['op']==OPCODES['ICVTAC']else[3,4,8])and sm in(0,1)and o['m']is None
                assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0])[4]==(3 if o['op']==OPCODES['ICVTAC']else 13)
                assert r['code'][at]==[o['op'],o['mode'],maps['frame'if sm else'data'][o['s'][0]],0,arg[0],64]
            elif o['op']==OPCODES['IADDC']:
                mm=1 if o['mode']&192==128 else 0 if o['mode']&192==192 else -1
                assert loads and arg[1:]==[13,4,8]and sm in(0,1)and mm in(0,1)
                if target==r['first']:assert inherited and r['code'][end+1][0]==OPCODES['IRET']
                else:assert len(callee['args'])==1 and at==first+(2 if idprep else 1) and at+(2 if idprep else 1)==end-bool(contract) and arg[0]==64
                assert r['code'][at]==[o['op'],o['mode'],maps['frame'if sm else'data'][o['s'][0]],maps['frame'if mm else'data'][o['m']],arg[0],64]
            else:
                assert r['code'][at]==[o['op'],o['mode'],o['s'][0]if sm==2 else maps['frame'if sm==1 else'data'][o['s'][0]],0,arg[0],64]
            at+=2 if prep else 1
            if idprep is not None:
                clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[idprep[0]]
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,idprep[1],238];at+=1
            if screenprep is not None:
                clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[screenprep[0]]
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,screenprep[1],126];at+=1
            if at<end and r['code'][at][5]==182:
                clear=original[at];assert arg[1]in ROOTS and o['op']==41 and sm==1 and clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==o['s']
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][clear['d'][0]],182];at+=1
        assert at+bool(contract)==end
        if contract:
            binding=original[end-1];assert binding['op']==39 and binding['d']==[pending,16]and binding['m']is None
            if r['code'][end-1][5]==131:
                index=original[first-1];assert loads and contract==24 and not inherited and index['op']==OPCODES['IINDW']and index['mode']>>3&7==1 and binding['mode']==45 and binding['s']==[index['m'],0]
                assert offset==maps['frame'][index['s'][0]]and r['code'][end-1]==[39,13,offset,0,32,131]
            elif r['code'][end-1][5]==81:
                assert loads and contract in(96,112)and not inherited
                proof=dict(r,contract=contract);window_geometry(binding,r['code'][end-1],maps,proof,pending);assert offset==r['code'][end-1][2]
            elif r['code'][end-1][5]==93:
                assert loads and contract==24 and not inherited and binding['mode']==45 and binding['s'][1]==48 and offset==maps['frame'][binding['s'][0]]
                assert r['code'][end-1]==[39,45,offset,24,32,93]
            elif r['code'][end-1][5]==85:
                assert loads and contract in(80,92,116,124)and not inherited and binding['mode']==13
                assert offset==maps['frame'][binding['s'][0]]and r['code'][end-1]==[39,13,offset,0,32,85]
            elif inherited:
                assert binding['mode']==45 and binding['s']==[16,0]and offset==32
                assert r['code'][end-1]==[118,13,32,0,32,64]
            else:
                assert binding['mode']in(5,13)and (binding['mode']!=5 or contract==24)and offset==maps['frame'if binding['mode']==13 else'data'][binding['s'][0]]
                assert r['code'][end-1]==[39,binding['mode'],offset,0,32,128|contract]
        spawn=original[end]['op']==OPCODES['ISPAWN']
        if spawn:assert loads and not contract and not inherited and callee['result']==[0,0]
        opcode=OPCODES['ISPAWN']if spawn else 4
        assert original[end]=={'op':opcode,'mode':10,'m':None,'s':[pending],'d':[target]}
        assert r['code'][end]==[opcode,10,40,0,target,186 if spawn else (78 if contract==24 and r.get('result_aggregate')in(80,124)else 32)if inherited else 0]
        covered.update(set(range(first,end+1))-preps);inside.update(range(first+1,end+1))
    previous=-1
    for v in r.get('variadics',[]):
        first,end=v['first'],v['end'];assert loads and previous<first<end<len(original) and not covered.intersection(range(first,end+1));previous=end
        op=v['operation'];assert op in(0,1,3) and v['contract']==(24 if op!=3 else 4)
        choices=imports if imports and isinstance(imports[0],list)else[imports]
        assert any(v['binding']<len(t)and t[v['binding']]=={'name':v['name'],'signature':v['signature']}for t in choices)
        assert v['name']=={0:'sprint',1:'aprint',3:'fprint'}[op] and v['signature']=={0:0x4c0624b6,1:0x77442d46,3:0xf46486c8}[op]
        o=original[first];assert o['op']==5 and o['mode']==17 and o['m']is None
        size,roots=types[o['s'][0]];count=v['count']+(1 if op!=3 else 2);frame=v['frame']
        assert frame['legacy_bytes']==size and frame['bytes']==64+8*count
        leaves=frame['leaves'];assert len(leaves)==count
        assert leaves[:1 if op!=3 else 2]==[[32+4*j,64+8*j,4,8,12 if op==3 and j==0 else 13]for j in range(1 if op!=3 else 2)]
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
            a=original[at];sm=a['mode']>>3&7
            if r['code'][at][5]==153:
                assert kind==13 and j>=(1 if op!=3 else 2)and a['op']==41 and a['mode']==41 and a['s'][1]==16
                library_step(a,r['code'][at],maps,r['first'],types);assert r['code'][at][3]==12<<24
                b=original[at+1];z=original[at+2];temp=maps['frame'][a['d'][0]]
                assert b['op']==OPCODES['IADDC']and b['mode']==133 and b['m']==a['d'][0]and b['d']==[pending,legacy]
                assert r['code'][at+1]==[b['op'],133,maps['data'][b['s'][0]],temp,native,57]
                assert z['op']==41 and z['mode']==1 and z['m']is None and z['d']==a['d']and next(l for l in r['data']['leaves']if l[0]==z['s'][0])[4]==20
                assert r['code'][at+2]==[41,1,maps['data'][z['s'][0]],0,temp,223];at+=3;continue
            if r['code'][at][5]==53:
                assert op==3 and j==0 and kind==12 and a['op']==41 and sm in(4,5)and a['mode']==(sm<<3)|5 and a['m']is None and a['d']==[pending,legacy]
                c=r['code'][at];id=c[3]>>24;field=a['s'][1];nativefield=40 if id==8 else 32 if field==20 else 40
                assert (id==8 and field==20)or(id==12 and field in(20,24))
                root=maps['frame'if sm==5 else'data'][a['s'][0]]
                assert c==[41,a['mode'],(nativefield<<16)|root,id<<24,64,53]
                assert r['frame'if sm==5 else'data']['map'][root//64]&(128>>(root//8%8))
                at+=1;continue
            if r['code'][at][5]==59:
                assert op==3 and j==0 and kind==12 and a['op']==41 and a['mode']==41 and a['s'][1]==40 and a['m']is None
                owner=maps['frame'][a['s'][0]];temp=maps['frame'][a['d'][0]];b=original[at+1];z=original[at+2]
                assert b==dict(op=41,mode=45,m=None,s=[a['d'][0],20],d=[pending,legacy])and z['op']==41 and z['mode']==1 and z['m']is None and z['d']==a['d']
                assert next(l for l in r['data']['leaves']if l[0]==z['s'][0])[4]==20
                assert r['code'][at:at+3]==[[41,41,owner,48,temp,59],[41,45,temp,40,64,59],[41,1,maps['data'][z['s'][0]],0,temp,59]]
                for off in(owner,temp):assert r['frame']['map'][off//64]&(128>>(off//8%8))
                at+=3;continue
            if r['code'][at][5]==58:
                assert kind==13 and j>=(1 if op!=3 else 2)and a['op']==OPCODES['IHEADP']and sm in(0,1)and a['mode']==(sm<<3)|5 and a['m']is None and a['d']==[pending,legacy]
                old=a['s'][0];source=maps['frame'if sm else'data'][old];assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==old)[2:]==[4,8,10]
                assert r['code'][at]==[a['op'],a['mode'],source,0,native,58]
            elif r['code'][at][5]==95:
                assert kind==9 and j>=(1 if op!=3 else 2)and a['op']==45 and sm in(4,5)and a['mode']==(sm<<3)|5 and a['m']is None and a['s'][1]in(16,36,40,44)and a['d']==[pending,legacy]
                source=maps['frame'if sm==5 else'data'][a['s'][0]];field={16:32,36:72,40:76,44:80}[a['s'][1]]
                leaf=next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==a['s'][0]);assert leaf[3:]==[8,12]
                assert r['frame'if sm==5 else'data']['map'][source//64]&(128>>(source//8%8))
                assert r['code'][at]==[OPCODES['ICVTWL'],a['mode'],source,field,native,95]
            elif r['code'][at][5]==222:
                assert kind==13 and j>=(1 if op!=3 else 2)and a['op']==41 and sm in(4,5)and a['mode']==(sm<<3)|5 and a['m']is None and a['s'][1]==0 and a['d']==[pending,legacy]
                source=maps['frame'if sm==5 else'data'][a['s'][0]]
                leaf=next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==a['s'][0]);assert leaf[3:]==[8,12]
                assert r['frame'if sm==5 else'data']['map'][source//64]&(128>>(source//8%8))
                assert r['code'][at]==[41,a['mode'],source,0,native,222]
            else:
                assert a['op']==oldop and sm in((0,1)if kind in(4,7,12,13)else(0,1,2))and a['m']is None and a['d']==[pending,legacy]
                source=a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]]
                assert r['code'][at]==[newop,a['mode'],source,0,native,64]
            if op==3 and j==0 and sm==1:fd=a['s'][0]
            at+=1
            if at<end-1 and r['code'][at][5]==223:
                clear=original[at];assert kind==13 and sm==1 and a['op']==41 and clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==a['s']
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][clear['d'][0]],223];at+=1
            if op==3 and j==0 and original[at]['mode']&7==1:
                clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[fd]
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][fd],0]
                assert not any(a['mode']>>3&7==1 and a['s']==[fd]for a in original[at+1:end-1]);at+=1
        assert at==end-1;binding=original[at];assert binding['op']==39 and binding['mode']in(13,45)and binding['m']is None and binding['d']==[pending,16]
        if v.get('inherited'):
            assert v['inherited']==1 and binding['mode']==45 and binding['s'][0]==16
            old=binding['s'][1];c=r.get('result_aggregate',0)
            # Closed aggregate String leaves retain their classic field identity.
            if old:assert (c==48 and old==4)or(c==124 and old==8)
            assert v['resultoffset']==32+2*old and r['code'][at]==[118,13,32,2*old,32,128|v['contract']]
        else:
            assert binding['mode']==13 and v['resultoffset']==maps['frame'][binding['s'][0]]and r['code'][at]==[39,13,v['resultoffset'],0,32,128|v['contract']]
        call=original[end];mode,offset=v['module'];assert mode in(0,1)and call['op']==9 and call['mode']==8|64|mode and call['m']==v['binding']and call['s']==[pending]
        assert offset==maps['frame'if mode else'data'][call['d'][0]]and r['code'][end]==[9,8|64|mode,40,v['binding'],offset,(78 if r.get('result_aggregate')in(48,80,124)else 32)if v.get('inherited')else 0]
        covered.update(range(first,end+1));inside.update(range(first+1,end+1))
    previous=-1
    for e in r.get('externals',[]):
        first,end=e['first'],e['end'];assert previous<first<end<len(original)and not covered.intersection(range(first,end+1));previous=end
        choices=imports if imports and isinstance(imports[0],list)else[imports]
        assert any(e['binding']<len(t)and t[e['binding']]=={'name':e['name'],'signature':e['signature']}for t in choices)
        o=original[first];mode,offset=e['module'];assert mode in (0,1,4,5)and o['op']==11 and o['mode']==(mode<<3)|64|1
        assert offset==(library_operand(o['s'],mode,5,maps)|(5<<24)if mode in(4,5)else maps['frame'if mode else 'data'][o['s'][0]])
        pending=o['d'][0];assert r['code'][first]==[11,o['mode'],offset,e['binding'],40,0]and o['m']==e['binding']
        oldoffset=32
        at=first+1;aggregates={a['index']:a['layout']for a in e.get('aggregate_args',[])};nominals={a['index']:a.get('nominal')for a in e.get('aggregate_args',[])}
        assert len(aggregates)==len(e.get('aggregate_args',[]))
        for j,(new,kind,oldbytes,width)in enumerate(e['args']):
            if at<end and r['code'][at][5]==76:
                assert loads and kind==13 and oldbytes==4 and width==8;oldoffset=(oldoffset+3)&~3;at=stringarray_argument(original,r['code'],at,maps,pending,oldoffset,new,76);oldoffset+=4;continue
            if at<end and r['code'][at][5]==61:
                assert loads and kind in ROOTS;oldoffset=(oldoffset+3)&~3;at=rootpath_argument(original,r['code'],at,maps,pending,oldoffset,new,r);oldoffset+=oldbytes;continue
            if at<end and r['code'][at][5]==86 and kind==9:
                assert loads and kind==9 and oldbytes==width==4;oldoffset=(oldoffset+3)&~3
                window_word(original[at],r['code'][at],maps,r['first'],pending,oldoffset,new);oldoffset+=4;at+=1;continue

            if at<end and r['code'][at][5]==143:
                assert loads and kind==12 and oldbytes==4 and width==8
                oldoffset=(oldoffset+3)&~3;a=original[at];h=original[at+1];field=original[at+2];clear=original[at+3];sm=a['mode']>>3&7;dm=a['mode']&7
                assert a['op']==OPCODES['IINDW']and a['mode']==(sm<<3)|128|dm and sm in(0,1)and dm in(0,1,2)
                assert h==dict(op=41,mode=41,m=None,s=[a['m'],0],d=h['d'])and len(h['d'])==1
                array=maps['frame'if sm else'data'][a['s'][0]];index=a['d'][0]if dm==2 else maps['frame'if dm else'data'][a['d'][0]];temp=maps['frame'][h['d'][0]]
                assert r['code'][at]==[OPCODES['IINDL'],a['mode'],array,56,index,143]and r['code'][at+1]==[41,41,56,0,temp,143]
                assert field['s']==[h['d'][0],48]and field['d']==[pending,oldoffset];field_argument(field,r['code'][at+2],new,maps)
                assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==h['d']and next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at+3]==[41,1,maps['data'][clear['s'][0]],0,temp,126]
                at+=4;oldoffset+=4;continue

            if at<end and r['code'][at][5]==153 and original[at]['op']!=OPCODES['IMOVP']:
                assert loads and (kind==3 or original[at]['op']==OPCODES['ILENA']) and original[at]['op']in(OPCODES['ILENA'],OPCODES['IMOVW']);library_step(original[at],r['code'][at],maps,r['first'],types);at+=1
            if at<end and r['code'][at][5]==155:
                assert loads;oldoffset=(oldoffset+(7 if oldbytes==8 else 3))&~(7 if oldbytes==8 else 3);library_argument(original[at],r['code'][at],maps,pending,oldoffset,new);at+=1
                if at<end and r['code'][at][5]==156:
                    a=original[at];c=r['code'][at];sm=a['mode']>>3&7;arm=a['mode']&192;sid=c[3]>>24
                    assert kind==3 and a['op']==OPCODES['ISLICEA']and a['d']==[pending,oldoffset]and (sid==0 or sid in LIBRARY_FIELDS) and arm in(64,128,192)
                    mid=a['m']if arm==64 else maps['frame'if arm==128 else'data'][a['m']]
                    assert c==[a['op'],a['mode'],library_operand(a['s'],sm,sid,maps),(sid<<24)|(mid&65535),new,156];at+=1
                oldoffset+=oldbytes;continue
            if at<end and r['code'][at][5]in(127,130)and kind!=1:
                if r['code'][at][5]==127:
                    assert loads and kind==12;oldoffset=(oldoffset+3)//4*4
                    at=array_argument(original,r['code'],at,maps,pending,oldoffset,new);oldoffset+=oldbytes;continue
                assert loads and kind==9;oldoffset=(oldoffset+3)//4*4
                word_argument(original[at],r['code'][at],maps,pending,oldoffset,new);at+=1;oldoffset+=4;continue
            if at<end and r['code'][at][5]==253 and kind==9:
                assert loads and oldbytes==width==4
                oldoffset=(oldoffset+3)//4*4;at=inthead_argument(original,r['code'],at,maps,pending,oldoffset,new,r);oldoffset+=4
                continue
            if at<end and r['code'][at][5]==252:
                assert loads and kind in(10,13)and oldbytes==4 and width==8
                oldtarget=32
                for _,pk,poldbytes,_ in e['args'][:j]:
                    align=8 if pk in(4,7)else 1 if pk==5 else 4;oldtarget=(oldtarget+align-1)//align*align+poldbytes
                oldtarget=(oldtarget+3)//4*4
                at=string_head_argument(original,r['code'],at,end,maps,pending,oldtarget,new,kind,r)
                oldoffset=oldtarget+oldbytes
                continue
            listclear=None
            if at<end and r['code'][at][5]==112:
                assert loads and kind==10 and oldbytes==4 and width==8
                temp=None;count=0;cons=None
                while at<end and r['code'][at][5]==112:
                    a=original[at];sm=a['mode']>>3&7
                    assert a['m']is None and a['mode']&7==1 and sm in(0,1,2)
                    target=maps['frame'][a['d'][0]]
                    assert next(l for l in r['frame']['leaves']if l[0]==a['d'][0])[2:4]==[4,8]
                    if temp is None:
                        assert a['op']==41 and sm==0 and next(l for l in r['data']['leaves']if l[0]==a['s'][0])[4]==20
                        temp=target;listclear=(temp,maps['data'][a['s'][0]])
                    else:
                        assert a['op']in(OPCODES['ICONSP'],OPCODES['ICONSW'])and target==temp
                        if cons is None:cons=a['op']
                        assert cons==a['op']
                        if sm!=2:assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]in((9,)if cons==OPCODES['ICONSW']else(13,20))
                        else:assert cons==OPCODES['ICONSW']
                        count+=1
                    assert r['code'][at]==[a['op'],a['mode'],a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]],0,target,112];at+=1
                assert 0<count<=256 and original[at]['s']==[next(old for old,v in maps['frame'].items()if v==temp)]and original[at]['mode']==13
            if at<end and r['code'][at][5]==187:
                assert loads and kind==10;temp=None;count=0
                while at<end and r['code'][at][5]==187:
                    a=original[at];sm=a['mode']>>3&7;assert a['m']is None and a['mode']&7==1 and sm in(0,1)
                    target=maps['frame'][a['d'][0]]
                    if temp is None:
                        assert a['op']==41 and next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]in(10,20)
                        temp=target;listclear=(temp,None)
                    else:
                        assert a['op']==OPCODES['ICONSP']and target==temp
                        assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]in(13,20);count+=1
                    assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm else'data'][a['s'][0]],0,target,187];at+=1
                assert 0<count<=256 and original[at]['mode']==13 and maps['frame'][original[at]['s'][0]]==temp
            if at+3<end and r['code'][at][5]==69:
                assert loads and kind==13 and oldbytes==4 and width==8
                a,b,v,z=original[at:at+4];sm=a['mode']>>3&7;low=b['mode']>>3&7;arm=b['mode']&192
                assert a['op']==41 and a['mode']==(sm<<3)|1 and sm in(0,1)and a['m']is None
                slot=a['d'][0];temp=maps['frame'][slot];source=maps['frame'if sm else'data'][a['s'][0]]
                assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]==3
                assert b['op']==OPCODES['ISLICEA']and b['mode']&7==1 and b['d']==[slot]and low in(0,1,2)and arm in(64,128,192)
                lo=b['s'][0]if low==2 else maps['frame'if low else'data'][b['s'][0]];hi=b['m']if arm==64 else maps['frame'if arm==128 else'data'][b['m']]
                oldoffset=(oldoffset+3)&~3;assert v==dict(op=OPCODES['ICVTAC'],mode=13,m=None,s=[slot],d=[pending,oldoffset])
                assert z['op']==41 and z['mode']==1 and z['m']is None and z['d']==[slot]and next(l for l in r['data']['leaves']if l[0]==z['s'][0])[4]==20
                assert r['code'][at:at+4]==[[41,a['mode'],source,0,temp,69],[b['op'],b['mode'],lo,hi,temp,69],[v['op'],13,temp,0,new,70],[41,1,maps['data'][z['s'][0]],0,temp,71]]
                oldoffset+=4;at+=4;continue
            stringprep=None
            if at+2<end and r['code'][at][5]==87:
                assert loads and kind==13;a,b,c=original[at:at+3];sm=a['mode']>>3&7
                assert a['op']==OPCODES['ILENC']and a['m']is None and a['mode']==(sm<<3)|1 and sm in(0,1)
                assert b['op']==41 and b['mode']==a['mode']and b['s']==a['s']and b['m']is None
                assert c['op']==OPCODES['ISLICEC']and c['mode']&7==1 and c['mode']&192==128 and c['m']==a['d'][0]and c['d']==b['d']
                oldtemp=b['d'][0];temp=maps['frame'][oldtemp];source=maps['frame'if sm else'data'][a['s'][0]];length=maps['frame'][a['d'][0]];cs=c['mode']>>3&7
                for space,old,k in(('frame'if sm else'data',a['s'][0],13),('frame',a['d'][0],9),('frame',oldtemp,13)):
                    assert next(l for l in r[space]['leaves']if l[0]==old)[4]==k
                low=c['s'][0]if cs==2 else maps['frame'if cs else'data'][c['s'][0]]
                assert r['code'][at:at+3]==[[a['op'],a['mode'],source,0,length,87],[41,b['mode'],source,0,temp,87],[c['op'],c['mode'],low,length,temp,87]]
                at+=3;stringprep=(temp,oldtemp,[oldtemp])
            if at<end and (r['code'][at][5]in(63,180)or(r['code'][at][5]==153 and original[at]['op']==OPCODES['IMOVP'])):
                assert loads and kind==13
                oldtemp=original[at]['d'][0];temp=maps['frame'][oldtemp];nprep=0;temps=[]
                if r['code'][at][5]==153:
                    assert r['code'][at][3]==12<<24 and original[at]['s'][1]==16;library_step(original[at],r['code'][at],maps,r['first'],types);temps.append(oldtemp);at+=1;nprep=1
                while at<end and r['code'][at][5]in(63,180):
                    a=original[at];sm=a['mode']>>3&7;target=a['d'][0];dest=maps['frame'][target]
                    assert a['mode']&7==1
                    if a['op']==OPCODES['ICVTWC']:
                        assert target not in temps and len(temps)<8;temps.append(target)
                        if r['code'][at][5]==63:
                            assert sm in(4,5)and len(a['s'])==2 and a['s'][1]in WINDOW_WORD_FIELDS
                            assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm==5 else'data'][a['s'][0]],WINDOW_WORD_FIELDS[a['s'][1]],dest,63]
                        else:
                            assert a['m']is None and sm in(0,1,2)
                            source=a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]]
                            if sm!=2:assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[2:]==[4,4,9]
                            assert r['code'][at]==[a['op'],a['mode'],source,0,dest,180]
                    else:
                        assert target in temps and a['op']==OPCODES['IADDC']and sm in(0,1)
                        mm=1 if a['mode']&192 in(0,128)else 0;middle=target if a['m']is None else a['m']
                        assert (sm==1 and a['s'][0]in temps)or(mm==1 and middle in temps)
                        assert r['code'][at]==[a['op'],(sm<<3)|(128 if mm else 192)|1,maps['frame'if sm else'data'][a['s'][0]],maps['frame'if mm else'data'][middle],dest,180]
                    nprep+=1;at+=1
                assert 1<=nprep<=16;stringprep=(temp,oldtemp,temps)
            a=original[at];sm=a['mode']>>3&7
            alignment=8 if oldbytes==8 and kind!=1 else 4;oldoffset=(oldoffset+alignment-1)&~(alignment-1)
            if kind==1:
                assert loads and j in aggregates
                layout=aggregates.pop(j);n=1 if oldbytes==4 else 2 if oldbytes==8 else 4 if oldbytes==16 else 0
                assert nominals[j]in(None,'Draw.Pointer','Draw.Chans') and (nominals[j]!='Draw.Pointer' or r['code'][at][5]in(64,227))
                assert n and width==oldbytes and layout=={'legacy_bytes':oldbytes,'bytes':width,'map':[0],'leaves':[[4*k,4*k,4,4,9]for k in range(n)]}
                if r['code'][at][5]==141:
                    assert a['op']==OPCODES['IHEADP']and sm in(0,1)and a['mode']==(sm<<3)|1 and a['m']is None
                    source=maps['frame'if sm else'data'][a['s'][0]];temp=maps['frame'][a['d'][0]]
                    assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[2:]==[4,8,10]
                    assert next(l for l in r['frame']['leaves']if l[0]==a['d'][0])[2:]==[4,8,12]
                    assert r['code'][at]==[a['op'],a['mode'],source,0,temp,141]
                    at+=1;a=original[at];sm=a['mode']>>3&7;assert sm==5 and maps['frame'][a['s'][0]]==temp and a['s'][1]==4 and r['code'][at][5]==217
                if r['code'][at][5]==166:
                    assert a['op']==41 and sm in(4,5)and a['mode']==(sm<<3)|1 and a['m']is None and a['s'][1]in(8,20)
                    temp=maps['frame'][a['d'][0]]
                    assert r['code'][at]==[41,a['mode'],maps['frame'if sm==5 else'data'][a['s'][0]],16 if a['s'][1]==8 else 24,temp,166]
                    at+=1;a=original[at];sm=a['mode']>>3&7;assert sm==5 and maps['frame'][a['s'][0]]==temp
                if r['code'][at][5]==84:
                    assert nominals[j]is None;window_geometry(a,r['code'][at],maps,r,pending,oldoffset,new);at+=1
                elif a['op']==OPCODES['IMOVM']and sm in(4,5):
                    field=a['s'][1]
                    assert oldbytes in(8,16)and a['mode']==(sm<<3)|64|5 and a['m']==oldbytes and a['d']==[pending,oldoffset]
                    # Window.r has classic offset4 and native offset8; Image starts at0.
                    if r['code'][at][5]==227:
                        assert oldbytes==16 and field==0 and nominals[j]=='Draw.Pointer'
                        root=maps['frame'if sm==5 else'data'][a['s'][0]]
                        assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==a['s'][0])[4]==12
                        assert r['frame'if sm==5 else'data']['map'][root//64]&(128>>(root//8%8))
                        role=227
                    elif r['code'][at][5]==230:
                        assert oldbytes==8 and field==4 and nominals[j]is None
                        root=maps['frame'if sm==5 else'data'][a['s'][0]]
                        assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==a['s'][0])[4]==12
                        assert r['frame'if sm==5 else'data']['map'][root//64]&(128>>(root//8%8))
                        role=230
                    elif field%8==4:
                        assert field in((4,)if oldbytes==16 else(4,12))
                        role=217 if oldbytes==16 else 218+(field-4)//8
                    else:
                        assert field in((0,16)if oldbytes==16 else(0,8,16,24))
                        role=(160+field//16)if oldbytes==16 else(162+field//8)
                    assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm==5 else'data'][a['s'][0]],oldbytes,new,role];at+=1
                elif a['op']==OPCODES['IMOVM']and a['m']==oldbytes:
                    assert sm in(0,1)and a['mode']==(sm<<3)|64|5 and a['m']==oldbytes and a['d']==[pending,oldoffset]
                    source=maps['frame'if sm else'data'][a['s'][0]]
                    for k in range(n):
                        assert maps['frame'if sm else'data'][a['s'][0]+4*k]==source+4*k
                        assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0]+4*k)[2:]==[4,4,9]
                    assert r['code'][at]==[a['op'],a['mode'],source,width,new,64];at+=1
                else:
                    k=0
                    while k<n:
                        a=original[at];sm=a['mode']>>3&7;step=r['code'][at]
                        if step[5]==253:
                            at=inthead_argument(original,r['code'],at,maps,pending,oldoffset+4*k,new+4*k,r);k+=1;continue
                        if step[5]==86:
                            window_word(a,step,maps,r['first'],pending,oldoffset+4*k,new+4*k);at+=1;k+=1;continue
                        if step[5]==130:
                            word_argument(a,step,maps,pending,oldoffset+4*k,new+4*k);at+=1;k+=1;continue
                        assert a['d']==[pending,oldoffset+4*k]and sm in(0,1,2)
                        source=a['s'][0]if sm==2 else maps['frame'if sm else'data'][a['s'][0]]
                        if a['op']==OPCODES['IMOVM']:
                            widthpart=a['m'];assert sm in(0,1)and a['mode']==(sm<<3)|64|5 and widthpart in(8,16)and k+widthpart//4<=n
                            for q in range(0,widthpart,4):assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0]+q)==[a['s'][0]+q,source+q,4,4,9]
                            assert step==[a['op'],a['mode'],source,widthpart,new+4*k,64];k+=widthpart//4
                        else:
                            assert a['op']==OPCODES['IMOVW']and a['mode']==(sm<<3)|5 and a['m']is None
                            assert step==[a['op'],a['mode'],source,0,new+4*k,64];k+=1
                        at+=1
                if at<end and r['code'][at][5]==126:
                    clear=original[at];assert sm==5 and a['op']==OPCODES['IMOVM']and clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[a['s'][0]]
                    assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                    assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][clear['d'][0]],126];at+=1
                oldoffset+=oldbytes;continue
            assert j not in aggregates
            assert a['d']==[pending,oldoffset];oldoffset+=oldbytes
            if r['code'][at][5]in(125,233):
                assert loads and kind in ROOTS and (r['code'][at][5]!=233 or kind==13);field_argument(a,r['code'][at],new,maps)
            elif r['code'][at][5]==67:
                assert loads and kind==9 and a['op']==OPCODES['IMOVW']and sm in(4,5)and a['m']is None and a['s'][1]in range(0,44,4)
                assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm==5 else'data'][a['s'][0]]|(a['s'][1]<<16),0,new,67]
            elif a['op']==OPCODES['ILENA']:
                assert loads and kind==9 and oldbytes==width==4 and sm in(0,1)and a['m']is None
                assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==a['s'][0])[4]==3
                assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm else'data'][a['s'][0]],0,new,64]
            elif a['op']==OPCODES['IADDC']:
                mm=1 if a['mode']&192==128 else 0 if a['mode']&192==192 else -1
                assert loads and kind==13 and oldbytes==4 and width==8 and sm in(0,1)and mm in(0,1)
                for space,old in (('frame'if sm else'data',a['s'][0]),('frame'if mm else'data',a['m'])):
                    assert any(leaf[0]==old and leaf[2:4]==[4,8]and (leaf[4]==13 or stringprep is not None and space=='frame'and old in stringprep[2]and leaf[4]in ROOTS)for leaf in r[space]['leaves'])
                assert r['code'][at]==[a['op'],a['mode'],maps['frame'if sm else'data'][a['s'][0]],maps['frame'if mm else'data'][a['m']],new,64]
            else:
                assert r['code'][at]==[a['op'],a['mode'],a['s'][0]if sm==2 else maps['frame'if sm==1 else 'data'][a['s'][0]],0,new,64]
            at+=1
            if stringprep is not None:
                temp,oldtemp,temps=stringprep;assert a['op']==OPCODES['IADDC']and ((sm==1 and a['s']==[oldtemp])or(a['mode']&192==128 and a['m']==oldtemp))
                for old in temps:
                    clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==[old]
                    assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                    assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][old],181];at+=1
            if listclear is not None:
                temp,nilroot=listclear;clear=original[at]
                assert clear['op']==41 and clear['mode']==1 and clear['m']is None
                assert (nilroot is None or maps['data'][clear['s'][0]]==nilroot)and maps['frame'][clear['d'][0]]==temp
                if nilroot is None:
                    assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                    assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,temp,187]
                else:assert r['code'][at]==[41,1,nilroot,0,temp,112]
                at+=1
            if at<end and r['code'][at][5]==126:
                assert loads and kind in ROOTS and a['op']==41 and sm==1
                clear=original[at];assert clear['op']==41 and clear['mode']==1 and clear['m']is None and clear['d']==a['s']
                assert next(l for l in r['data']['leaves']if l[0]==clear['s'][0])[4]==20
                assert r['code'][at]==[41,1,maps['data'][clear['s'][0]],0,maps['frame'][clear['d'][0]],126];at+=1
        assert not aggregates
        contract=e['contract'];assert at+bool(contract)==end
        assert (contract==72)==(e.get('result_channels')==[6,6,6])
        assert e.get('result_geometry',0)==({96:8,112:16}.get(contract,0))
        if contract:
            b=original[end-1];assert b['op']==39 and b['d']==[pending,16]
            if r['code'][end-1][5]==81 and e['inherited']:
                assert loads and contract==96 and r.get('result_aggregate')==112 and e['resultoffset']==32 and b['mode']==45 and b['s']==[16,8]
                assert r['code'][end-1]==[39,45,32,8,32,81]
            elif r['code'][end-1][5]==81:
                assert loads and contract in(96,112)and not e['inherited'];window_geometry(b,r['code'][end-1],maps,dict(r,contract=contract),pending);assert e['resultoffset']==r['code'][end-1][2]
            elif contract in(96,112)and e['inherited']:
                assert loads and r.get('result_geometry')=={96:8,112:16}[contract]and b['mode']==45 and b['s']==[16,0]
            elif contract in(12,96,112):
                assert loads and not e['inherited']and b['mode']in(5,13)
                space='frame'if b['mode']==13 else'data';width={12:12,96:8,112:16}[contract];old=b['s'][0];native=maps[space][old]
                assert e['resultoffset']==native and native>=(64 if space=='frame'else 0)and native%4==0
                for i in range(0,width,4):
                    leaf=next(l for l in r[space]['leaves']if l[0]==old+i)
                    assert leaf==[old+i,native+i,4,4,9]
                    assert not(r[space]['map'][(native+i)//64]&(128>>((native+i)//8%8)))

            if contract==88:
                assert loads and not e['inherited']and(e['name'],e['signature'])in(('fstat',0xda4499c2),('stat',0x319328dd))and b['mode']==13
                oldbase=b['s'][0];base=maps['frame'][oldbase];oldfields=(0,8,12,16,20,24,32,36,40,44,48,56,64,68);newfields=(0,8,16,24,32,40,48,52,56,60,64,72,80,84);kinds=(9,13,13,13,13,4,9,9,9,9,9,4,9,9)
                for oldoff,newoff,kind in zip(oldfields,newfields,kinds):
                    assert next(l for l in r['frame']['leaves']if l[0]==oldbase+oldoff)==[oldbase+oldoff,base+newoff,8 if kind==4 else 4,4 if kind==9 else 8,kind]
                for i in range(88):assert bool(r['frame']['map'][(base+i)//64]&(128>>((base+i)//8%8)))==(8<=i<40)
            if r['code'][end-1][5]==85:
                assert loads and not e['inherited']and contract in(80,92,116,124)and b['mode']==13 and b['m']is None
                kinds={80:[13,9],92:[9]*5,116:[12,6],124:[13,12,13]}[contract];native_offsets={80:[0,8],92:[0,4,8,12,16],116:[0,8],124:[0,8,16]}[contract];width={80:16,92:20,116:16,124:24}[contract]
                old=b['s'][0];base=maps['frame'][old];assert e['resultoffset']==base and base>=64 and base%8==0
                for j,(kind,off)in enumerate(zip(kinds,native_offsets)):
                    assert next(l for l in r['frame']['leaves']if l[0]==old+4*j)==[old+4*j,base+off,4,8 if kind in(6,12,13)else 4,kind]
                for j in range(width):assert bool(r['frame']['map'][(base+j)//64]&(128>>((base+j)//8%8)))==(contract==80 and j<8 or contract in(116,124))
                assert r['code'][end-1]==[39,13,base,0,32,85]
            elif r['code'][end-1][5]==81:pass
            elif r['code'][end-1][5]==60:
                fields=({20:288,24:296}if r['code'][end-1][3]in(288,296)else{0:0,20:24,40:48,52:64,56:72,84:104})
                assert loads and contract==24 and not e['inherited']and b['mode']==45 and b['m']is None and b['s'][1]in fields
                base=maps['frame'][b['s'][0]];assert e['resultoffset']==base and r['code'][end-1]==[39,45,base,fields[b['s'][1]],32,60]
                assert next(l for l in r['frame']['leaves']if l[0]==b['s'][0])[3:]==[8,12]
            elif r['code'][end-1][5]==131:
                index=original[first-1];assert loads and contract==24 and not e['inherited']and index['op']==OPCODES['IINDW']and index['mode']>>3&7==1 and b['mode']==45 and b['s']==[index['m'],0]
                assert e['resultoffset']==maps['frame'][index['s'][0]]and r['code'][end-1]==[39,13,e['resultoffset'],0,32,131]
            elif r['code'][end-1][5]==245:
                assert loads and contract==4 and not e['inherited']and b['mode']==45 and b['m']is None and b['s'][1]==12
                source=maps['frame'][b['s'][0]];assert source==e['resultoffset']
                leaf=next(l for l in r['frame']['leaves']if l[0]==b['s'][0]);assert leaf[2:4]==[4,8]and leaf[4]in set(ROOTS)|{20}
                assert r['frame']['map'][source//64]&(128>>(source//8%8))
                assert r['code'][end-1]==[39,45,source,12,32,245]
            elif r['code'][end-1][5]==220:
                assert loads and contract==24 and not e['inherited']and b['mode']==45 and b['m']is None and b['s'][1]==20
                source=maps['frame'][b['s'][0]];assert source==e['resultoffset']
                leaf=next(l for l in r['frame']['leaves']if l[0]==b['s'][0]);assert leaf[3:]==[8,12]
                assert r['frame']['map'][source//64]&(128>>(source//8%8))
                assert r['code'][end-1]==[39,45,source,24,32,220]
            elif e['inherited']:assert b['s']==[16,0]and r['code'][end-1]==[118,13,32,0,32,64]
            else:
                assert b['mode']in(5,13)and (b['mode']!=5 or contract in(24,96,112))
                assert r['code'][end-1]==[39,b['mode'],maps['frame'if b['mode']==13 else'data'][b['s'][0]],0,32,128|contract]
        assert original[end]=={'op':9,'mode':8|64|mode,'m':e['binding'],'s':[pending],'d':o['s']}
        assert r['code'][end]==[9,8|64|mode,40,e['binding'],offset,(78 if contract==96 and r.get('result_aggregate')==112 else 32)if e['inherited']else 0]
        covered.update(range(first,end+1));inside.update(range(first+1,end+1))
    for pc,(op,mode,s,m,d,returned)in enumerate(r['code']):
        if pc in covered:continue
        o=original[pc]
        if returned==80:rawarray_index(original,r['code'],pc,maps,r);continue
        if returned==79:rawarray_operation(o,r['code'][pc],maps,types,r);continue
        if returned==65:rawarray_use(original,r['code'],pc,maps,r);continue
        if returned==77:
            c=r['result_aggregate'];width={12:12,48:16,72:24,80:16,92:20,96:8,112:16,116:16,124:24}[c];count={12:3,48:2,72:3,80:2,92:5,96:2,112:4,116:2,124:3}[c]
            sm=o['mode']>>3&7;assert loads and op==o['op']and o['d'][0]==16 and o['d'][1]%4==0
            field=o['d'][1]//4;assert field<count;off=8*field if c in(48,72,116,124)else(8 if c==80 and field else 4*field)
            n=8 if op==41 else 4 if op==45 else o['m']if op==42 else width if op==OPCODES['IMOVMP']else 0
            root=c in(48,72,116,124)or(c==80 and field==0)
            assert n and n<=width-off and op in(41,45,42,OPCODES['IMOVMP'])and d==32 and m==(c<<16)|(n<<8)|off
            assert mode==(sm<<3)|(64 if op in(42,OPCODES['IMOVMP'])else 0)|5 and sm in(0,1,2)
            assert s==(o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]])
            if op in(41,45):assert o['m']is None and root==(op==41)
            elif op==42:
                assert c in(12,92,96,112)and sm in(0,1)and n%4==0
                for k in range(0,n,4):assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0]+k)==[o['s'][0]+k,s+k,4,4,9]
            else:
                assert c in(48,72,116,124)and field==0 and sm==1 and types[o['m']][0]==4*count and list(types[o['m']][1])==[192 if c in(48,116) else 224]
                for k in range(count):assert maps['frame'][o['s'][0]+4*k]==s+8*k
            continue
        if returned==68:
            assert loads and op==o['op']==OPCODES['IMOVMP']and mode==o['mode']==69 and o['s']and len(o['s'])==1 and o['d'][1]==0 and m in(11,12) and types[o['m']][0]==(104 if m==11 else 28) and list(types[o['m']][1])==([132,38,4]if m==11 else[30])
            assert [s,m,d]==[maps['data'][o['s'][0]],m,maps['frame'][o['d'][0]]];continue
        if returned==73:
            sm=mode>>3&7;dm=mode&7;si=m>>24;di=(m>>20)&15;sf=(m>>8)&4095;df=m&255
            fields={1:{0:0,20:24,40:48,52:64,56:72,84:104},2:dict(zip(range(0,28,4),range(0,56,8))),3:{0:0,4:8,8:16},4:dict(zip(range(0,20,4),range(0,40,8))),5:{0:0,4:8,8:16,12:24}}
            assert loads and op==o['op']==41 and mode==o['mode']and o['m']is None and sm in(4,5)and dm in(4,5)
            assert sf==fields[si][o['s'][1]]and df==fields[di][o['d'][1]]and s==maps['frame'if sm==5 else'data'][o['s'][0]]and d==maps['frame'if dm==5 else'data'][o['d'][0]];continue
        if returned==62:
            sm=mode>>3&7;dm=mode&7;width=m&255;sf=(m>>16)&255;df=(m>>8)&255;sw=m>>24
            assert loads and op==o['op']==42 and mode==o['mode']and sm in(4,5)and dm in(4,5)and o['m']==width and width in(8,16)and sw in(0,1)
            assert df==WINDOW_GEOMETRY_FIELDS[width][o['d'][1]]and sf==(WINDOW_GEOMETRY_FIELDS[width][o['s'][1]]if sw else o['s'][1])
            assert [s,d]==[maps['frame'if sm==5 else'data'][o['s'][0]],maps['frame'if dm==5 else'data'][o['d'][0]]];continue
        if returned==63:
            sm=mode>>3&7;dm=mode&7;parse=op==OPCODES['ICVTCW'];owner=o['d']if parse else o['s'];om=dm if parse else sm;vm=sm if parse else dm;value=o['s']if parse else o['d']
            assert loads and op==o['op']and op in(OPCODES['ICVTCW'],OPCODES['ICVTWC'])and mode==o['mode']and o['m']is None and om in(4,5)and vm in(0,1)
            assert m==WINDOW_WORD_FIELDS[owner[1]]and (d if parse else s)==maps['frame'if om==5 else'data'][owner[0]]and (s if parse else d)==maps['frame'if vm else'data'][value[0]];continue
        if returned in(138,139):
            sm=o['mode']>>3&7;dm=o['mode']&7;assert loads and sm in(4,5)and o['s'][1]in(0,4,8,12)and op==o['op']and mode==o['mode']
            source=maps['frame'if sm==5 else'data'][o['s'][0]];assert source==s
            if returned==138:
                arm=mode&192;assert op in(OPCODES['IBEQW'],OPCODES['IBNEW'])and dm==2 and arm in(128,192)
                assert m==(o['s'][1]<<16)|maps['frame'if arm==128 else'data'][o['m']]and d==o['d'][0]-r['first']
            else:
                assert op==OPCODES['IMOVM']and dm in(4,5)and o['s'][1]==o['d'][1]==0 and o['m']==m==16 and mode&192==64
                assert d==maps['frame'if dm==5 else'data'][o['d'][0]]
            continue
        if returned==94:
            assert loads and op==o['op']and op in(OPCODES['ISEND'],OPCODES['IRECV'])and mode==o['mode']and o['m']is None
            send=op==OPCODES['ISEND'];cm=mode&7 if send else mode>>3&7;vm=mode>>3&7 if send else mode&7;ca=o['d']if send else o['s'];va=o['s']if send else o['d'];field=2*ca[1]
            assert cm in(4,5)and ca[1]in(0,4,8,12,16)and m==(field<<8)|(4 if field==0 else 24)
            value=va[0]if vm==2 else maps['frame'if vm else'data'][va[0]];channel=maps['frame'if cm==5 else'data'][ca[0]]
            assert [s,d]==([value,channel]if send else[channel,value]);continue
        if returned in(90,92,107,108,109,110):
            assert loads and op==o['op']and mode==o['mode'];sm=mode>>3&7;dm=mode&7
            if returned==90:
                assert op==OPCODES['IMOVMP']and sm in(4,5)and dm in(4,5)and o['s'][1]==o['d'][1]==0 and types[o['m']][0]==28 and list(types[o['m']][1])==[254]and m==8 and s==maps['frame'if sm==5 else'data'][o['s'][0]]and d==maps['frame'if dm==5 else'data'][o['d'][0]]
            elif returned==92:
                if s in(5,6):
                    assert op==OPCODES['INEWCMP']and sm==2 and dm in(4,5)and o['m']==m==0
                    assert o['d'][1]==4*(s-5) and types[o['s'][0]][0]==16 and list(types[o['s'][0]][1])==[16 if s==5 else 80]
                    assert d==maps['frame'if dm==5 else'data'][o['d'][0]]
                else:
                    assert op==OPCODES['INEWCMP']and sm==2 and dm in(4,5)and o['m']==0 and s in(2,3) and o['d'][1]==(12 if s==2 else 32) and types[o['s'][0]][0]==4*s and list(types[o['s'][0]][1])==([192]if s==2 else[224]) and m==0 and d==maps['frame'if dm==5 else'data'][o['d'][0]]
            elif returned==108:
                assert op in(OPCODES['INEWCW'],OPCODES['INEWCP'])and sm==3 and dm in(4,5)and o['m']==m==0 and o['s']is None and o['d'][1]in((52,)if s==576 else(0,4,8,12)if s>=256 else(0,4,8,16))
                assert s==(576 if s==576 else(256 if s>=256 else 0)|2*o['d'][1])and d==maps['frame'if dm==5 else'data'][o['d'][0]]
            elif returned==109:
                assert op==OPCODES['IRECV']and sm in(0,1)and dm==5 and o['m']is None and s==maps['frame'if sm else'data'][o['s'][0]]
                if m==4:assert o['d']==[16,0]and d==32
                else:assert o['d'][1]in(0,4,8)and m==(65536|(o['d'][1]<<8)|4)and d==maps['frame'][o['d'][0]]
            elif returned==110:assert op==OPCODES['IMOVM']and sm in(4,5)and dm in(4,5)and o['s'][1]==0 and o['d'][1]==4 and o['m']==m==16 and s==maps['frame'if sm==5 else'data'][o['s'][0]]and d==maps['frame'if dm==5 else'data'][o['d'][0]]
            else:
                assert op in(OPCODES['ISEND'],OPCODES['IRECV'])and o['m']is None
                send=op==OPCODES['ISEND'];cm=dm if send else sm;vm=sm if send else dm;ca=o['d']if send else o['s'];va=o['s']if send else o['d'];channel=d if send else s;value=s if send else d;width=m&255
                assert width in(16,24)and vm in(0,1)and channel==maps['frame'if cm in(1,5)else'data'][ca[0]]and m==((((16 if width==16 else 64)<<8)if cm in(4,5)else 0)|width)
                if cm in(4,5):assert (ca[1],width)in((8,16),(12,16),(32,24))
                else:assert cm in(0,1)
                for i in range(width//8):assert next(l for l in r['frame'if vm else'data']['leaves']if l[0]==va[0]+4*i)[1:4]==[value+8*i,4,8]
            continue
        if returned==111:
            assert loads and mode==o['mode'];sm=mode>>3&7;dm=mode&7;arm=mode&192
            if op==OPCODES['ISLICELA']:
                assert op==o['op']and sm in(0,1)and dm in(0,1)and arm in(64,128,192)
                assert [s,m,d]==[library_operand(o['s'],sm,0,maps),o['m']if arm==64 else maps['frame'if arm==128 else'data'][o['m']],library_operand(o['d'],dm,0,maps)]
            elif op==OPCODES['IINDL']:
                assert o['op']==OPCODES['IINDW']and sm in(0,1)and dm in(4,5)and arm==128 and o['d'][1]==36
                assert [s,m,d]==[maps['frame'if sm else'data'][o['s'][0]],48,maps['frame'if dm==5 else'data'][o['d'][0]]|(72<<16)]
                assert r['code'][pc+1][5]==255 and r['code'][pc+1][0]==41
            else:
                before=original[pc-1];assert before['op']==OPCODES['IINDW']and r['code'][pc-1][5]==255 and o['s']==[before['m'],0]and s==48
                if op==41:assert o['op']==41 and mode==45 and m==0 and o['m']is None and o['d']==[16,0]and d==32
                else:assert (o['op'],op)in((OPCODES['IBEQW'],OPCODES['IBEQL']),(OPCODES['IBNEW'],OPCODES['IBNEL']))and mode==234 and m==maps['data'][o['m']]and d==o['d'][0]-r['first']
                inside.add(pc)
            continue
        if returned==159:
            assert loads and op==o['op']==OPCODES['ISLICELA']and mode==o['mode'];sm=mode>>3&7;dm=mode&7;arm=mode&192
            assert s==library_operand(o['s'],sm,0,maps)and d==library_operand(o['d'],dm,0,maps)and m==(o['m']if arm==64 else maps['frame'if arm==128 else'data'][o['m']]);continue
        if returned==142:
            index=original[pc-2];head=original[pc-1];assert loads and op==o['op']==41 and mode==o['mode']==45 and o['m']is None and m==(7<<24)and d==48
            assert index['op']==OPCODES['IINDW']and r['code'][pc-2][5]==255 and head['op']==OPCODES['IHEADP']and r['code'][pc-1][5]==213 and o['s']==[head['d'][0],20]and o['d']==[index['m'],0]
            assert s==maps['frame'][head['d'][0]]|(24<<16);continue
        if returned in(153,154):
            assert loads;library_step(o,r['code'][pc],maps,r['first'],types);continue
        if returned in(157,158):
            assert loads and mode==o['mode']
            if returned==157:
                sid=m>>24;did=(m>>16)&255;sm=mode>>3&7;dm=mode&7;gap=m>>8&255;assert gap in(0,1,11);assert op==o['op']==OPCODES['IINDB']and mode&192==128 and m==(sid<<24)|(did<<16)|(gap<<8)|48
                assert s==library_operand(o['s'],sm,sid,maps)and d==library_operand(o['d'],dm,did,maps)and r['code'][pc+1+gap][5]==158
                if sid:assert sid==1 and o['s'][1]==4
                if gap==1:
                    shift=r['code'][pc+1];assert sid==0 and sm==1 and shift[0]==OPCODES['ISHRW']and shift[1]==(2<<3|128|1)and shift[5]==0 and 0<=shift[2]<=31 and shift[4]>=64 and (shift[4]+4<=s or shift[4]>=s+8)
                if gap==11:
                    assert sid==did==0 and sm==1
                    sites=[x for x in r.get('calls',[])if pc<x[0]and x[1]<pc+12];assert len(sites)==2 and sites[0][2]==sites[1][2]
                    callee=next(x for x in rows if x['first']==sites[0][2]);assert callee['result']==[9,4]and not any(callee['frame']['map'])and not callee.get('calls')and not callee.get('externals')
                    allowed={OPCODES[x]for x in('IRET','IMOVW','IADDW','ISUBW','IBEQW','IBNEW','IBLTW','IBLEW','IBGTW','IBGEW','IJMP')};assert all(x[0]in allowed for x in callee['code'])
                inside.update(range(pc+1,pc+2+gap))
            else:
                back=next(n for n in range(1,min(12,pc)+1)if r['code'][pc-n][5]==157 and(r['code'][pc-n][3]>>8&255)==n-1)
                prev=original[pc-back];assert r['code'][pc-back][5]==157 and (r['code'][pc-back][3]>>8&255)==back-1 and op==o['op']and op in(OPCODES[x]for x in('IMOVB','ICVTBW','ICVTWB'))and o['m']is None and m==0
                sm=mode>>3&7;dm=mode&7;read=sm==5;assert (o['s']if read else o['d'])==[prev['m'],0]
                value=library_operand(o['d']if read else o['s'],dm if read else sm,0,maps,True)
                assert [s,d]==([48,value]if read else[value,48])
            continue
        if returned==131:
            sm=o['mode']>>3&7;dm=o['mode']&7
            assert loads and op==OPCODES['IINDL']and o['op']==OPCODES['IINDW']and sm==1 and o['mode']==8|128|dm and original[pc+1]['op']in(OPCODES['IFRAME'],OPCODES['IMFRAME'])
            index=o['d'][0]if dm==2 else maps['frame'if dm else'data'][o['d'][0]]
            assert [op,mode,s,m,d,returned]==[OPCODES['IINDL'],o['mode'],maps['frame'][o['s'][0]],48,index,131]
            continue
        if returned in(82,83):
            assert loads;window_geometry(o,[op,mode,s,m,d,returned],maps,r);continue
        if returned in(88,89):
            assert loads;window_word(o,[op,mode,s,m,d,returned],maps,r['first']);continue
        if returned==133:
            sm=o['mode']>>3&7;dm=o['mode']&7;arm=o['mode']&192;mm=2 if arm==64 else 1 if arm==128 else 0
            assert loads and op==o['op']and mode==o['mode']and sm in(4,5)
            field=o['s'][1];native=s>>16;assert native==field or(field,native)in((16,32),(36,72),(40,76),(44,80))
            assert s&65535==maps['frame'if sm==5 else'data'][o['s'][0]]
            assert m==(0 if arm==0 else o['m']if mm==2 else maps['frame'if mm else'data'][o['m']])
            assert d==(o['d'][0]-r['first']if dm==2 else maps['frame'if dm else'data'][o['d'][0]])
            continue
        if returned==254:
            sm=o['mode']>>3&7;dm=o['mode']&7;assert loads and o['op']in(OPCODES['INEWA'],OPCODES['INEWAZ'])and o['mode']==(sm<<3)|64|dm and sm in(0,1,2)and dm in(0,1)and types[o['m']][0]==4 and list(types[o['m']][1])==[128]
            source=o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]]
            assert [op,mode,s,m,d,returned]==[OPCODES['INEWAZ'],o['mode'],source,2,maps['frame'if dm else'data'][o['d'][0]],254]
            continue
        if returned==255:
            assert loads
            if op==OPCODES['IINDL']:
                sm=o['mode']>>3&7;dm=o['mode']&7;assert o['op']==OPCODES['IINDW']and o['mode']==(sm<<3)|128|dm and sm in(0,1)and dm in(0,1,2)
                source=maps['frame'if sm else'data'][o['s'][0]];index=o['d'][0]if dm==2 else maps['frame'if dm else'data'][o['d'][0]]
                assert [op,mode,s,m,d,returned]==[OPCODES['IINDL'],o['mode'],source,48,index,255]
                assert r['code'][pc+1][5]==111 or (r['code'][pc+1][0]==41 and r['code'][pc+1][5]==255)or(r['code'][pc+1][0]==OPCODES['IHEADP']and r['code'][pc+1][5]==213 and r['code'][pc+2][5]==142)
                if r['code'][pc+1][5]==213:inside.update((pc+1,pc+2))
            else:
                before=original[pc-1];assert op==o['op']==41 and o['m']is None and mode==o['mode']and before['op']==OPCODES['IINDW']
                sm=mode>>3&7;dm=mode&7;read=sm==5;indirect=o['s']if read else o['d'];direct=o['d']if read else o['s'];vm=dm if read else sm
                assert vm in(0,1)and indirect==[before['m'],0]and (read or dm==5)
                value=maps['frame'if vm else'data'][direct[0]]
                assert [s,m,d]==([48,0,value]if read else[value,0,48])
            continue
        if returned==213:
            assert loads and op==o['op']and op in(OPCODES['ICONSP'],OPCODES['IHEADP'],OPCODES['ITAIL'])and mode==o['mode']and o['m']is None and m==0
            sm=mode>>3&7;dm=mode&7;assert sm in(0,1)and dm in(0,1)
            for address,old,new,kind in((sm,o['s'][0],s,12 if op==OPCODES['ICONSP']else 10),(dm,o['d'][0],d,12 if op==OPCODES['IHEADP']else 10)):
                leaf=next(l for l in r['frame'if address else'data']['leaves']if l[0]==old)
                assert leaf[1]==new and leaf[2:4]==[4,8]and (leaf[4]==kind or (op==OPCODES['ICONSP']and address==sm and old==o['s'][0]and leaf[4]==20) or (op==OPCODES['ICONSP']and address==dm and old==o['d'][0]and leaf[4]==12))
            continue
        if returned==214:
            assert loads and op==o['op']==OPCODES['IMOVP']and mode==o['mode']and o['m']is None
            sm=mode>>3&7;dm=mode&7;read=sm in(4,5);cm=sm if read else dm;vm=dm if read else sm;ca=o['s']if read else o['d'];va=o['d']if read else o['s'];assert cm in(4,5)and vm in(0,1)and ca[1]in(0,20)
            assert m==(24 if ca[1]else 0)
            assert(s if read else d)==maps['frame'if cm==5 else'data'][ca[0]]
            leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==va[0]);assert leaf[1]==(d if read else s)and leaf[2:4]==[4,8]and leaf[4]in((12,20)if ca[1]else(13,20))
            assert next(l for l in r['frame'if cm==5 else'data']['leaves']if l[0]==ca[0])[4]in ROOTS
            continue
        if returned in(208,209,210,211,212,215) or returned==216 and op==OPCODES["IMOVM"]:
            assert loads and op==o['op']==OPCODES['IMOVM']and mode==o['mode']and mode&192==64
            width=16 if returned in(208,210)else 8 if returned in(209,211)else m&255
            assert width in(8,16)and o['m']==width
            sm=mode>>3&7;dm=mode&7
            if returned==212:
                assert sm in(0,1)and dm in(0,1)and m==width
                for address,old,new in((sm,o['s'][0],s),(dm,o['d'][0],d)):
                    leaves=r['frame'if address else'data']['leaves']
                    for i in range(0,width,4):assert next(l for l in leaves if l[0]==old+i)==[old+i,new+i,4,4,9]
            else:
                read=returned in(208,209)or returned in(215,216) and sm in(4,5);cm=sm if read else dm;vm=dm if read else sm;ca=o['s']if read else o['d'];va=o['d']if read else o['s']
                if returned==216:
                    assert cm in(4,5) and vm in(0,1) and ca[1]==16 and width==16 and m==(32<<8)|16
                else:
                    assert cm in(4,5)and vm in(0,1)and ca[1]in(((4,)if width==16 else(0,4,12))if returned==215 else((0,16)if width==16 else(0,8,16,24)))and m==((ca[1]+(4 if returned==215 and not(width==8 and ((ca[1]==4 and m>>8==4)or(ca[1]==0 and m>>8==0)))else 0))<<8)|width
                assert next(l for l in r['frame'if cm==5 else'data']['leaves']if l[0]==ca[0])[4]in ROOTS
                assert(s if read else d)==maps['frame'if cm==5 else'data'][ca[0]]
                leaves=r['frame'if vm else'data']['leaves'];value=d if read else s
                for i in range(0,width,4):assert next(l for l in leaves if l[0]==va[0]+i)==[va[0]+i,value+i,4,4,9]
            continue
        if returned in(248,249,250):
            sm=o['mode']>>3&7;dm=o['mode']&7;cons=returned==248;head=returned==249
            assert loads and op==o['op']==OPCODES['ICONSM'if cons else'IHEADM'if head else'ITAIL']and mode==o['mode']and dm in(0,1)
            if not cons:assert sm in(0,1)
            if returned==250:
                assert o['m']is None and m==0
                assert r['code'][pc]==[op,mode,maps['frame'if sm else'data'][o['s'][0]],0,maps['frame'if dm else'data'][o['d'][0]],250]
            else:
                assert o['m']==m==16 and mode&192==64
                value=o['s']if cons else o['d'];vm=sm if cons else dm;space='frame'if (vm&~4)else'data';base=maps[space][value[0]]
                if vm in(4,5):assert cons and value[1]==4 and next(l for l in r[space]['leaves']if l[0]==value[0])[2:]==[4,8,12]
                else:
                    for i in range(0,16,4):assert next(l for l in r[space]['leaves']if l[0]==value[0]+i)==[value[0]+i,base+i,4,4,9]
                root=maps['frame'if (dm if cons else sm)else'data'][(o['d']if cons else o['s'])[0]]
                assert r['code'][pc]==[op,mode,base if cons else root,16,root if cons else base,returned]
            continue
        if returned in(246,247):
            width=16 if returned==246 else 8;sm=o['mode']>>3&7
            assert loads and r.get('result_geometry')==width and r['result']==[1,width]and op==o['op']==OPCODES['IMOVM']and mode==o['mode']==(sm<<3)|64|5 and sm in(0,1)and o['m']==width and o['d']==[16,0]
            base=maps['frame'if sm else'data'][o['s'][0]]
            for i in range(0,width,4):
                leaf=next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0]+i);assert leaf==[o['s'][0]+i,base+i,4,4,9]
                assert not(r['frame'if sm else'data']['map'][(base+i)//64]&(128>>((base+i)//8%8)))
            assert r['code'][pc]==[op,mode,base,width,32,returned]
            continue
        if returned==242:
            assert loads and op==o['op']and op in(OPCODES['INEW'],OPCODES['INEWZ'])and mode==o['mode']==17 and o['m']is None
            assert types[o['s'][0]]==(16,b'')
            leaf=next(l for l in r['frame']['leaves']if l[0]==o['d'][0]);assert leaf[2:4]==[4,8]and leaf[4]in set(ROOTS)|{20}
            assert r['code'][pc]==[op,mode,0,0,leaf[1],242]
            continue
        if returned in(243,244):
            sm=o['mode']>>3&7;dm=o['mode']&7
            assert loads and op==o['op']==OPCODES['IMOVW'if returned==243 else'ICVTCW']and mode==o['mode']==(sm<<3)|dm and sm in((0,1,2)if returned==243 else(0,1))and dm in(4,5)and o['m']is None and o['d'][1]in(0,4,8,12)
            owner=next(l for l in r['frame'if dm==5 else'data']['leaves']if l[0]==o['d'][0]);assert owner[2:4]==[4,8]and owner[4]in set(ROOTS)|{20}
            if sm==2:value=o['s'][0]
            else:
                leaf=next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0]);assert leaf[2:]==([4,4,9]if returned==243 else[4,8,13]);value=leaf[1]
            assert r['code'][pc]==[op,mode,value,o['d'][1],owner[1],returned]
            continue
        if returned in(228,229):
            sm=o['mode']>>3&7;dm=o['mode']&7;assert loads and sm in(4,5)and len(o['s'])==2 and o['s'][1]in(0,4,8,12)
            leaf=next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0]);assert leaf[2:4]==[4,8]and leaf[4]in set(ROOTS)|{20}
            source=leaf[1];assert r['frame'if sm==5 else'data']['map'][source//64]&(128>>(source//8%8))
            if returned==229:
                assert o['op']in(93,94)and o['m']==0 and o['mode']==(sm<<3)|64|2
                target=o['d'][0]-r['first'];assert 0<=target<len(original)
            else:
                assert o['op']==OPCODES['IMOVW']and o['m']is None and o['mode']==(sm<<3)|dm and dm in(0,1)
                destination=next(l for l in r['frame'if dm else'data']['leaves']if l[0]==o['d'][0]);assert destination[2:]==[4,4,9]
                target=destination[1];assert not(r['frame'if dm else'data']['map'][target//64]&(128>>(target//8%8)))
            assert r['code'][pc]==[o['op'],o['mode'],source,o['s'][1],target,returned]
            continue
        if returned==232:
            sm=o['mode']>>3&7;mm=1 if o['mode']&192==128 else 0 if o['mode']&192==192 else -1
            assert loads and o['op']in(OPCODES['IBEQC'],OPCODES['IBNEC'])and sm in(4,5)and mm in(0,1)and o['s'][1]==20 and o['mode']&7==2
            owner=next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0]);value=next(l for l in r['frame'if mm else'data']['leaves']if l[0]==o['m'])
            assert owner[2:4]==[4,8]and owner[4]in set(ROOTS)|{20}and value[2:]==[4,8,13]
            for space,offset in(('frame'if sm==5 else'data',owner[1]),('frame'if mm else'data',value[1])):assert r[space]['map'][offset//64]&(128>>(offset//8%8))
            assert r['code'][pc]==[o['op'],o['mode'],owner[1],value[1],o['d'][0]-r['first'],232]
            continue
        if returned==234:
            sm=o['mode']>>3&7;dm=o['mode']&7;assert loads and o['op']==OPCODES['IANDW']and sm in(4,5)and dm in(0,1)and o['mode']==(sm<<3)|64|dm and o['s'][1]==16 and -32768<=o['m']<=32767
            owner=next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0]);value=next(l for l in r['frame'if dm else'data']['leaves']if l[0]==o['d'][0])
            assert owner[2:4]==[4,8]and owner[4]in set(ROOTS)|{20}and value[2:]==[4,4,9]
            assert r['frame'if sm==5 else'data']['map'][owner[1]//64]&(128>>(owner[1]//8%8))
            assert not(r['frame'if dm else'data']['map'][value[1]//64]&(128>>(value[1]//8%8)))
            assert r['code'][pc]==[o['op'],o['mode'],owner[1],(32<<16)|(o['m']&65535),value[1],234]
            continue
        if returned==236:
            dm=o['mode']&7;assert loads and o['op']in(OPCODES['IORW'],OPCODES['IANDW'],OPCODES['IXORW'])and dm in(4,5)and o['mode']==16|dm and o['m']is None and o['d'][1]==16 and -32768<=o['s'][0]<=32767
            owner=next(l for l in r['frame'if dm==5 else'data']['leaves']if l[0]==o['d'][0]);assert owner[2:4]==[4,8]and owner[4]in set(ROOTS)|{20}
            assert r['frame'if dm==5 else'data']['map'][owner[1]//64]&(128>>(owner[1]//8%8))
            assert r['code'][pc]==[o['op'],o['mode'],o['s'][0],32,owner[1],236]
            continue
        if returned==237:
            sm=o['mode']>>3&7;assert loads and o['op']==OPCODES['ICVTWC']and sm in(4,5)and o['mode']==(sm<<3)|1 and o['m']is None and o['s'][1]==36
            owner=next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0]);value=next(l for l in r['frame']['leaves']if l[0]==o['d'][0]);assert owner[2:4]==[4,8]and owner[4]in set(ROOTS)|{20}and value[2:4]==[4,8]and value[4]in ROOTS+(20,)
            assert r['code'][pc]==[o['op'],o['mode'],owner[1],72,value[1],237]
            for space,root_at in(('frame'if sm==5 else'data',owner[1]),('frame',value[1])):assert r[space]['map'][root_at//64]&(128>>(root_at//8%8))
            continue
        if returned in(241,251):
            sm=o['mode']>>3&7;field=o['s'][1];assert loads and o['op']in(93,94)and sm in(4,5)and o['mode']==(sm<<3)|192|2 and field in((24,)if returned==251 else(0,4,8,12,32))
            owner=maps['frame'if sm==5 else'data'][o['s'][0]];other=maps['data'][o['m']];assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0])[4]==12
            assert next(l for l in r['data']['leaves']if l[0]==o['m'])[4]==20
            assert r['code'][pc]==[OPCODES['IBEQL']if o['op']==93 else OPCODES['IBNEL'],o['mode'],owner,((48 if returned==251 else(0,8,16,24,64)[(0,4,8,12,32).index(field)])<<16)|other,o['d'][0]-r['first'],returned]
            for space,root_at in(('frame'if sm==5 else'data',owner),('data',other)):assert r[space]['map'][root_at//64]&(128>>(root_at//8%8))
            continue
        if returned==207:
            assert loads and o['op']in(93,94)and op==(OPCODES['IBEQL']if o['op']==93 else OPCODES['IBNEL'])and mode==o['mode']
            sm=mode>>3&7;mm=mode&192;assert sm in(4,5)and mm in(128,192)and mode&7==2
            family=m>>24;offset=(m>>16)&255;other=m&65535
            mappings={1:{0:0,4:8,8:16,12:24,16:32,44:48,48:56},2:{0:0,20:24,40:48,56:72,84:104},3:{0:0,4:8},4:{28:56,48:88}}
            assert family in mappings and mappings[family][o['s'][1]]==offset
            assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0])[4]==12
            assert next(l for l in r['frame'if mm==128 else'data']['leaves']if l[0]==o['m'])[4]==12
            assert s==maps['frame'if sm==5 else'data'][o['s'][0]]and other==maps['frame'if mm==128 else'data'][o['m']]and d==o['d'][0]-r['first']and d not in inside
            continue
        if returned in(205,206):
            assert loads and op==o['op']==OPCODES['IMOVP'if returned==205 else'IMOVW']and o['m']is None and mode==o['mode']
            sm=mode>>3&7;dm=mode&7;read=sm in(4,5);cm=sm if read else dm;vm=dm if read else sm;ca=o['s']if read else o['d'];va=o['d']if read else o['s']
            assert cm in(4,5)and vm in(0,1,2)
            fields=[0,8,16,24,32,40,48,56,64,72,76,80,88];assert ca[1]%4==0 and 0<=ca[1]<=48
            assert next(l for l in r['frame'if cm==5 else'data']['leaves']if l[0]==ca[0])[4]in ROOTS
            assert (ca[1]//4 in(4,9,10,11))==(returned==206)
            if vm!=2:
                leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==va[0]);assert leaf[3]==(8 if returned==205 else 4)and (leaf[4]in ROOTS+(20,))==(returned==205)
            value=va[0]if vm==2 else maps['frame'if vm else'data'][va[0]];owner=maps['frame'if cm==5 else'data'][ca[0]]
            assert r['code'][pc]==[op,mode,owner if read else value,fields[ca[1]//4],value if read else owner,returned]
            continue
        if returned==204:
            assert loads and op==o['op']and op in(OPCODES['ISEND'],OPCODES['IRECV'])and mode==o['mode']and o['m']is None
            sending=op==OPCODES['ISEND'];cm=(mode&7)if sending else(mode>>3&7);vm=(mode>>3&7)if sending else(mode&7)
            ca=o['d']if sending else o['s'];va=o['s']if sending else o['d'];assert cm in(4,5)and vm in(0,1,2)and ca[1]in(0,4,8,12)
            assert next(l for l in r['frame'if cm==5 else'data']['leaves']if l[0]==ca[0])[4]in ROOTS
            contract=24 if ca[1]in(4,8)else 4
            if vm!=2:
                leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==va[0]);assert leaf[3]==(contract&15)and (leaf[4]in ROOTS+(20,))==(contract==24)
            value=va[0]if vm==2 else maps['frame'if vm else'data'][va[0]];owner=maps['frame'if cm==5 else'data'][ca[0]]
            assert r['code'][pc]==[op,mode,value if sending else owner,(2*ca[1]<<8)|contract,owner if sending else value,204]
            continue
        if returned in(135,136):
            assert loads and op==o['op']and mode==o['mode']
            if returned==135:
                assert op==OPCODES['IMOVMP']and mode&192==64 and types[o['m']]==(8,b'\x40')and m==0
                spans=((mode>>3&7,o['s'][0],s),(mode&7,o['d'][0],d))
            else:
                assert op in(OPCODES['ISEND'],OPCODES['IRECV'])and o['m']is None and m==136
                send=op==OPCODES['ISEND'];cm=mode&7 if send else mode>>3&7;vm=mode>>3&7 if send else mode&7
                ca=o['d']if send else o['s'];va=o['s']if send else o['d'];channel=d if send else s;value=s if send else d
                assert cm in(0,1)and maps['frame'if cm else'data'][ca[0]]==channel and next(l for l in r['frame'if cm else'data']['leaves']if l[0]==ca[0])[4]==6
                spans=((vm,va[0],value),)
            for address,old,new in spans:
                assert address in(0,1)
                leaves=r['frame'if address else'data']['leaves'];fields=[next(l for l in leaves if l[0]==old+4*i)for i in range(2)]
                assert fields[0][2:]==[4,4,9]and fields[1][2:4]==[4,8]and fields[1][4]in ROOTS+(20,)and fields[0][1]==new and fields[1][1]==new+8
            continue
        if returned in(201,202):
            write=returned==201
            assert loads and o['op']==op==OPCODES['IMOVMP']and o['mode']==mode and mode&192==64 and o['m']is not None and m==0
            assert types[o['m']]==(16,bytes([0x50 if write else 0x10]))
            for address,old,new in((mode>>3&7,o['s'][0],s),(mode&7,o['d'][0],d)):
                assert address in(0,1)
                leaves=r['frame'if address else'data']['leaves']
                for i in range(4):
                    leaf=next(l for l in leaves if l[0]==old+4*i);root=i==3 or write and i==1
                    assert leaf[2:]==[4,8 if root else 4,6 if i==3 else 3 if write and i==1 else 9]and leaf[1]==new+([0,8,16,24]if write else[0,4,8,16])[i]
            continue
        if returned in(194,196):
            assert loads and o['op']==op==OPCODES['IMOVMP']and o['mode']==mode and mode&192==64 and o['m']is not None and m==0
            count=2 if returned==194 else 3
            assert types[o['m']]==(4*count,bytes([0xc0 if count==2 else 0xe0]))
            for address,old,new in((mode>>3&7,o['s'][0],s),(mode&7,o['d'][0],d)):
                assert address in(0,1)
                leaves=r['frame'if address else'data']['leaves'];fields=[next(l for l in leaves if l[0]==old+4*i)for i in range(count)]
                for i,leaf in enumerate(fields):assert leaf[2:4]==[4,8]and leaf[4]in ROOTS+(20,)and leaf[1]==new+8*i
            continue
        if returned in(91,134,189,190,191,192,193,195,197,198,199,203):
            assert loads and o['m']is None
            sm=o['mode']>>3&7;assert o['mode']&7==1
            destination=maps['frame'][o['d'][0]]
            if returned==189:
                assert o['op']==OPCODES['IMOVW']and sm in(0,1)
                source=maps['frame'if sm else'data'][o['s'][0]]
                assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0])[4]==6
                assert r['code'][pc]==[OPCODES['IMOVL'],o['mode'],source,0,destination,189]
            elif returned==91:
                assert o['op']==OPCODES['IMOVW']and sm in(4,5)and o['s'][1]in(0,4,8,12,32)
                assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0])[4]==12
                assert r['code'][pc]==[OPCODES['IMOVL'],o['mode'],maps['frame'if sm==5 else'data'][o['s'][0]],2*o['s'][1],destination,91]
            elif returned==192:
                assert o['op']==OPCODES['IMOVW']and sm in(4,5)and o['s'][1]==52
                assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0])[4]==12
                assert r['code'][pc]==[OPCODES['IMOVL'],o['mode'],maps['frame'if sm==5 else'data'][o['s'][0]],64,destination,192]
            elif returned==198:
                assert o['op']==OPCODES['IMOVW']and sm in(4,5)and o['s'][1]in(0,4)
                assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0])[4]==12
                assert r['code'][pc]==[OPCODES['IMOVL'],o['mode'],maps['frame'if sm==5 else'data'][o['s'][0]],2*o['s'][1],destination,198]
            elif returned==193:
                assert o['op']==OPCODES['IMOVW']and sm in(4,5)and o['s'][1]in(0,4,8,12,16)
                assert next(l for l in r['frame'if sm==5 else'data']['leaves']if l[0]==o['s'][0])[4]==12
                assert r['code'][pc]==[OPCODES['IMOVL'],o['mode'],maps['frame'if sm==5 else'data'][o['s'][0]],2*o['s'][1],destination,193]
            elif returned==134:
                assert o['op']==OPCODES['ILEA']and sm in(0,1)
                leaves=r['frame'if sm else'data']['leaves'];a=next(l for l in leaves if l[0]==o['s'][0]);b=next(l for l in leaves if l[0]==o['s'][0]+4)
                assert a[2:]==[4,4,9]and b[2:4]==[4,8]and b[4]in ROOTS+(20,)and b[1]==a[1]+8
                assert r['code'][pc]==[OPCODES['ILEA'],o['mode'],a[1],136,destination,134]
            elif returned==190:
                assert o['op']==OPCODES['ILEA']and sm in(0,1)
                leaf=next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0]);contract=24 if leaf[4]in ROOTS else leaf[3]
                if m==8:
                    tail=next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0]+4);assert leaf[2:]==tail[2:]==[4,4,9]and tail[1]==leaf[1]+4;contract=8
                if m==152:
                    ls=r['frame'if sm else'data']['leaves'];base=maps['frame'if sm else'data'][o['s'][0]]
                    for i in range(3):
                        part=next(l for l in ls if l[0]==o['s'][0]+4*i);assert part[1]==base+8*i and part[2:]==[4,4 if i==1 else 8,(13,9,6)[i]]
                    contract=152
                assert r['code'][pc]==[OPCODES['ILEA'],o['mode'],maps['frame'if sm else'data'][o['s'][0]],contract,destination,190]
            elif returned in(199,203):
                assert o['op']==OPCODES['ILEA']and sm in(0,1)
                leaves=r['frame'if sm else'data']['leaves'];first=next(l for l in leaves if l[0]==o['s'][0]);write=returned==199
                for i in range(4):
                    leaf=next(l for l in leaves if l[0]==o['s'][0]+4*i);root=i==3 or write and i==1
                    assert leaf[2:]==[4,8 if root else 4,6 if i==3 else 3 if write and i==1 else 9]and leaf[1]==first[1]+([0,8,16,24]if write else[0,4,8,16])[i]
                assert r['code'][pc]==[OPCODES['ILEA'],o['mode'],first[1],104 if write else 120,destination,returned]
            elif returned in(195,197):
                assert o['op']==OPCODES['ILEA']and sm in(0,1)
                count=2 if returned==195 else 3
                leaves=r['frame'if sm else'data']['leaves'];fields=[next(l for l in leaves if l[0]==o['s'][0]+4*i)for i in range(count)]
                for i,leaf in enumerate(fields):assert leaf[2:4]==[4,8]and leaf[4]in ROOTS+(20,)and leaf[1]==fields[0][1]+8*i
                assert r['code'][pc]==[OPCODES['ILEA'],o['mode'],fields[0][1],48 if count==2 else 72,destination,returned]
            else:
                assert o['op']in(OPCODES['IALT'],OPCODES['INBALT'])and sm==1
                headers=original[pc-2:pc];assert all(h['op']==OPCODES['IMOVW']and h['mode']==17 for h in headers)
                n=sum(h['s'][0]for h in headers)
                assert r['code'][pc]==[o['op'],o['mode'],maps['frame'][o['s'][0]],n,destination,191]
            continue
        if o['op']==OPCODES['IEXIT']:
            assert loads and o['mode']==27 and o['s']is None and o['m']is None and o['d']is None
            assert r['code'][pc]==[OPCODES['IEXIT'],27,0,0,0,0]
            continue
        if o['op']in(OPCODES['ICONSW'],OPCODES['IHEADW']):
            assert loads and op==o['op']and mode==o['mode']and not m and o['m']is None
            sm=mode>>3&7;dm=mode&7;cons=op==OPCODES['ICONSW'];assert sm in((0,1,2)if cons else(0,1))
            source=o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]]
            if returned:assert not cons and returned==4 and o['d']==[16,0]and r['result']==[9,4];destination=32
            else:assert dm in(0,1);destination=maps['frame'if dm==1 else'data'][o['d'][0]]
            assert r['code'][pc]==[op,mode,source,0,destination,returned]
            continue
        if returned in(45,47):
            sm=mode>>3&7;assert loads and r.get('result_aggregate')==(124 if returned==45 else 48) and op==o['op']==OPCODES['IRECV']and mode==o['mode']and o['m']is None and o['d']==[16,8 if returned==45 else 0]and sm in(0,1)
            assert r['code'][pc]==[op,mode,maps['frame'if sm else'data'][o['s'][0]],16,32,returned];assert next(l for l in r['frame'if sm else'data']['leaves']if l[0]==o['s'][0])[4]==6;continue
        if returned in(50,52):
            assert loads and op==o['op']and mode==o['mode']
            if returned==50:
                assert op==OPCODES['IMOVMP']and o['mode']&192==64 and types[o['m']]==(12,b'\xa0')and m==0
                pairs=[(o['s'],s,mode>>3&7),(o['d'],d,mode&7)]
            else:
                assert op in(OPCODES['ISEND'],OPCODES['IRECV'])and o['m']is None and m==152
                sending=op==OPCODES['ISEND'];ca=o['d']if sending else o['s'];cm=mode&7 if sending else mode>>3&7;value=d if sending else s
                assert cm in(0,1)and next(l for l in r['frame'if cm else'data']['leaves']if l[0]==ca[0])[4]==6 and value==maps['frame'if cm else'data'][ca[0]]
                pairs=[(o['s']if sending else o['d'],s if sending else d,mode>>3&7 if sending else mode&7)]
            for old,new,sm in pairs:
                assert sm in(0,1);ls=r['frame'if sm else'data']['leaves']
                for i in range(3):
                    leaf=next(l for l in ls if l[0]==old[0]+4*i);assert leaf[1]==new+8*i and leaf[2:4]==[4,4 if i==1 else 8]and(leaf[4]==9 if i==1 else leaf[4]in ROOTS+(20,))
            continue
        if returned==54:
            assert loads and op==o['op']and op in(OPCODES['ISEND'],OPCODES['IRECV'])and mode==o['mode']and m==8 and o['m']is None
            sending=op==OPCODES['ISEND'];ca=o['d']if sending else o['s'];va=o['s']if sending else o['d'];cm=(mode&7)if sending else(mode>>3&7);vm=(mode>>3&7)if sending else(mode&7)
            assert cm in(0,1)and vm in(0,1)and next(l for l in r['frame'if cm else'data']['leaves']if l[0]==ca[0])[4]==6
            ls=r['frame'if vm else'data']['leaves'];a=next(l for l in ls if l[0]==va[0]);b=next(l for l in ls if l[0]==va[0]+4);assert a[2:]==b[2:]==[4,4,9]and b[1]==a[1]+4
            channel=maps['frame'if cm else'data'][ca[0]];assert r['code'][pc]==[op,mode,a[1]if sending else channel,8,channel if sending else a[1],54];continue
        if returned==55:
            assert loads and o['op']==OPCODES['INEWCM']and op==OPCODES['INEWCL']and o['mode']>>3&7==2 and o['s']==[8]and mode==(o['mode']&~56)|24
            dm=mode&7;mm=mode&192;assert dm in(0,1)and next(l for l in r['frame'if dm else'data']['leaves']if l[0]==o['d'][0])[4]==6
            middle=0 if mm==0 else o['m']if mm==64 else maps['frame'if mm==128 else'data'][o['m']]
            assert r['code'][pc]==[op,mode,0,middle,maps['frame'if dm else'data'][o['d'][0]],55];continue
        if o['op']in(OPCODES['ISEND'],OPCODES['IRECV']):
            assert loads and op==o['op']and returned==188 and o['m']is None and mode==o['mode']
            sending=op==OPCODES['ISEND'];cm=(mode&7)if sending else(mode>>3&7);vm=(mode>>3&7)if sending else(mode&7)
            ca=o['d']if sending else o['s'];va=o['s']if sending else o['d'];assert cm in(0,1)and vm in(0,1,2)
            assert next(l for l in r['frame'if cm else'data']['leaves']if l[0]==ca[0])[4]==6
            width=m&15;assert m in(1,4,8,24)
            if vm!=2:
                leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==va[0]);assert leaf[3]==width and (leaf[4]in ROOTS+(20,))==(m==24)
            value=va[0]if vm==2 else maps['frame'if vm else'data'][va[0]];channel=maps['frame'if cm else'data'][ca[0]]
            assert r['code'][pc]==[op,mode,value if sending else channel,m,channel if sending else value,188]
            continue
        if o['op']==OPCODES['INEWCMP']:
            assert loads and op==o['op']==OPCODES['INEWCMP']and mode==o['mode']and returned in(183,185)
            size,mapping=types[o['s'][0]];n=size//4;assert n in(2,3)and size==4*n and mapping in(bytes([192 if n==2 else 224]),b'\x40',b'\xa0')
            if mapping==b'\x40':assert size==8;n=1
            if mapping==b'\xa0':assert size==12;n=4
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
                leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==direct[0]);assert leaf[2:4]==[4,8]and (leaf[4]==(13 if field[1]==52 else 12)or(not read and vm==0 and leaf[4]==20)or(vm==1 and leaf[4]in ROOTS))
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
                leaf=next(l for l in r['frame'if vm else'data']['leaves']if l[0]==direct[0]);assert leaf[2:4]==[4,8]and (leaf[4]==(6 if index==3 else 12)or(not read and vm==0 and leaf[4]==20)or(vm==1 and leaf[4]in ROOTS))
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
                assert leaf[2:4]==[4,8]
                if leaf[4]!=kind and space==('frame'if bm else'data')and not read:
                    starts=[q for q in range(max(0,pc-3),pc)if r['code'][q][5]==172 and r['code'][q][4]==maps[space][old]]
                    assert len(starts)==1 and all(r['code'][q][5]==96 and r['code'][q][4]==maps[space][old]for q in range(starts[0]+1,pc));inside.update(range(starts[0]+1,pc+1))
                else:assert leaf[4]==kind or(not read and space=='data'and leaf[4]==20)
            continue
        if loads and op==OPCODES['INEWAZ']and o['op']==OPCODES['INEWA']:
            assert m==0 and types[o['m']][0]==1 and not any(types[o['m']][1])
            o={**o,'op':op,'m':0}
        if op==OPCODES['IADDC']and not o['mode']&192:
            assert loads and o['mode']&7==1
            o={**o,'mode':o['mode']|128,'m':o['d'][0]}
        assert op==o['op']and mode==o['mode']
        if mode&7!=5:assert returned==0
        if op==8:
            assert loads and mode&192==64 and 0<=m<64 and returned==0
            for address,offset,kind in ((mode>>3&7,s,13),(mode&7,d,11)):
                assert address in (0,1)
                leaves=r['frame'if address else'data']['leaves']
                leaf=next(leaf for leaf in leaves if leaf[1]==offset);assert leaf[3]==8
                if leaf[4]!=kind:
                    # A reused mapped temporary is a String after this exact writer.
                    assert kind==13 and address==1 and leaf[4]in ROOTS and pc>0
                    writer=original[pc-1];assert writer['op']==OPCODES['IADDC']and writer['mode']&7==1 and writer['d']==o['s']and writer['mode']&192 in(128,192)
                    for operandmode,old in((writer['mode']>>3&7,writer['s'][0]),(1 if writer['mode']&192==128 else 0,writer['m'])):
                        assert operandmode in(0,1)and next(l for l in r['frame'if operandmode else'data']['leaves']if l[0]==old)[4]==13

        if op==OPCODES['ICVTCW']:
            assert loads and mode in(1,9)and o['m']is None and m==returned==0
            space='frame'if mode==9 else'data'
            source=next(l for l in r[space]['leaves']if l[0]==o['s'][0]);destination=next(l for l in r['frame']['leaves']if l[0]==o['d'][0])
            assert source[1]==s and source[2:4]==[4,8]and source[4]in(set(ROOTS)|{20})
            assert destination==[o['d'][0],d,4,4,9]
            assert r[space]['map'][s//64]&(128>>(s//8%8))
            assert not(r['frame']['map'][d//64]&(128>>(d//8%8)))
        if op==RAISE:
            assert loads and mode in(3,11)and m==d==returned==0
            address=mode>>3&7;leaves=r['frame'if address else'data']['leaves']
            if not any(leaf[1]==s and leaf[3:]==[8,13]for leaf in leaves):
                assert address==1 and pc>0 and original[pc-1]['op']==OPCODES['IADDC']and r['code'][pc-1][0]==OPCODES['IADDC']and r['code'][pc-1][1]&7==1 and r['code'][pc-1][4]==s
                assert any(leaf[1]==s and leaf[3]==8 and leaf[4]in set(ROOTS)|{20}for leaf in leaves)and r['frame']['map'][s//64]&(128>>(s//8%8));inside.add(pc)
        if r.get('result_channels'):
            assert r['result_channels']==[6,6,6]and r['result']==[14,24]
            if o['op']==43:
                assert o['mode']==77 and o['d']==[16,0]and types[o['m']]==(12,bytes([224]))
                base=maps['frame'][o['s'][0]]
                assert r['code'][pc]==[43,77,base,0,32,171]
                for k in range(3):assert maps['frame'][o['s'][0]+4*k]==base+8*k
                continue
            if o['op']==41 and o['mode']&7==5:
                assert op==41 and mode==o['mode']and m==0 and o['m']is None
                assert o['d']in([16,0],[16,4],[16,8])and d==32+2*o['d'][1]and returned==168+o['d'][1]//4
                address=mode>>3&7;assert address in(0,1)and o['m']is None
                assert s==maps['frame'if address else'data'][o['s'][0]]
                leaf=next(l for l in r['frame'if address else'data']['leaves']if l[0]==o['s'][0]);assert leaf[4]in(6,20)and leaf[3]==8
                assert r['code'][pc]==[41,o['mode'],s,0,32+2*o['d'][1],168+o['d'][1]//4]
                continue
        if returned in(145,146,147,149,150,151):
            assert loads and op==o['op']and mode==o['mode']and o['d'][0]==16
            mixed=returned>=149;kind=9 if returned<=149 else 4 if returned==150 else 7
            assert (r.get('result_mixed')==[kind,13]if mixed else r.get('result_words')==[9,9,9])
            assert o['d'][1]==(0 if mixed else 4*(returned-145))and d==32+o['d'][1]
            sm=mode>>3&7;assert sm in(0,1,2)
            assert s==(o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]])
            mm=mode&192;assert m==(o['m']if mm==64 else maps['frame'if mm==128 else'data'][o['m']])if mm else m==0
            continue
        if (r.get('result_pair')or r.get('result_mixed')or r['result']==[13,8])and o['op']==OPCODES['ISLICEC']and o['mode']&7==5:
            assert op==o['op']and mode==o['mode']
            assert o['d'][0]==16 and o['d'][1]in(0,4,8)and d==(40 if o['d'][1]else 32)and returned==(56 if o['d'][1]else 40 if r.get('result_pair')else 24)
            sm=mode>>3&7;assert s==(o['s'][0]if sm==2 else maps['frame'if sm else'data'][o['s'][0]])
            mm=mode&192;assert m==(o['m']if mm==64 else maps['frame'if mm==128 else'data'][o['m']])
            continue
        if r.get('result_mixed')and o['op']==OPCODES['IMOVP']and o['mode']&7==5:
            assert op==o['op']and mode==o['mode']and m==0 and o['m']is None
            assert o['d']==[16,4 if r['result_mixed'][0]==9 else 8]and d==40 and returned==56
            assert s==maps['frame'if mode>>3&7 else'data'][o['s'][0]];continue
        if r.get('result_pair'):
            assert r['result_pair']in([11,10],[13,13])and r['result']==[14,16]
            if o['op']==41 and o['mode']&7==5:
                assert op==41 and mode==o['mode']and m==0 and o['m']is None
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
