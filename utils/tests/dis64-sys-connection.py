#!/usr/bin/env python3
"""Use actual Connection fields, Word guards and read/write in the private file profile."""
from pathlib import Path
import json,runpy,struct
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string.py'))
RAW=BASE['RAW'];section=BASE['section'];replace=BASE['replace']
ROUNDTRIP='(status,c):=sys->dial("one",nil);if(status<0)return;b:=array of byte "ping";n:=sys->write(c.dfd,b,len b);b=array[16] of byte;n=sys->read(c.dfd,b,len b);if(n<0)return;text:=string b[:n];r:=sys->werrstr(text);b=array of byte text;n=sys->write(c.dfd,b,len b);'
EVENTS=[('dial','one',0,0,0,''),('write','ping',100,4,4),('read','pon',100,16,3),('werrstr','pon',0,0,0),('write','pon',100,3,3)]


def programs():
    return [
      ('roundtrip',ROUNDTRIP,EVENTS,0,False),
      ('dial-failure',ROUNDTRIP.replace('"one"','"fail"'),[('dial','fail',0,0,-1,'')],-1,False),
      ('read-failure',ROUNDTRIP,EVENTS[:2]+[('read','',100,16,-1)],0,False),
      ('fields','(status,c):=sys->dial("one",nil);r:=sys->iounit(c.dfd);r=sys->iounit(c.cfd);r=sys->werrstr(c.dir);',[EVENTS[0],('iounit',None,100,0,4096),('iounit',None,101,0,4096),('werrstr','directory',0,0,0)],0,False),
      ('directory-bytes','(status,c):=sys->dial("one",nil);b:=array of byte c.dir;n:=sys->write(c.dfd,b,len b);',[EVENTS[0],('write','directory',100,9,9)],0,False),
      ('empty','(status,c):=sys->dial("one",nil);b:=array[0] of byte;n:=sys->read(c.dfd,b,len b);text:=string b;r:=sys->werrstr(text);',[EVENTS[0],('read','',100,0,0),('werrstr','',0,0,0)],0,False),
      ('saved-count','(status,c):=sys->dial("one",nil);b:=array of byte "ping";n:=sys->read(c.dfd,b,len b);r:=sys->werrstr(c.dir);b=array[n] of byte;n=sys->write(c.dfd,b,len b);',[EVENTS[0],('read','hi',100,4,2),('werrstr','directory',0,0,0),('write','\0\0',100,2,2)],0,True),
      ('status-word','(status,c):=sys->dial("one",nil);status++;',[EVENTS[0]],1,False),
      ('nil-buffer','(status,c):=sys->dial("one",nil);n:=sys->read(c.dfd,nil,0);',[EVENTS[0],('read','',100,0,0)],0,False),
      ('unicode-reply',ROUNDTRIP,[EVENTS[0],EVENTS[1],('read','Grüße',100,16,7),('werrstr','Grüße',0,0,0),('write','Grüße',100,7,7)],0,False),
      ('replace-after-io',ROUNDTRIP+'(status,c)=sys->dial("two",nil);n=sys->write(c.dfd,b,len b);',EVENTS+[('dial','two',0,0,0,''),('write','pon',102,3,3)],0,False),
      ('pre-dial-word','r:=sys->werrstr("first");(status,c):=sys->dial("one",nil);r=sys->iounit(c.dfd);',[('werrstr','first',0,0,0),EVENTS[0],('iounit',None,100,0,4096)],0,False),
    ]


def imports(image):
    pool=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-multiliteral.py'))['pool_records'](image,section)
    payload=section(image,5);at=4;result=[]
    for _ in range(struct.unpack_from('>I',payload)[0]):
        owner,symbol,n=struct.unpack_from('>III',payload,at);at+=12+n
        result.append(''.join(map(chr,pool[symbol])))
    return result


def rows(limbo,work):
    return [(name,BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;'+body,extra=('-z',) if zero else ()),events,status) for name,body,events,status,zero in programs()]


def refusals(rows):
    decode=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode'];ins=RAW['I']
    image=rows[0][1];records=decode(section(image,2));raw=[r[-1] for r in records]
    def changed(at,new):
        code=list(raw);code[at]=new;return replace(image,2,RAW['W'](len(code))+b''.join(code))
    wordarg=next(i for i,r in enumerate(records) if r[0]=='ILENA' and r[5]==80)
    indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    for source in (64,72,88,96,104,112,120,140):yield changed(wordarg,indirect('IMOVW',source,40,80))
    fdarg=next(i for i,r in enumerate(records) if r[0]=='IMOVP' and r[2]==88 and r[5]==64)
    for source in (64,72,80,104,112,120,128,132):yield changed(fdarg,indirect('IMOVP',source,40,64))
    scalarlea=next(i for i,r in enumerate(records) if r[0]=='ILEA' and r[2]==128)
    for source in (64,72,80,88,96,104,112,120,132,136):yield changed(scalarlea,indirect('ILEA',source,40,32))
    types=section(image,3)
    for byte in (0xdd,0xdb,0xd7,0xcf,0x5f):
        bad=bytearray(types);bad[13]=byte;yield replace(image,3,bad)
    for value in (0x80,0x40):
        bad=bytearray(types);bad[14]=value;yield replace(image,3,bad)
    bad=bytearray(types);bad[-1]=0x80;yield replace(image,3,bad)
    branch=next(i for i,r in enumerate(records) if r[0]=='IBLEW')
    for target in (branch+1,branch+3,fdarg,scalarlea):yield changed(branch,ins('IBLEW',146,source=0,middle=80,destination=target))
    # A String/Word call cannot initialize the future Connection's FD fields.
    image=next(r[1] for r in rows if r[0]=='pre-dial-word');records=decode(section(image,2));raw=[r[-1] for r in records];names=imports(image)
    starts=[i for i,r in enumerate(records) if r[0]=='IMFRAME'];dial=starts[1];iounit=starts[2]
    end=next(i+1 for i in range(iounit,len(records)) if records[i][0]=='IMCALL');end+=1
    code=raw[:dial]+raw[iounit:end]+raw[dial:iounit]+raw[end:]
    yield replace(image,2,RAW['W'](len(code))+b''.join(code))


def compiler_refusals(limbo,work):
    for suffix in ('x:=c;r:=sys->iounit(x.dfd);','c.dfd=nil;r:=sys->iounit(c.dfd);','if(status>=0){r:=sys->iounit(c.dfd);}'):
        body='sys:=load Sys Sys->PATH;(status,c):=sys->dial("one",nil);'+suffix
        BASE['compile_source'](limbo,work,body,accepted=False)


def boot(command,work,consumer,limbo,execute,retired):
    import socket,threading
    for request,reconnect in (('ping',False),('Grüße',False),('ping',True)):
        for jit in (0,1):
            received=[];errors=[];reply=b'pong\n'
            with socket.socket() as listener:
                listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15);port=listener.getsockname()[1]
                def server():
                    try:
                        with listener.accept()[0] as connection:
                            connection.settimeout(15);data=b'';expected=request.encode()
                            while len(data)<len(expected):
                                chunk=connection.recv(len(expected)-len(data))
                                if not chunk:raise RuntimeError('early request EOF')
                                data+=chunk
                            if data!=expected:raise RuntimeError('wrong request '+repr(data))
                            connection.sendall(reply);echo=b''
                            while True:
                                chunk=connection.recv(64)
                                if not chunk:break
                                echo+=chunk
                            if not echo or not reply.startswith(echo):raise RuntimeError('wrong read/slice/string echo '+repr(echo))
                            received.append(echo)
                        if reconnect:
                            with listener.accept()[0] as connection:
                                connection.settimeout(15);data=b''
                                while True:
                                    chunk=connection.recv(64)
                                    if not chunk:break
                                    data+=chunk
                                if data!=received[0]:raise RuntimeError('lost buffer across Connection replacement '+repr(data))
                                received.append(data)
                    except Exception as error:errors.append(str(error))
                thread=threading.Thread(target=server);thread.start()
                address='tcp!127.0.0.1!'+str(port)
                body=ROUNDTRIP.replace('"one"',json.dumps(address)).replace('"ping"',json.dumps(request,ensure_ascii=False))
                if reconnect:body+='(status,c)=sys->dial('+json.dumps(address)+',nil);n=sys->write(c.dfd,b,len b);'
                consumer.write_bytes(BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;'+body))
                output=execute(command+['-c',str(jit),'-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
                thread.join(20)
                if thread.is_alive() or errors or len(received)!=(2 if reconnect else 1):raise RuntimeError('connection data exchange '+repr((received,errors))+output)
                if 'exit=""' not in output or retired not in output or 'panic:' in output:raise RuntimeError(output)
    # A bound, non-listening socket reserves the port and deterministically refuses connect.
    with socket.socket() as refused:
        refused.bind(('127.0.0.1',0));address='tcp!127.0.0.1!'+str(refused.getsockname()[1])
        consumer.write_bytes(BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;'+ROUNDTRIP.replace('"one"',json.dumps(address))))
        for jit in (0,1):
            output=execute(command+['-c',str(jit),'-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
            if 'exit=""' not in output or retired not in output or 'panic:' in output:raise RuntimeError('failed dial guard '+output)
    compiler_refusals(limbo,work)
    for image in refusals(rows(limbo,work)):
        consumer.write_bytes(image);output=execute(command+['-d','/'+str(consumer.relative_to(work.parent)),'-P','$Sys','-X','dis64-managed-trial'])
        if retired in output or 'panic: loading Dis64 pair' not in output:raise RuntimeError('malformed connection scheduled: '+output)
    print('PASS Sys connection whole console: 8 real TCP connections, byte exchange/UTF/sliced read echo/replacement, 2 failed-dial guards, C/JIT and malformed-file refusals',flush=True)


if __name__=='__main__':
    import argparse,shlex,tempfile
    parser=argparse.ArgumentParser(description=__doc__)
    for option in ('limbo','root','emu'):parser.add_argument('--'+option,type=Path,required=True)
    parser.add_argument('--runner',default='');args=parser.parse_args()
    base=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
    with tempfile.TemporaryDirectory(prefix='sys-connection-',dir=args.root.resolve()) as td:
        work=Path(td);boot(shlex.split(args.runner)+[str(args.emu.resolve()),'-v','-r',str(args.root.resolve())],work,work/'connection.dis64',args.limbo,base['execute'],base['RETIRED'])
