#!/usr/bin/env python3
"""Generated fixed Sys String groups, ordinal oracles and isolated namespace effects."""
from pathlib import Path
import argparse
import itertools
import json
import runpy
import shlex
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string.py'))
RAW=BASE['RAW'];section=BASE['section'];replace=BASE['replace']
METHODS=('werrstr','chdir','remove')


def compile_case(limbo,work,body,order=METHODS,accepted=True):
    interface='Sys: module {PATH: con "$Sys";'+''.join(name+': fn(s: string): int;' for name in order)+'};'
    (work/'group-sys.m').write_text(interface)
    source=work/'sys-group.b';output=work/'sys-group.dis64'
    source.write_text('implement SysGroup; include "group-sys.m"; SysGroup: module {init: fn();}; init(){sys:=load Sys Sys->PATH;'+body+'}')
    result=subprocess.run([str(limbo.resolve()),'-64l','-I',str(work),'-o',str(output),str(source)],capture_output=True,text=True,timeout=30)
    if accepted:
        if result.returncode:raise RuntimeError(body+'\n'+result.stderr)
        return output.read_bytes()
    if result.returncode<=0 or output.exists() or 'Dis64' not in result.stderr:raise RuntimeError('unsupported Sys group compiled: '+result.stderr)


def programs():
    cases=[]
    for order in itertools.permutations(METHODS):
        events=[(name,{'werrstr':'mark','chdir':'/ok','remove':'/missing'}[name],0 if name!='remove' else -1) for name in order[::-1]]
        body='bad:=0;r:=0;'+''.join('r=sys->'+name+'('+json.dumps(arg)+');if(r!='+str(status)+')r=1/bad;' for name,arg,status in events)
        cases.append(('order-'+'-'.join(order),body,events,order))
    for name in ('chdir','remove'):
        events=[(name,'/missing',-1),(name,'/ok' if name=='chdir' else '/victim',0),(name,'/missing' if name=='chdir' else '/victim',-1)]
        body='bad:=0;r:=0;'+''.join('r=sys->'+method+'('+json.dumps(arg)+');if(r!='+str(status)+')r=1/bad;' for method,arg,status in events)
        cases.append(('single-'+name,body,events,(name,)))
    events=[('chdir','/missing',-1),('werrstr','saved',0)]
    cases.append(('saved-status','a:=sys->chdir("/missing");b:=sys->werrstr("saved");if(a != -1)b=1/b;',events,('chdir','werrstr')))
    events=[('chdir','/ok',0),('remove','/missing',-1)]*2+[('werrstr','loop-done',0)]
    cases.append(('loop','n:=0;r:=0;while(n<2){r=sys->chdir("/ok");r=sys->remove("/missing");n++;}r=sys->werrstr("loop-done");',events,METHODS))
    events=[('remove','/victim',0),('chdir','/ok',0),('remove','/victim',-1),('werrstr','done',0)]
    cases.append(('reload','r:=sys->remove("/victim");sys=load Sys Sys->PATH;r=sys->chdir("/ok");r=sys->remove("/victim");r=sys->werrstr("done");',events,METHODS))
    # Flat 80-byte frame with all three ordinals, independent of scalar lowering.
    events=[('remove','/missing',-1),('werrstr','flat',0),('chdir','/ok',0)]
    cases.append(('flat','r:=sys->remove("/missing");r=sys->werrstr("flat");r=sys->chdir("/ok");',events,METHODS))
    events=[('chdir','',-1),('remove','',-1),('werrstr','nil-ok',0)]
    cases.append(('nil','bad:=0;r:=sys->chdir(nil);if(r != -1)r=1/bad;r=sys->remove(nil);if(r != -1)r=1/bad;r=sys->werrstr("nil-ok");',events,METHODS))
    return cases


def imports(image):
    pool=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-multiliteral.py'))['pool_records'](image,section)
    payload=section(image,5);count=struct.unpack_from('>I',payload)[0];names=[]
    if len(payload)!=4+28*count:raise RuntimeError('group import extent mismatch')
    for i in range(count):
        record=payload[4+28*i:4+28*(i+1)];module,symbol,length=struct.unpack_from('>III',record)
        name=''.join(map(chr,pool[symbol]))
        if module!=0 or symbol!=(3+i if i else 1) or length!=16 or record[12:]!=RAW['W'](0xface0010,1,1,0x71) or name not in METHODS:raise RuntimeError('group structural import oracle mismatch')
        names.append(name)
    if len(set(names))!=count:raise RuntimeError('duplicate group import')
    return names,pool


def permute_imports(image,order):
    names,pool=imports(image);order=tuple(name for name in order if name in names)
    if set(order)!=set(names):raise RuntimeError('incomplete ordinal permutation')
    decode=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode']
    code=section(image,2);records=decode(code);body=b''
    for name,mode,source,middle,dest,secondary,raw in records:
        if name in ('IMFRAME','IMCALL'):raw=RAW['I'](name,mode,source=source,middle=order.index(names[middle]),destination=dest)
        body+=raw
    image=replace(image,2,RAW['W'](len(records))+body)
    for i,name in enumerate(order):pool[3+i if i else 1]=tuple(map(ord,name))
    payload=RAW['W'](len(pool))+b''.join(RAW['W'](len(value),*value) for value in pool)
    return replace(image,7,payload)


def rows(limbo,work):
    rows=[];orders=set()
    for name,body,events,order in programs():
        image=permute_imports(compile_case(limbo,work,body,order),order);names,pool=imports(image)
        if set(names)!={event[0] for event in events}:raise RuntimeError('group imports disagree with source')
        if len(names)==3:orders.add(tuple(names))
        if name=='flat':
            indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect'];ins=RAW['I']
            roots={tuple(value):i for i,value in enumerate(pool[3+len(names):])}
            code=ins('ILOAD',64,source=16,middle=0,destination=0)
            for method,arg,_ in events:
                ordinal=names.index(method);root=roots[tuple(map(ord,arg))]
                code+=ins('IMOVP',1,source=24+8*root,destination=64)+ins('IMFRAME',65,source=0,middle=ordinal,destination=40)
                code+=indirect('IMOVP',64,40,64)+indirect('ILEA',72,40,32)+ins('IMCALL',72,source=40,middle=ordinal,destination=0)
            if section(image,2)!=RAW['W'](17)+code+RAW['EXIT']:raise RuntimeError('flat three-import CODE oracle mismatch')
        rows.append((name,image,events))
    if len(orders)!=6:raise RuntimeError('all six independently permuted import ordinal orders were not exercised: '+repr(orders))
    return rows


def refusals(image):
    payload=section(image,5)
    if struct.unpack_from('>I',payload)[0]!=3:raise RuntimeError('three imports required')
    for i in range(3):
        for offset,value in ((0,1),(4,2),(8,20),(20,0x71),(24,1)):
            bad=bytearray(payload);struct.pack_into('>I',bad,4+28*i+offset,value);yield replace(image,5,bad)
    bad=bytearray(payload);struct.pack_into('>I',bad,0,4);yield replace(image,5,bad)
    named=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-module.py'))
    for index in (1,4,5):
        for text in ('open','chdir\0suffix'):yield named['rename'](image,index,text)
    # Alias a second import to an already admitted method: equal type does not make duplicates valid.
    names,_=imports(image)
    yield named['rename'](image,4,names[0])
    code=section(image,2);decode=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode']
    records=decode(code)
    for operation in ('IMFRAME','IMCALL'):
        at=4
        for name,mode,source,middle,dest,secondary,raw in records:
            if name==operation:
                changed=RAW['I'](name,mode,source=source,middle=3,destination=dest)
                yield replace(image,2,code[:at]+changed+code[at+len(raw):])
                changed=RAW['I'](name,mode,source=source,middle=(middle+1)%3,destination=dest)
                yield replace(image,2,code[:at]+changed+code[at+len(raw):]);break
            at+=len(raw)
    # A well-framed third import cannot remain unused by the admitted program.
    body=b''
    for name,mode,source,middle,dest,secondary,raw in records:
        if name in ('IMFRAME','IMCALL') and middle==2:raw=RAW['I'](name,mode,source=source,middle=1,destination=dest)
        body+=raw
    yield replace(image,2,RAW['W'](len(records))+body)


def boot(command,work,consumer,limbo,execute,retired):
    # All effects are confined to this freshly created directory below emu -r.
    path='/'+str(work.relative_to(work.parent));(work/'ok').mkdir(exist_ok=True)
    for name,body,events,order in programs():
        actual=body.replace('"/ok"',json.dumps(path+'/ok')).replace('"/missing"',json.dumps(path+'/missing')).replace('"/victim"',json.dumps(path+'/victim'))
        image=permute_imports(compile_case(limbo,work,actual,order),order);consumer.write_bytes(image)
        for jit in (0,1):
            (work/'victim').write_text('isolated removal fixture')
            output=execute(command+['-c',str(jit),'-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
            if f'native64 scalar module SysGroup: jit={jit}, exit=""' not in output or retired not in output or 'panic:' in output:raise RuntimeError(name+' '+output)
            removed=any(method=='remove' and arg=='/victim' and status==0 for method,arg,status in events)
            if (work/'victim').exists()==removed:raise RuntimeError('actual remove side effect mismatch: '+name)
    for bad in refusals(rows(limbo,work)[0][1]):
        consumer.write_bytes(bad);output=execute(command+['-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
        if retired in output or 'panic: loading Dis64 pair' not in output:raise RuntimeError('malformed group scheduled: '+output)
    print('PASS fixed Sys String groups: all six ordinal orders, real chdir/remove statuses and isolated file effects, flat/scalar/looped calls and malformed group refusals',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);parser.add_argument('--root',required=True,type=Path);parser.add_argument('--emu',required=True,type=Path);parser.add_argument('--runner',default='');args=parser.parse_args()
    base=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
    with tempfile.TemporaryDirectory(prefix='sys-groups-',dir=args.root.resolve()) as td:
        work=Path(td);boot(shlex.split(args.runner)+[str(args.emu.resolve()),'-v','-r',str(args.root.resolve())],work,work/'consumer.dis64',args.limbo,base['execute'],base['RETIRED'])
