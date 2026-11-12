#!/usr/bin/env python3
"""Mixed fixed Sys signatures: independent records/ordinals and actual console boot."""
from pathlib import Path
import argparse
import json
import runpy
import shlex
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string.py'))
RAW=BASE['RAW'];section=BASE['section'];replace=BASE['replace']
METHODS=('werrstr','chdir','remove','millisec','sleep','dup','unmount','bind','fildes','iounit')
ARGS={'werrstr':(0x71,),'chdir':(0x71,),'remove':(0x71,),'millisec':(),'sleep':(1,),'dup':(1,1),'unmount':(0x71,0x71),'bind':(0x71,0x71,1),'fildes':(1,),'iounit':(0x73,0x74,1,1)}


def imports(image):
    pool=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-multiliteral.py'))['pool_records'](image,section)
    payload=section(image,5);at=4;names=[];signatures=[]
    for i in range(struct.unpack_from('>I',payload)[0]):
        owner,symbol,length=struct.unpack_from('>III',payload,at);at+=12
        name=''.join(map(chr,pool[symbol]));signature=payload[at:at+length];at+=length
        if name not in ARGS or owner!=0 or symbol!=(3+i if i else 1) or signature!=RAW['W'](0xface0010,1 if name=='iounit' else len(ARGS[name]),*((0x73,0x74,1,1) if name=='fildes' else (1,)),*ARGS[name]):raise RuntimeError('mixed signature oracle mismatch')
        names.append(name);signatures.append(signature)
    if at!=len(payload) or len(names)!=len(set(names)):raise RuntimeError('mixed import framing/identity mismatch')
    return names,signatures,pool


def permute(image,order):
    names,sigs,pool=imports(image);order=tuple(name for name in order if name in names)
    if set(order)!=set(names):raise RuntimeError('incomplete fixed signature permutation')
    decode=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode'];records=decode(section(image,2));code=b''
    for name,mode,source,middle,dest,secondary,raw in records:
        if name in ('IMFRAME','IMCALL'):raw=RAW['I'](name,mode,source=source,middle=order.index(names[middle]),destination=dest)
        code+=raw
    image=replace(image,2,RAW['W'](len(records))+code)
    records=[]
    for i,name in enumerate(order):
        signature=sigs[names.index(name)];records.append(RAW['W'](0,3+i if i else 1,len(signature))+signature);pool[3+i if i else 1]=tuple(map(ord,name))
    image=replace(image,5,RAW['W'](len(records))+b''.join(records))
    return replace(image,7,RAW['W'](len(pool))+b''.join(RAW['W'](len(value),*value) for value in pool))


# Events are source-level expectations for real handlers with a deterministic OS boundary.
def programs():
    allbody='t:=sys->millisec();r:=sys->werrstr("start");r=sys->sleep(t-t);r=sys->dup(-1,-1);r=sys->chdir("/ok");r=sys->remove("/missing");r=sys->werrstr("end");'
    allevents=[('millisec',None,0,0,23),('werrstr','start',0,0,0),('sleep',None,0,0,0),('dup',None,-1,-1,-1),('chdir','/ok',0,0,0),('remove','/missing',0,0,-1),('werrstr','end',0,0,0)]
    cases=[]
    for shift in range(6):cases.append(('six-'+str(shift),allbody,allevents,METHODS[shift:]+METHODS[:shift]))
    cases.append(('six-reverse',allbody,allevents,METHODS[::-1]))
    cases += [
      ('clock','r:=sys->millisec();',[('millisec',None,0,0,23)],METHODS),
      ('numeric','t:=sys->millisec();r:=sys->sleep(0);r=sys->dup(-1,-1);',[('millisec',None,0,0,23),('sleep',None,0,0,0),('dup',None,-1,-1,-1)],METHODS),
      ('sleep','r:=sys->sleep(0);r=sys->sleep(-1);r=sys->sleep(1);',[('sleep',None,0,0,0),('sleep',None,-1,0,0),('sleep',None,1,0,0)],METHODS),
      ('dup','r:=sys->dup(-1,-1);r=sys->dup(-2,-3);',[('dup',None,-1,-1,-1),('dup',None,-2,-3,-1)],METHODS),
      ('registers','r:=sys->werrstr("zero");a:=r+7;b:=a+2;r=sys->dup(a,b);',[('werrstr','zero',0,0,0),('dup',None,7,9,9)],METHODS),
      ('saved','n:=sys->dup(-1,-1);r:=sys->werrstr("held");r=sys->sleep(n);',[('dup',None,-1,-1,-1),('werrstr','held',0,0,0),('sleep',None,-1,0,0)],METHODS),
      ('nested','r:=sys->sleep(sys->werrstr("nested"));r=sys->werrstr("end");',[('werrstr','nested',0,0,0),('sleep',None,0,0,0),('werrstr','end',0,0,0)],METHODS),
      ('loop','n:=0;r:=0;while(n<3){r=sys->werrstr("tick");r=sys->sleep(r);n++;}t:=sys->millisec();r=sys->dup(-1,-1);',[('werrstr','tick',0,0,0),('sleep',None,0,0,0)]*3+[('millisec',None,0,0,23),('dup',None,-1,-1,-1)],METHODS),
      ('branch','r:=sys->dup(-1,-1);if(r<0){r=sys->werrstr("bad-fd");r=sys->sleep(0);}else r=sys->werrstr("unexpected");',[('dup',None,-1,-1,-1),('werrstr','bad-fd',0,0,0),('sleep',None,0,0,0)],METHODS),
      ('reload','r:=sys->werrstr("before");sys=load Sys Sys->PATH;r=sys->sleep(r);r=sys->remove("/missing");sys=load Sys Sys->PATH;r=sys->dup(r,-1);r=sys->chdir("/ok");r=sys->werrstr("after");',[('werrstr','before',0,0,0),('sleep',None,0,0,0),('remove','/missing',0,0,-1),('dup',None,-1,-1,-1),('chdir','/ok',0,0,0),('werrstr','after',0,0,0)],METHODS),
      ('bits','r:=sys->werrstr("bits");n:=r+536870911;n++;n<<=2;r=sys->dup(n,n);',[('werrstr','bits',0,0,0),('dup',None,-2147483648,-2147483648,-1)],METHODS),
    ]
    pairbody=allbody+'r=sys->unmount("/left","/right");r=sys->bind("/source","/target",0);r=sys->werrstr("after-pair");'
    pairevents=allevents+[('unmount','/left',0,0,-1,'/right'),('bind','/source',0,0,-1,'/target'),('werrstr','after-pair',0,0,0)]
    for shift in range(8):cases.append(('pair-order-'+str(shift),pairbody,pairevents,METHODS[shift:]+METHODS[:shift]))
    cases += [
      ('pair-utf-bind','r:=sys->bind("исходник","цель",0);',[('bind','исходник',0,0,-1,'цель')],METHODS),
      ('pair-utf','r:=sys->unmount("лево","право");r=sys->bind("исходник","цель",0);',[('unmount','лево',0,0,-1,'право'),('bind','исходник',0,0,-1,'цель')],METHODS),
      ('pair-nil','r:=sys->unmount(nil,nil);r=sys->bind(nil,"/target",0);',[('unmount','',0,0,-1,''),('bind','',0,0,-1,'/target')],METHODS),
      ('pair-alias','a:="/left";b:=a;a=nil;r:=sys->unmount(b,"/right");',[('unmount','/left',0,0,-1,'/right')],METHODS),
      ('pair-flags','r:=sys->dup(-1,-1);r=sys->bind("/source","/target",r);',[('dup',None,-1,-1,-1),('bind','/source',-1,0,-1,'/target')],METHODS),
      ('pair-loop','n:=0;r:=0;while(n<2){r=sys->unmount("/left","/right");r=sys->bind("/source","/target",n);n++;}',[('unmount','/left',0,0,-1,'/right'),('bind','/source',0,0,-1,'/target'),('unmount','/left',0,0,-1,'/right'),('bind','/source',1,0,-1,'/target')],METHODS),
      ('pair-reload','r:=sys->unmount("/left","/right");sys=load Sys Sys->PATH;r=sys->bind("/source","/target",0);',[('unmount','/left',0,0,-1,'/right'),('bind','/source',0,0,-1,'/target')],METHODS),
    ]
    fd_body='fd:=sys->fildes(1);r:=sys->iounit(fd);r=sys->werrstr("fd held");fd=sys->fildes(2);r=sys->iounit(fd);fd=sys->fildes(-1);r=sys->iounit(fd);'
    fd_events=[('fildes',None,1,0,101),('iounit',None,101,0,4096),('werrstr','fd held',0,0,0),('fildes',None,2,0,102),('iounit',None,102,0,4096),('fildes',None,-1,0,-1),('iounit',None,-1,0,-1)]
    fd_prefix=allbody+'r=sys->unmount("/left","/right");r=sys->bind("/source","/target",0);'
    prefix_events=allevents+[('unmount','/left',0,0,-1,'/right'),('bind','/source',0,0,-1,'/target')]
    for shift in range(10):cases.append(('fd-order-'+str(shift),fd_prefix+fd_body.replace('r:=sys->iounit','r=sys->iounit'),prefix_events+fd_events,METHODS[shift:]+METHODS[:shift]))
    cases += [
      ('fd-loop','fd:=sys->fildes(1);n:=0;while(n<3){fd=sys->fildes(2);n++;}r:=sys->iounit(fd);',[('fildes',None,1,0,101)]+[('fildes',None,2,0,102)]*3+[('iounit',None,102,0,4096)],METHODS),
      ('fd-loop-zero','fd:=sys->fildes(1);n:=0;while(n<0){fd=sys->fildes(2);n++;}r:=sys->iounit(fd);',[('fildes',None,1,0,101),('iounit',None,101,0,4096)],METHODS),
      ('fd-branch','fd:=sys->fildes(1);r:=sys->werrstr("branch");if(r==0)fd=sys->fildes(2);r=sys->iounit(fd);',[('fildes',None,1,0,101),('werrstr','branch',0,0,0),('fildes',None,2,0,102),('iounit',None,102,0,4096)],METHODS),
      ('fd-branch-false','fd:=sys->fildes(1);r:=sys->werrstr("branch");if(r==1)fd=sys->fildes(2);r=sys->iounit(fd);',[('fildes',None,1,0,101),('werrstr','branch',0,0,0),('iounit',None,101,0,4096)],METHODS),
      ('fd-saved-word','n:=sys->dup(-1,-1);fd:=sys->fildes(1);r:=sys->sleep(n);r=sys->iounit(fd);',[('dup',None,-1,-1,-1),('fildes',None,1,0,101),('sleep',None,-1,0,0),('iounit',None,101,0,4096)],METHODS),
      ('fd-nil','r:=sys->iounit(nil);',[('iounit',None,-1,0,-1)],METHODS),
      ('fd-alias','fd:=sys->fildes(1);saved:=fd;fd=nil;r:=sys->iounit(saved);',[('fildes',None,1,0,101),('iounit',None,101,0,4096)],METHODS),
      ('fd-scalar','r:=sys->werrstr("first");n:=r+1;fd:=sys->fildes(n);r=sys->iounit(fd);r=sys->sleep(r-r);',[('werrstr','first',0,0,0),('fildes',None,1,0,101),('iounit',None,101,0,4096),('sleep',None,0,0,0)],METHODS),
      ('fd-reload','fd:=sys->fildes(1);sys=load Sys Sys->PATH;r:=sys->iounit(fd);',[('fildes',None,1,0,101),('iounit',None,101,0,4096)],METHODS),
      ('fd-root-drop','fd:=sys->fildes(1);fd=nil;r:=sys->iounit(nil);',[('fildes',None,1,0,101),('iounit',None,-1,0,-1)],METHODS),
    ]
    return cases


def rows(limbo,work):
    result=[]
    for name,body,events,order in programs():
        image=permute(BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;'+body),order)
        if name=='clock':
            ins=RAW['I'];indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
            code=ins('ILOAD',64,source=16,middle=0,destination=0)+ins('IMFRAME',65,source=0,middle=0,destination=40)+indirect('ILEA',104,40,32)+ins('IMCALL',72,source=40,middle=0,destination=0)+RAW['EXIT']
            if section(image,2)!=RAW['W'](5)+code:raise RuntimeError('zero-argument mixed CODE oracle mismatch')
        result.append((name,image,events))
    return result


def refusals(limbo,work):
    image=BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;r:=sys->dup(0,0);')
    ins=RAW['I'];indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    code=[ins('ILOAD',64,source=16,middle=0,destination=0),ins('IMFRAME',65,source=0,middle=0,destination=40),indirect('IMOVW',0,40,64,2),indirect('IMOVW',104,40,68),indirect('ILEA',104,40,32),ins('IMCALL',72,source=40,middle=0,destination=0),RAW['EXIT']]
    # An immediate first argument cannot define the uninitialized return cell read by the second.
    yield replace(image,2,RAW['W'](len(code))+b''.join(code))
    for offset in (64,68,72,76,80,84,88,96,108):
        bad=list(code);bad[3]=indirect('IMOVW',offset,40,68);yield replace(image,2,RAW['W'](len(bad))+b''.join(bad))
    for badarg in (64,72):
        bad=list(code);bad[3]=indirect('IMOVW',0,40,badarg,2);yield replace(image,2,RAW['W'](len(bad))+b''.join(bad))
    signature=section(image,5)
    for offset,value in ((12,1),(16,1),(20,0x71),(24,0x71),(28,0x71),(32,0x71)):
        bad=bytearray(signature);struct.pack_into('>I',bad,offset,value);yield replace(image,5,bad)


    pair=BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;r:=sys->bind("left","right",0);')
    decode=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode']
    records=decode(section(pair,2));raw=[record[-1] for record in records]
    # Caller map, root publication, callee offsets, and scalar facts are separate checks.
    types=bytearray(section(pair,3));types[13]=0x80;yield replace(pair,3,types)
    for index in (1,2,4,5):
        bad=list(raw);bad[index]=ins('IMOVW',66,source=0,destination=80);yield replace(pair,2,RAW['W'](len(bad))+b''.join(bad))
    for offset in (64,68,72,76,80,84,88,96,108):
        bad=list(raw);bad[6]=indirect('IMOVW',offset,40,80);yield replace(pair,2,RAW['W'](len(bad))+b''.join(bad))
    bad=list(raw);bad[6]=indirect('IMOVW',104,40,80);yield replace(pair,2,RAW['W'](len(bad))+b''.join(bad))
    for source,dest in ((64,72),(72,64),(80,72)):
        bad=list(raw);bad[5]=indirect('IMOVP',source,40,dest);yield replace(pair,2,RAW['W'](len(bad))+b''.join(bad))
    signature=section(pair,5)
    for offset in (28,32):
        bad=bytearray(signature);struct.pack_into('>I',bad,offset,1);yield replace(pair,5,bad)


    fd=BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;fd:=sys->fildes(1);r:=sys->iounit(fd);')
    raw=[r[-1] for r in decode(section(fd,2))]
    expected=[ins('ILOAD',64,source=16,middle=0,destination=0),ins('IMFRAME',65,source=0,middle=0,destination=40),indirect('IMOVW',1,40,64,2),indirect('ILEA',96,40,32),ins('IMCALL',72,source=40,middle=0,destination=0),ins('IMFRAME',65,source=0,middle=1,destination=40),indirect('IMOVP',96,40,64),indirect('ILEA',104,40,32),ins('IMCALL',72,source=40,middle=1,destination=0),RAW['EXIT']]
    if section(fd,2)!=RAW['W'](10)+b''.join(expected) or section(fd,3)[:14]!=RAW['W'](2,112,8)+bytes((0,0x88)):raise RuntimeError('FD whole CODE/root-map oracle mismatch')
    bad=bytearray(section(fd,3));bad[13]=0x80;yield replace(fd,3,bad)
    bad=list(raw);bad[3]=indirect('ILEA',104,40,32);yield replace(fd,2,RAW['W'](len(bad))+b''.join(bad))
    for offset in (64,72,80,88,104):
        bad=list(raw);bad[6]=indirect('IMOVP',offset,40,64);yield replace(fd,2,RAW['W'](len(bad))+b''.join(bad))
    bad=list(raw);bad.insert(1,ins('IBEQW',82,source=0,middle=0,destination=6));yield replace(fd,2,RAW['W'](len(bad))+b''.join(bad))
    signature=section(fd,5)
    for offset in (24,28,32,36):
        bad=bytearray(signature);struct.pack_into('>I',bad,offset,2);yield replace(fd,5,bad)
    image=BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;fd:=sys->fildes(1);r:=sys->sleep(0);')
    bad=[r[-1] for r in decode(section(image,2))];bad[6]=indirect('IMOVW',104,40,64)
    yield replace(image,2,RAW['W'](len(bad))+b''.join(bad))
    BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;a:=sys->fildes(1);b:=sys->fildes(2);r:=sys->iounit(a);',accepted=False)


def boot(command,work,consumer,limbo,execute,retired):
    path='/'+str(work.relative_to(work.parent));(work/'ok').mkdir(exist_ok=True)
    for name,body,events,order in programs():
        body=body.replace('"/ok"',json.dumps(path+'/ok')).replace('"/missing"',json.dumps(path+'/missing'))
        image=permute(BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;'+body),order);consumer.write_bytes(image)
        for jit in (0,1):
            output=execute(command+['-c',str(jit),'-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
            nilfd=any(e[0]=='iounit' and e[2]<0 for e in events)
            marker='native64 scalar module $Sys: jit=0, exit="fd out of range or not open"' if nilfd else f'native64 scalar module SysText: jit={jit}, exit=""'
            if marker not in output or retired not in output or 'panic:' in output:raise RuntimeError(name+' '+output)
    for bad in refusals(limbo,work):
        consumer.write_bytes(bad);output=execute(command+['-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
        if retired in output or 'panic: loading Dis64 pair' not in output:raise RuntimeError('malformed fixed argument program scheduled: '+output)
    print('PASS mixed fixed Sys: ten signatures, 0/1/2/3 arguments, String/Word roots, nested/looped/reloaded calls, full CODE and argument-initialization refusals',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);parser.add_argument('--root',required=True,type=Path);parser.add_argument('--emu',required=True,type=Path);parser.add_argument('--runner',default='');args=parser.parse_args()
    base=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
    with tempfile.TemporaryDirectory(prefix='sys-fixed-',dir=args.root.resolve()) as td:
        work=Path(td);boot(shlex.split(args.runner)+[str(args.emu.resolve()),'-v','-r',str(args.root.resolve())],work,work/'consumer.dis64',args.limbo,base['execute'],base['RETIRED'])
