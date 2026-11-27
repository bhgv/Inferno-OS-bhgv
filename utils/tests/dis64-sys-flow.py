#!/usr/bin/env python3
"""Source and independent CFG oracles for String-rooted Sys Word control flow."""
from pathlib import Path
import runpy
import struct
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string.py'))
RAW=BASE['RAW'];section=BASE['section'];replace=BASE['replace']


def programs():
    return [
      ('oracle','r:=sys->werrstr("oracle");n:=r+1;', ['oracle']),
      ('arithmetic','r:=sys->werrstr("arith"); n:=(r+3)*7; if(n!=21)n=1/r;', ['arith']),
      ('nested','if(sys->werrstr("left")+sys->werrstr("right")!=0)return; r:=sys->werrstr("nested-ok");', ['left','right','nested-ok']),
      ('diamond','r:=sys->werrstr("start"); n:=0; if(r==0){n=7;r=sys->werrstr("yes");}else{n=9;r=sys->werrstr("no");} if(n!=7)n=1/r;', ['start','yes']),
      ('skip','r:=sys->werrstr("start"); if(r!=0)r=sys->werrstr("skip"); r=sys->werrstr("end");', ['start','end']),
      ('loop','n:=0; r:=0; while(n<4){r=sys->werrstr("loop");n++;} if(n!=4)n=1/r;', ['loop']*4),
      ('break-continue','n:=0; r:=0; while(n<6){n++;if(n==2)continue;r=sys->werrstr("tick");if(n==4)break;} if(n!=4)n=1/r;', ['tick']*3),
      ('early','n:=0; if(n==0)return; r:=sys->werrstr("unreached");', []),
      ('late-return','r:=sys->werrstr("return"); if(r==0)return; r=sys->werrstr("unreached");', ['return']),
      ('short-circuit','r:=sys->werrstr("first"); if(r==0 && sys->werrstr("and")==0)r=sys->werrstr("yes"); if(r!=0 && sys->werrstr("skip")==0)r=sys->werrstr("bad");', ['first','and','yes']),
      ('or','r:=sys->werrstr("first"); if(r==0 || sys->werrstr("skip")==0)r=sys->werrstr("or");', ['first','or']),
      ('nil','r:=sys->werrstr(nil); if(r==0)r=sys->werrstr("nil-ok");', ['', 'nil-ok']),
      ('reload','r:=sys->werrstr("before"); n:=r+2; sys=load Sys Sys->PATH; n*=3; r=sys->werrstr("after"); if(n!=6)n=1/r;', ['before','after']),
      ('bits','r:=sys->werrstr("bits"); n:=((r+7)<<3)^5; n=(n&63)|128; n=n>>1; if(n!=94)n=1/r;', ['bits']),
      ('wrap','r:=sys->werrstr("wrap"); n:=r+536870911; n++; n<<=3; if(n!=0)n=1/r;', ['wrap']),
      ('compare','r:=sys->werrstr("compare");n:=r - 7;if(n>=0)n=1/r;if(n > -1)n=1/r;if(n <= -8)n=1/r;if(n < -7)n=1/r;if(n == -7)r=sys->werrstr("compare-ok");', ['compare','compare-ok']),
      ('reload-loop','n:=0;r:=0;while(n<3){sys=load Sys Sys->PATH;r=sys->werrstr("reload-loop");n++;}', ['reload-loop']*3),
      ('error','r:=sys->werrstr("fault"); n:=1/r;', ['fault']),
    ]


def rows(limbo,work):
    result=[]
    for name,body,events in programs():
        image=BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH; '+body)
        if struct.unpack_from('>I',section(image,3),4)[0]!=112:raise RuntimeError('scalar caller did not isolate its String root: '+name)
        if name=='oracle':
            ins=RAW['I'];indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
            code=ins('ILOAD',64,source=16,middle=0,destination=0)+ins('IMOVP',1,source=24,destination=64)
            code+=ins('IMFRAME',65,source=0,middle=0,destination=40)+indirect('IMOVP',64,40,64)+indirect('ILEA',104,40,32)+ins('IMCALL',72,source=40,middle=0,destination=0)
            code+=ins('IADDW',145,source=1,middle=104,destination=80)+RAW['EXIT']
            if section(image,2)!=RAW['W'](8)+code:raise RuntimeError('extended Sys String CODE oracle mismatch')
        result.append((name,image,events,'zero divide' if name=='error' else ''))
    return result


def raw_cases(limbo,work):
    image=BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH; r:=sys->werrstr("raw");n:=r+1;')
    ins=RAW['I'];indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    load=ins('ILOAD',64,source=16,middle=0,destination=0)
    literal=ins('IMOVP',1,source=24,destination=64)
    call=[literal,ins('IMFRAME',65,source=0,middle=0,destination=40),indirect('IMOVP',64,40,64),indirect('ILEA',104,40,32),ins('IMCALL',72,source=40,middle=0,destination=0)]
    def build(code):return replace(image,2,RAW['W'](len(code))+b''.join(code))
    result=[]
    # Both edges are structurally reachable regardless of a constant condition.
    for condition in (0,1):
        for left in (False,True):
            for right in (False,True):
                code=[load,ins('IMOVW',17,source=condition,destination=80),ins('IBEQW',146,source=0,middle=80,destination=5),
                      ins('IMOVW',17,source=7,destination=84 if left else 88),ins('IJMP',26,destination=6),
                      ins('IMOVW',17,source=9,destination=84 if right else 88),ins('IMOVW',9,source=84,destination=88),*call,RAW['EXIT']]
                result.append(('diamond-'+str((condition,left,right)),build(code),left and right))
    good=[load,ins('IMOVW',17,source=0,destination=80),ins('IBEQW',146,source=1,middle=80,destination=3),*call,RAW['EXIT']]
    for target in (0,3,4,5,6,7,8):
        code=list(good);code[2]=ins('IBEQW',146,source=1,middle=80,destination=target)
        result.append(('call-boundary-'+str(target),build(code),target in (3,8)))
    for offset in (64,68,72,76,80,84,88,96,104,108):
        code=[load,ins('IMOVW',9,source=offset,destination=80),*call,RAW['EXIT']]
        result.append(('uninitialized-or-root-'+str(offset),build(code),False))
    for condition in (0,1):
        for preinit in (False,True):
            code=[load,ins('IMOVW',17,source=condition,destination=80),ins('IMOVW',17,source=0,destination=84 if preinit else 88),
                  ins('IBEQW',146,source=0,middle=80,destination=7),ins('IMOVW',17,source=7,destination=84),
                  ins('IMOVW',17,source=0,destination=80),ins('IJMP',26,destination=3),ins('IMOVW',9,source=84,destination=88),*call,RAW['EXIT']]
            result.append(('loop-first-iteration-'+str((condition,preinit)),build(code),preinit))
    for condition in (0,1):
        code=[load,ins('IMOVW',17,source=condition,destination=80),ins('IBEQW',146,source=0,middle=80,destination=9),
              *call,ins('IJMP',26,destination=10),ins('IMOVW',17,source=0,destination=88),ins('IMOVW',9,source=104,destination=84),*call,RAW['EXIT']]
        result.append(('conditional-result-'+str(condition),build(code),False))
    return result


def budget(limbo,work):
    prefix='sys:=load Sys Sys->PATH;n:=0;r:=0;'+('r=sys->werrstr("budget");n+=r;'*29)
    body=prefix+'n++;if(n==1)r=sys->werrstr("last");'
    image=BASE['compile_source'](limbo,work,body)
    decoded=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode'](section(image,2))
    words={'MOVW','ADDW','SUBW','MULW','DIVW','MODW','ANDW','ORW','XORW','SHLW','SHRW','BEQW','BNEW','BLTW','BLEW','BGTW','BGEW','JMP','ILOAD'}
    count=sum(row[0] in words or row[0][1:] in words for row in decoded)
    if count!=64:raise RuntimeError('scalar/LOAD budget oracle drift: '+str(count))
    BASE['compile_source'](limbo,work,prefix+'n++;n++;if(n==1)r=sys->werrstr("last");',accepted=False)
    return image


def compiler_refusals(limbo,work):
    # Flattened String aliases must not invent a value at a conditional join.
    for body in ('sys:=load Sys Sys->PATH; r:=sys->werrstr("start");n:int;if(r==0)n=7;r=sys->werrstr("end");n++;',
                 'sys:=load Sys Sys->PATH; n:=0; s:="left"; if(n==0)s="right"; r:=sys->werrstr(s);',
                 'sys:=load Sys Sys->PATH; n:=0; s:="old"; while(n<2){s="new";n++;} r:=sys->werrstr(s);'):
        BASE['compile_source'](limbo,work,body,accepted=False)


def boot(command,work,consumer,limbo,execute,retired):
    pair=['-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial']
    for name,image,events,status in rows(limbo,work):
        consumer.write_bytes(image)
        for jit in (0,1):
            output=execute(command+['-c',str(jit)]+pair)
            if f'native64 scalar module SysText: jit={jit}, exit="{status}"' not in output or retired not in output or 'panic:' in output:raise RuntimeError(name+' '+output)
    for name,image,accepted in raw_cases(limbo,work):
        consumer.write_bytes(image);output=execute(command+pair)
        if (retired in output)!=accepted or (not accepted and 'panic: loading Dis64 pair' not in output):raise RuntimeError('raw Sys flow '+name+' '+output)
    consumer.write_bytes(budget(limbo,work))
    for jit in (0,1):
        output=execute(command+['-c',str(jit)]+pair)
        if f'native64 scalar module SysText: jit={jit}, exit=""' not in output or retired not in output:raise RuntimeError('scalar budget boot failed: '+output)
    compiler_refusals(limbo,work)
    print('PASS Sys String scalar flow: arithmetic, saved results, diamonds, loops, break/continue, early return, short-circuit and independent CFG refusals',flush=True)
