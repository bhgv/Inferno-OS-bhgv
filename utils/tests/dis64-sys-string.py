#!/usr/bin/env python3
"""Execute serialized fixed String-to-Word Sys calls with actual native handlers and GC."""
from pathlib import Path
import argparse
import json
import os
import runpy
import shlex
import struct
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
LITERAL=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-literal.py'))
RAW=LITERAL['RAW']; section=LITERAL['section']; replace=LITERAL['replace']


def compile_source(limbo,work,body,flag='-64l',accepted=True):
    source=work/'sys-string.b';output=work/'sys-string.dis64'
    source.write_text('implement SysText; include "sys.m"; SysText: module {init: fn();}; init(){'+body+'}')
    result=subprocess.run([str(limbo.resolve()),'-I',str(ROOT/'module'),flag,'-o',str(output),str(source)],capture_output=True,text=True,timeout=30)
    if accepted:
        if result.returncode: raise RuntimeError(body+'\n'+result.stderr)
        return output.read_bytes()
    if result.returncode<=0 or output.exists() or 'Dis64' not in result.stderr: raise RuntimeError('unsupported Sys String source compiled: '+body+'\n'+result.stderr)


def rows(limbo,work):
    result=[]
    for value in ('plain','Grüße 世界','🙂𝄞','a\0b','x'*255,''):
        for alias,reload in ((False,False),(True,True)):
            literal=json.dumps(value,ensure_ascii=False)
            body='sys:=load Sys Sys->PATH; '
            body+=('seed:='+literal+'; alias:=seed; seed=nil; result:=sys->werrstr(alias);' if alias else 'result:=sys->werrstr('+literal+');')
            if reload: body+='sys=load Sys Sys->PATH; result=sys->werrstr('+literal+');'
            image=compile_source(limbo,work,body)
            # Complete CODE oracle independent of producer lowering and runtime checks.
            ins=RAW['I']; calls=2 if reload else 1
            indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
            call=(ins('IMFRAME',65,source=0,middle=0,destination=40)+indirect('IMOVP',64,40,64)+indirect('ILEA',72,40,32)+ins('IMCALL',72,source=40,middle=0,destination=0))
            code=b''
            for i in range(calls):
                code+=ins('ILOAD',64,source=16,middle=0,destination=0)+ins('IMOVP',1,source=24 if value else 8,destination=64)+call
            code+=RAW['EXIT']
            if section(image,2)!=RAW['W'](calls*6+1)+code: raise RuntimeError('Sys String CODE oracle mismatch')
            if section(image,5)!=RAW['W'](1,0,1,16,0xface0010,1,1,0x71): raise RuntimeError('Sys String signature oracle mismatch')
            result.append((image,value))
    for count in (1,32):
        values=[f'{i:02d}:'+'x'*29 for i in range(count)]
        body='sys:=load Sys Sys->PATH; '+''.join(('r:=' if i==0 else 'r=')+'sys->werrstr('+json.dumps(value)+');' for i,value in enumerate(values))
        result.append((compile_source(limbo,work,body),values[-1]))
    return result


def refusals(image):
    signature=section(image,5)
    for word,value in ((1,1),(2,2),(3,20),(4,1),(5,0),(6,0x71),(7,1)):
        bad=bytearray(signature);struct.pack_into('>I',bad,word*4,value);yield replace(image,5,bad)
    code=section(image,2)
    for original,replacement in ((RAW['I']('IMOVP',1,source=24,destination=64),RAW['I']('IMOVP',1,source=16,destination=64)),
                                 (RAW['I']('IMFRAME',65,source=0,middle=0,destination=40),RAW['I']('IMFRAME',65,source=0,middle=1,destination=40)),
                                 (RAW['I']('IMCALL',72,source=40,middle=0,destination=0),RAW['I']('IMCALL',72,source=40,middle=0,destination=8))):
        if original not in code: raise RuntimeError('refusal pattern drift')
        yield replace(image,2,code.replace(original,replacement,1))
    types=section(image,3)
    for index,value in ((13,0xc0),(13,0),(-1,0xe0)):
        bad=bytearray(types);bad[index]=value;yield replace(image,3,bad)
    bad=bytearray(types);struct.pack_into('>I',bad,18,4);yield replace(image,3,bad)
    indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    for op,source,target in (('IMOVP',64,72),('ILEA',72,40)):
        original=indirect(op,source,40,64 if op=='IMOVP' else 32)
        changed=indirect(op,source,40,target)
        yield replace(image,2,code.replace(original,changed,1))
    named=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-module.py'))
    for index,text in ((0,'$sys'),(1,'remove'),(3,'$Other'),(1,'werrstr\0suffix')): yield named['rename'](image,index,text)


def compiler_refusals(limbo,work):
    body='sys:=load Sys Sys->PATH; r:=sys->werrstr("text");'
    for flag in ('-64','-64m'):compile_source(limbo,work,body,flag,False)
    for text in ('sys:=load Sys "$Other"; r:=sys->werrstr("text");',
                 'sys:=load Sys Sys->PATH; r:=sys->werrstr("text"); r=r+1;',
                 'sys:=load Sys Sys->PATH; r:=sys->werrstr("'+('x'*256)+'");',
                 'sys:=load Sys Sys->PATH; '+'r:=sys->werrstr("a");'+'r=sys->werrstr("a");'*32):
        compile_source(limbo,work,text,accepted=False)


def boot(command,work,consumer,limbo,execute,retired):
    images=rows(limbo,work)
    pair=['-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys']
    trial=['-X','dis64-managed-trial']
    for image,_ in images:
        consumer.write_bytes(image)
        for jit in (0,1):
            for seed in ([],['-A','ignored seed']):
                output=execute(command+['-c',str(jit)]+pair+trial+seed)
                if f'native64 scalar module SysText: jit={jit}, exit=""' not in output or retired not in output or 'panic:' in output: raise RuntimeError(output)
    consumer.write_bytes(images[0][0])
    for options in (pair,):
        output=execute(command+options)
        if retired in output or 'panic:' not in output and 'Usage:' not in output:raise RuntimeError('Sys String bypassed explicit admission: '+output)
    rejected=subprocess.run(command+pair[:2]+['-P','$Math']+trial,start_new_session=True,capture_output=True,text=True,timeout=20)
    if rejected.returncode != 1 or 'Usage: emu' not in rejected.stdout+rejected.stderr:raise RuntimeError('wrong builtin option was admitted')
    for image in refusals(images[0][0]):
        consumer.write_bytes(image);output=execute(command+pair+trial)
        if retired in output or 'panic: loading Dis64 pair' not in output:raise RuntimeError('malformed Sys String scheduled: '+output)
    compiler_refusals(limbo,work)
    print('PASS serialized Sys String calls: real builtin LOAD, literals/aliases/nil, 32 roots/calls, C/JIT requests, admission and compiler refusals',flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--limbo',required=True,type=Path);parser.add_argument('--root',required=True,type=Path);parser.add_argument('--emu',required=True,type=Path);parser.add_argument('--runner',default='');args=parser.parse_args()
    base=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
    with tempfile.TemporaryDirectory(prefix='sys-string-',dir=args.root.resolve()) as directory:
        work=Path(directory);boot(shlex.split(args.runner)+[str(args.emu.resolve()),'-v','-r',str(args.root.resolve())],work,work/'consumer.dis64',args.limbo,base['execute'],base['RETIRED'])
