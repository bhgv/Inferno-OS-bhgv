#!/usr/bin/env python3
"""Selected Limbo dial/mount/open/read/write/unmount pipeline and malformed image corpus."""
from pathlib import Path
import argparse,json,os,pwd,re,runpy,selectors,shlex,socket,struct,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
BASE=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-string.py'))
CONN=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-connection.py'))
CASES=runpy.run_path(str(ROOT/'utils/tests/dis64-sys-mount-cases.py'))
RAW=BASE['RAW'];section=BASE['section'];replace=BASE['replace']


def rows(limbo,work):
    return [(name,BASE['compile_source'](limbo,work,'sys:=load Sys Sys->PATH;'+body),events,status)
            for name,body,events,status,_ in CASES['programs']()]


def refusals(rows):
    decode=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['decode']
    indirect=runpy.run_path(str(ROOT/'utils/tests/dis64-bound-call.py'))['indirect']
    for name,image,_,_ in rows:
        if name not in ('mount-only','mount-open','mount-origin'):continue
        code=decode(section(image,2));raw=[r[-1]for r in code];names=CONN['imports'](image)
        groups=[]
        for i,r in enumerate(code):
            if r[0]=='IMFRAME':
                end=next(j+1 for j in range(i,len(code))if code[j][0]=='IMCALL')
                if end<len(code)and code[end][0]=='IMOVW':end+=1
                groups.append((names[r[3]],i,end))
        def changed(at,new):
            parts=list(raw);parts[at]=new;return replace(image,2,RAW['W'](len(parts))+b''.join(parts))
        for method,start,end in groups:
            if method=='mount':
                fd=next(i for i in range(start,end)if code[i][0]=='IMOVP'and code[i][5]==64)
                for slot in (80,104,112,120,128,132,136,144):
                    yield changed(fd,indirect('IMOVP',slot,40,64))
                for i in range(start,end):
                    if code[i][0]in('IMOVP','IMOVW')and code[i][4]==40:
                        for offset in (32,64,72,80,88,92,96,104):
                            if offset!=code[i][5]:yield changed(i,indirect(code[i][0],code[i][2],40,offset))
                    if code[i][0]=='ILEA':
                        for slot in (64,80,88,112,144):yield changed(i,indirect('ILEA',slot,40,32))
        for method,start,end in groups:
            if method=='open':
                at=next(i for i in range(start,end)if code[i][0]=='ILEA')
                for slot in (64,80,88,96,104,112,120,128,136,152):
                    yield changed(at,indirect('ILEA',slot,40,32))
        signature=section(image,5);at=4
        for method in names:
            n=struct.unpack_from('>I',signature,at+8)[0]
            if method=='mount':
                for offset in range(0,n,4):
                    bad=bytearray(signature);struct.pack_into('>I',bad,at+12+offset,0);yield replace(image,5,bad)
            at+=12+n
        types=section(image,3)
        for i,value in ((13,0x5f),(13,0xcf),(14,0),(14,0x80)):
            if types[i]!=value:
                bad=bytearray(types);bad[i]=value;yield replace(image,3,bad)
        if name in ('mount-open','mount-origin'):
            opening=next(g for g in groups if g[0]=='open');reading=next(g for g in groups if g[0]=='read')
            _,start,end=opening;_,read,stop=reading
            # Valid call groups and complete imports, but the FD is read before open.
            parts=raw[:start]+raw[end:stop]+raw[start:end]+raw[stop:]
            yield replace(image,2,RAW['W'](len(parts))+b''.join(parts))
            # Open must not manufacture the separate Connection-origin fact.
            if name=='mount-origin':
                dial=next(g for g in groups if g[0]=='dial');mount=next(g for g in groups if g[0]=='mount')
                parts=raw[:dial[1]]+raw[start:end]+raw[mount[1]:mount[2]]+raw[dial[1]:dial[2]]+raw[mount[2]:start]+raw[end:]
                yield replace(image,2,RAW['W'](len(parts))+b''.join(parts))


def compiler_refusals(limbo,work):
    dial='sys:=load Sys Sys->PATH;(s,c):=sys->dial("one",nil);'
    for body in (
        dial+'r:=sys->mount(c.dfd,nil,"/m",0,"");fd:=sys->open("/m/x",0);other:=fd;r=sys->iounit(other);',
        dial+'r:=sys->mount(c.dfd,nil,"/m",0,"");fd:=sys->open("/m/x",0);fd=nil;',
        dial+'r:=sys->mount(c.dfd,nil,"/m",0,"");fd:=sys->open("/m/x",0);other:=sys->open("/m/y",0);r=sys->iounit(fd);r=sys->iounit(other);',
        dial+'while(s==0){r:=sys->mount(c.dfd,nil,"/m",0,"");}',
    ):
        BASE['compile_source'](limbo,work,body,accepted=False)


def stop(server,port,closed,released):
    wire=runpy.run_path(str(ROOT/'tools/libstyx/tests/loopback.py'))
    with socket.create_connection(('127.0.0.1',port),timeout=5)as peer:
        wire['rpc'](peer,100,struct.pack('<I',1024)+wire['string']('9P2000'),65535)
        wire['rpc'](peer,104,struct.pack('<II',1,0xffffffff)+wire['string']('alice')+wire['string'](''))
        wire['openfile'](peer,3,'ctl',1)
        wire['rpc'](peer,118,struct.pack('<IQI',3,0,4)+b'stop')
    output,errors=server.communicate(timeout=10)
    if server.returncode or f'closed {closed} handles, released {released} clients'not in output:
        raise RuntimeError(output+errors)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('limbo','root','emu','server'):parser.add_argument('--'+name,required=True,type=Path)
    parser.add_argument('--runner',default='');a=parser.parse_args()
    boot=runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))
    proxy=runpy.run_path(str(ROOT/'tools/libstyx/tests/mount-native64.py'))['fragmented_version']
    root=a.root.resolve();command=shlex.split(a.runner)+[str(a.emu.resolve()),'-v','-r',str(root)];count=0
    with tempfile.TemporaryDirectory(prefix='guest-mount-',dir=root)as directory:
        work=Path(directory);image=work/'guest.dis64';compiler_refusals(a.limbo,work)
        tutorials=[re.findall(r'```limbo\n(.*?)```',(ROOT/('doc/dis64-sys-mount'+lang+'.md')).read_text(),re.S)for lang in ('','.ru')]
        if len(tutorials[0])!=1 or tutorials[0]!=tutorials[1]:raise RuntimeError('RU/EN tutorial mismatch')
        for mode in ('mount','read','write','replace','missing','remount','tutorial'):
            for jit in (0,1):
                for fragment in (False,True):
                    handles={'mount':0,'read':1,'write':1,'replace':2,'missing':0,'remount':0,'tutorial':1}[mode]
                    server=subprocess.Popen([str(a.server.resolve()),str(handles+1),'2',pwd.getpwuid(os.getuid()).pw_name],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                    try:
                        with selectors.DefaultSelector()as ready:
                            ready.register(server.stdout,selectors.EVENT_READ)
                            if not ready.select(10):raise RuntimeError('server did not announce')
                        port=int(server.stdout.readline());seen=[];responses=[]
                        with proxy(port,seen,fragment,responses)as clientport:
                            body='sys:=load Sys Sys->PATH;(status,c):=sys->dial("tcp!127.0.0.1!'+str(clientport)+'",nil);if(status<0)return;r:=sys->mount(c.dfd,nil,"/mnt",Sys->MREPL,"");if(r<0)return;'
                            if mode in ('read','replace','missing'):
                                path='/mnt/missing'if mode=='missing'else'/mnt/first'
                                body+='fd:=sys->open('+json.dumps(path)+',Sys->OREAD);b:=array[16]of byte;r=sys->read(fd,b,len b);'
                                if mode!='missing':body+='if(r<0)return;text:=string b[:r];r=sys->werrstr(text);'
                            if mode in ('write','replace'):
                                body+=('fd='if mode=='replace'else'fd:=')+'sys->open("/mnt/ctl",Sys->OWRITE);'+('b='if mode=='replace'else'b:=')+'array of byte "change";r=sys->write(fd,b,len b);'
                            body+='r=sys->unmount(nil,"/mnt");'
                            if mode=='remount':body+='r=sys->mount(c.dfd,nil,"/mnt",0,"");r=sys->unmount(nil,"/mnt");'
                            if mode=='tutorial':body=re.search(r'init\(\)\s*\{(.*)\}\s*$',tutorials[0][0],re.S).group(1).replace('tcp!127.0.0.1!5640','tcp!127.0.0.1!'+str(clientport))
                            image.write_bytes(BASE['compile_source'](a.limbo,work,body))
                            output=boot['execute'](command+['-c',str(jit),'-d','/'+str(image.relative_to(root)),'-P','$Sys','-X','dis64-managed-trial'])
                            if f'native64 scalar module SysText: jit={jit}, exit=""'not in output or boot['RETIRED']not in output or 'panic:'in output or 'faults:'in output:raise RuntimeError(output)
                        attaches=2 if mode=='remount'else 1
                        if seen.count(100)!=1 or seen.count(104)!=attaches or seen.count(120)!=attaches+handles:
                            raise RuntimeError((mode,seen))
                        if mode in ('read','replace','tutorial')and 116 not in seen:raise RuntimeError('missing Tread')
                        if mode in ('write','replace')and 118 not in seen:raise RuntimeError('missing Twrite')
                        if mode!='missing' and any(packet[4]==107 for packet in responses):raise RuntimeError('unexpected remote Rerror')
                        if mode in ('read','replace','tutorial'):
                            reads=[packet[11:] for packet in responses if packet[4]==117]
                            if reads!=[b'initial']:raise RuntimeError('remote read payload mismatch: '+repr(reads))
                        if mode in ('write','replace'):
                            counts=[struct.unpack_from('<I',packet,7)[0]for packet in responses if packet[4]==119]
                            if counts!=[6]:raise RuntimeError('remote write acknowledgement mismatch')
                        stop(server,port,handles+1,2);count+=1
                    finally:
                        if server.poll()is None:server.terminate();server.communicate(timeout=5)
            print('PASS guest mount full console:',mode,'C/JIT direct/fragmented, actual request and close counts',flush=True)
        print('PASS guest mount full console:',count,'roots',flush=True)


if __name__=='__main__':main()
