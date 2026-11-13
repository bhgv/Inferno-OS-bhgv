#!/usr/bin/env python3
"""Complete private Sys dial images; tuple-result admission and owning runtime inputs."""
from pathlib import Path
import json,runpy,struct
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string.py'))
RAW=BASE['RAW'];section=BASE['section'];replace=BASE['replace']


def rows(limbo,work):
    result=[]
    for values in ([('one','local')],[('fail','')],[('','')],[('Grüße 世界','местный')],
                   [('one',''),('two','local')],[('one','local'),('fail','')],[('fail',''),('two','')],
                   [('one','')]*32):
        body='sys:=load Sys Sys->PATH;'
        for i,(address,local) in enumerate(values):
            body+='(status,c)'+(':=' if i==0 else '=')+'sys->dial('+json.dumps(address,ensure_ascii=False)+','+(json.dumps(local,ensure_ascii=False) if local else 'nil')+');'
        image=BASE['compile_source'](limbo,work,body)
        result.append((image,values))
    return result


def refusals(image):
    decode=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode']
    ins=RAW['I'];records=decode(section(image,2));raw=[r[-1] for r in records]
    lea=next(i for i,r in enumerate(records) if r[0]=='ILEA')
    for offset in (64,72,84,88,96,104,112):
        changed=list(raw);changed[lea]=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']('ILEA',offset,40,32)
        yield replace(image,2,RAW['W'](len(changed))+b''.join(changed))
    for offset in (64,72,80,88,96,104):
        changed=list(raw);changed[1]=ins('IMOVP',9,source=offset,destination=64)
        yield replace(image,2,RAW['W'](len(changed))+b''.join(changed))
    for i in range(1,8):
        changed=bytearray(section(image,3));changed[13]^=1<<(i-1);yield replace(image,3,changed)
    sig=section(image,5)
    for at in range(16,len(sig),4):
        changed=bytearray(sig);struct.pack_into('>I',changed,at,0);yield replace(image,5,changed)
    for i in range(1,len(raw)-1):
        yield replace(image,2,RAW['W'](len(raw)-1)+b''.join(raw[:i]+raw[i+1:]))


def compiler_refusals(limbo,work):
    for suffix in ('c.dfd=nil;','sys=load Sys Sys->PATH;','(other,d):=sys->dial("two",nil);'):
        body='sys:=load Sys Sys->PATH;(status,c):=sys->dial("one",nil);'+suffix
        import subprocess
        source=work/'ordinary-dial.b';source.write_text('implement SysText;include "sys.m";SysText: module {init: fn();};init(){'+body+'}')
        subprocess.run([str(limbo.resolve()),'-I',str(ROOT/'module'),'-o',str(work/'ordinary-dial.dis'),str(source)],check=True,capture_output=True)
        BASE['compile_source'](limbo,work,body,accepted=False)


def boot(command,work,consumer,limbo,execute,retired):
    import socket,threading
    compiler_refusals(limbo,work)
    for calls in (1,2,32):
        for jit in (0,1):
            accepted=[];errors=[]
            with socket.socket() as listener:
                listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15)
                port=listener.getsockname()[1]
                def receive():
                    try:
                        for _ in range(calls):
                            connection,_=listener.accept()
                            with connection:
                                connection.settimeout(15);accepted.append(connection.recv(1))
                    except Exception as error:errors.append(str(error))
                thread=threading.Thread(target=receive);thread.start()
                body='sys:=load Sys Sys->PATH;'
                for i in range(calls):body+='(status,c)'+(':=' if i==0 else '=')+'sys->dial("tcp!127.0.0.1!'+str(port)+'",nil);'
                consumer.write_bytes(BASE['compile_source'](limbo,work,body))
                output=execute(command+['-c',str(jit),'-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
                thread.join(20)
                if thread.is_alive() or errors or accepted!=[b'']*calls:raise RuntimeError('dial connection/retirement: '+repr((accepted,errors))+output)
                if 'exit=""' not in output or retired not in output or 'panic:' in output:raise RuntimeError(output)
    for image in refusals(rows(limbo,work)[0][0]):
        consumer.write_bytes(image)
        output=execute(command+['-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
        if retired in output or 'panic: loading Dis64 pair' not in output:raise RuntimeError('malformed dial scheduled: '+output)
    print('PASS Sys dial whole console: 70 TCP connections, replacement/retirement, C/JIT requests and malformed tuple refusals',flush=True)


if __name__=='__main__':
    import argparse,shlex,tempfile
    parser=argparse.ArgumentParser(description=__doc__)
    for option in ('limbo','root','emu'):parser.add_argument('--'+option,type=Path,required=True)
    parser.add_argument('--runner',default='');args=parser.parse_args()
    base=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
    with tempfile.TemporaryDirectory(prefix='sys-dial-',dir=args.root.resolve()) as td:
        work=Path(td);boot(shlex.split(args.runner)+[str(args.emu.resolve()),'-v','-r',str(args.root.resolve())],work,work/'dial.dis64',args.limbo,base['execute'],base['RETIRED'])
