#!/usr/bin/env python3
"""Compile Limbo call files and run them through production readmod/scheduler/C/RX."""
import argparse,json,os,runpy,shlex,struct,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FILES=runpy.run_path(str(ROOT/'utils/tests/dis64-file-calls.py'))
TYPED=runpy.run_path(str(ROOT/'utils/tests/dis64-typed-body.py'))
CALLS=FILES['INFER']['CALLS']
CHAIN=runpy.run_path(str(ROOT/'utils/tests/dis64-chain-oracle.py'))

def source(header,init,definitions):return 'implement Calls;Calls:module{init:fn();'+header+'};init(){'+init+'}'+definitions

def cases():
    rows=[]
    triangular='sum(n:int):int{total:=0;while(n>0){total+=n;n--;}return total;}'
    for n in (0,1,7,-3):
        t=lambda x:max(x,0)*(max(x,0)+1)//2
        rows.append(('sum-'+str(n),source('sum:fn(n:int):int;',f'a:=sum({n});b:=sum(a);',triangular),1,t(t(n)),0))
    rows+=[('add',source('add:fn(a:int,b:int):int;','a:=23;b:=19;c:=add(a,b);','add(a:int,b:int):int{return a+b;}'),1,42,0),
      ('branch',source('abs:fn(n:int):int;','a:=abs(-7);b:=abs(a);','abs(n:int):int{if(n<0)return -n;return n+2;}'),1,9,0),
      ('real',source('id:fn(n:real):real;','n:=42;r:=real n;a:=id(r);b:=id(a);','id(n:real):real{return n;}'),3,struct.unpack('>Q',struct.pack('>d',42.0))[0],0),
      ('void',source('touch:fn(n:int);','touch(7);touch(-3);','touch(n:int){v:=n+1;}'),0,0,0),
      ('leaf-error',source('quot:fn(n:int):int;','a:=quot(0);','quot(n:int):int{one:=1;return one/n;}'),1,0,1),
      ('root-error',source('id:fn(n:int):int;','a:=id(7);zero:=0;b:=a/zero;','id(n:int):int{return n;}'),1,0,1),
      ('three-functions',source('add:fn(a:int,b:int):int;sum:fn(n:int):int;','a:=add(3,4);b:=sum(a);', 'add(a:int,b:int):int{return a+b;}'+triangular),1,28,0),
      ('entry-last','implement Calls;Calls:module{sum:fn(n:int):int;init:fn();};'+triangular+'init(){a:=sum(3);}',1,6,0)]
    for n in (21,-3):
        rows.append(('big-'+str(n),source('wide:fn(n:big):big;',f'n:={n};v:=big n;a:=wide(v);','wide(n:big):big{return n+n;}'),2,(2*n)&((1<<64)-1),0))
    for suffix,value,tail in (('min',-2147483648,''),('max',2147483647,'n-=1;')):
        rows.append(('big-int32-'+suffix,source('wide:fn(n:big):big;','n:=1;n<<=30;n*=2;'+tail+'v:=big n;a:=wide(v);','wide(n:big):big{return n+n;}'),2,(2*value)&((1<<64)-1),0))
    rows += [
      ('nested-fresh',source('mid:fn(n:int):int;leaf:fn(n:int):int;','a:=mid(7);b:=mid(a);','mid(n:int):int{a:=leaf(n);b:=leaf(a);return b;}leaf(n:int):int{return n+1;}'),1,11,0),
      ('nested-forward',source('mid:fn(n:int):int;leaf:fn(n:int):int;','a:=mid(7);','mid(n:int):int{return leaf(n);}leaf(n:int):int{return n+1;}'),1,8,0),
      ('nested-deep',source('mid:fn(n:int):int;next:fn(n:int):int;leaf:fn(n:int):int;','a:=mid(7);','mid(n:int):int{return next(n);}next(n:int):int{return leaf(n);}leaf(n:int):int{return n+1;}'),1,8,0),
      ('nested-loop-leaf',source('mid:fn(n:int):int;sum:fn(n:int):int;','a:=mid(7);','mid(n:int):int{return sum(n);}'+triangular),1,28,0),
      ('nested-big',source('mid:fn(n:big):big;leaf:fn(n:big):big;','n:=-3;v:=big n;a:=mid(v);','mid(n:big):big{return leaf(n);}leaf(n:big):big{return n+n;}'),2,(-6)&((1<<64)-1),0),
      ('nested-real',source('mid:fn(n:real):real;leaf:fn(n:real):real;','n:=42;v:=real n;a:=mid(v);','mid(n:real):real{return leaf(n);}leaf(n:real):real{return n;}'),3,struct.unpack('>Q',struct.pack('>d',42.0))[0],0),
      ('nested-void',source('mid:fn(n:int);leaf:fn(n:int);','mid(7);','mid(n:int){leaf(n);}leaf(n:int){a:=n+1;}'),0,0,0),
      ('nested-diamond',source('mid:fn(n:int):int;left:fn(n:int):int;right:fn(n:int):int;leaf:fn(n:int):int;','a:=mid(7);','mid(n:int):int{a:=left(n);b:=right(n);return a+b;}left(n:int):int{return leaf(n);}right(n:int):int{return leaf(n);}leaf(n:int):int{return n+1;}'),1,16,0),
      ('nested-convert-big',source('mid:fn(n:int):big;leaf:fn(n:big):big;','a:=mid(-3);','mid(n:int):big{v:=big n;return leaf(v);}leaf(n:big):big{return n+n;}'),2,(-6)&((1<<64)-1),0),
      ('nested-convert-real',source('mid:fn(n:int):real;leaf:fn(n:real):real;','a:=mid(42);','mid(n:int):real{v:=real n;return leaf(v);}leaf(n:real):real{return n;}'),3,struct.unpack('>Q',struct.pack('>d',42.0))[0],0),
      ('nested-mid-error',source('mid:fn(n:int):int;leaf:fn(n:int):int;','a:=mid(0);','mid(n:int):int{one:=leaf(1);return one/n;}leaf(n:int):int{return n;}'),1,0,1),
      ('nested-error',source('mid:fn(n:int):int;leaf:fn(n:int):int;','a:=mid(0);','mid(n:int):int{return leaf(n);}leaf(n:int):int{one:=1;return one/n;}'),1,0,1)]
    return rows

def refusals():
    return [
      source('id:fn(n:int):int;mid:fn(n:int):int;','a:=mid(3);','id(n:int):int{return mid(n);}mid(n:int):int{return id(n);}'),
      source('id:fn(n:int):int;','a:=1;if(a>0){b:=id(a);}','id(n:int):int{return n;}'),
      source('id:fn(n:int):int;','a:=id(3);g=a;','g:int;id(n:int):int{return n;}'),
      source('id:fn(s:string):string;','s:=id(nil);','id(s:string):string{return s;}'),
      source('','a:=hidden(3);','hidden(n:int):int{return n;}'),
      source('id:fn(b:byte):byte;','a:=id(byte 7);','id(b:byte):byte{return b;}'),
      'implement Calls;Calls:module{init:fn(n:int);id:fn(n:int):int;};init(n:int){a:=id(n);}id(n:int):int{return n;}',
      'implement Calls;Calls:module{id:fn(n:int):int;};id(n:int):int{return n;}',
      source('id:fn(n:int):int;','a:=id(0);'+'a=id(a);'*60,'id(n:int):int{return n;}'),
      source('f:fn(x:real):real;','n:=1;r:=real n;a:=f(r);','f(x:real):real{return x+x;}')]

def rebuild(parts):
    at=92;directory=bytearray();payload=bytearray()
    for kind in (1,2,3,6,7):
        data=parts[kind];directory+=struct.pack('>HHII',kind,1,at,len(data));payload+=data;at+=len(data)
    return b'Dis64\r\n\x1a'+struct.pack('>HHIIIII',1,0,92,1,0,0,5)+directory+payload

def damaged(data):
    s=sections(data);decoded=CALLS.decode(s[2]);encoded=[x[-1]for x in decoded]
    first=lambda op:next(i for i,x in enumerate(decoded)if x[0]==op)
    for name,pc,raw in (
      ('call-target',first('ICALL'),CALLS.call(0)),
      ('frame-owner',first('IFRAME'),CALLS.frame(0)),
      ('result-address',first('ILEA'),CALLS.indirect('ILEA',64,40,32)),
      ('uninitialized',first('IFRAME')+1,CALLS.indirect('IMOVW',68,40,64)),
    ):
        items=encoded.copy();items[pc]=raw;t=dict(s);t[2]=CALLS.IMAGE(items);yield name,rebuild(t)
    t=dict(s);types=bytearray(t[3]);types[12]=128;t[3]=types;yield 'root-map',rebuild(t)
    t=dict(s);exports=bytearray(t[6]);firstlength=struct.unpack_from('>I',exports,16)[0];second=4+16+firstlength;struct.pack_into('>I',exports,second+20,3);t[6]=exports;yield 'signature-kind',rebuild(t)
    yield 'truncated',data[:-1]
    yield 'trailer',data+b'\0'

def nested_damaged(data):
    s=sections(data);decoded=CALLS.decode(s[2]);items=[x[-1]for x in decoded]
    r=CALLS.BASE.Reader(s[6]);entries=[]
    for _ in range(r.word()):
        symbol,pc,frame,length=[r.word()for _ in range(4)];r.take(length);entries.append((pc,frame))
    first,end=entries[1][0],entries[2][0]
    framepc=next(i for i in range(first,end)if decoded[i][0]=='IFRAME')
    callpc=next(i for i in range(first,end)if decoded[i][0]=='ICALL')
    bindpc=next(i for i in range(first,end)if decoded[i][0]=='ILEA')
    storepc=next(i for i in range(first,end)if decoded[i][1]&7==5 and decoded[i][4]==32)
    mutations={
      'self-cycle':{framepc:CALLS.frame(1,decoded[framepc][4]),callpc:CALLS.call(first,decoded[callpc][2])},
      'deep-frame-owner':{framepc:CALLS.frame(0,decoded[framepc][4])},
      'deep-result-owner':{bindpc:CALLS.indirect('ILEA',decoded[bindpc][2],48 if decoded[bindpc][4]==40 else 40,32)},
      'deep-result-source':{bindpc:CALLS.indirect('ILEA',64,decoded[bindpc][4],32)},
      'deep-return-owner':{storepc:CALLS.indirect('IMOVW',decoded[storepc][2],40,0)},
      'deep-return-missing':{storepc:CALLS.INIT.move(0,64)},
      'deep-leaf-return-missing':{next(i for i in range(end,len(decoded))if decoded[i][1]&7==5):CALLS.INIT.move(0,64)},
      'deep-early-return':{framepc:CALLS.RET},
      'deep-exit':{end-1:CALLS.EXIT}}
    for name,changes in mutations.items():
        out=items.copy()
        for pc,raw in changes.items():out[pc]=raw
        parts=dict(s);parts[2]=CALLS.IMAGE(out);yield name,rebuild(parts)

def mutual_damaged(data):
    s=sections(data);decoded=CALLS.decode(s[2]);r=CALLS.BASE.Reader(s[6]);entries=[]
    for _ in range(r.word()):
        symbol,pc,frame,length=[r.word()for _ in range(4)];r.take(length);entries.append((pc,frame))
    first,end=entries[2][0],entries[3][0];items=[x[-1]for x in decoded]
    framepc=next(i for i in range(first,end)if decoded[i][0]=='IFRAME')
    callpc=next(i for i in range(first,end)if decoded[i][0]=='ICALL')
    items[framepc]=CALLS.frame(1,decoded[framepc][4]);items[callpc]=CALLS.call(entries[1][0],decoded[callpc][2]);s[2]=CALLS.IMAGE(items)
    yield 'mutual-cycle',rebuild(s)

def sections(data):
    assert data[:8]==b'Dis64\r\n\x1a'and struct.unpack_from('>HHIIIII',data,8)==(1,0,92,1,0,0,5)
    result={}
    for i in range(5):
        kind,flags,offset,size=struct.unpack_from('>HHII',data,32+12*i);assert flags==1 and offset+size<=len(data);result[kind]=data[offset:offset+size]
    assert set(result)=={1,2,3,6,7};return result

def verify(data,rows,expected=True):
    s=sections(data);assert struct.unpack('>IIIII',s[1])==(0,1,0,0,0)
    decoded=CALLS.decode(s[2]);reader=CALLS.BASE.Reader(s[6]);exports=[]
    for i in range(reader.word()):
        symbol,pc,frame,length=[reader.word()for _ in range(4)];raw=reader.take(length);sig=struct.unpack('>'+'I'*(length//4),raw);assert sig[0]==1 and sig[2]==len(sig)-3;exports.append((symbol,pc,frame,sig))
    pool=CALLS.BASE.Reader(s[7]);names=[''.join(chr(pool.word())for _ in range(pool.word()))for i in range(pool.word())]
    types=CALLS.BASE.Reader(s[3]);frames=[]
    for i in range(types.word()):
        size,alignment=types.word(),types.word();assert alignment==8;assert not any(types.take((size+63)//64));frames.append(size)
    assert len(exports)==len(rows)==len(frames)and len(decoded)<=256
    byname={r['name']:r for r in rows};assert all(r['accepted']for r in rows)
    locals_=[]
    for i,(symbol,first,frame,sig)in enumerate(exports):
        r=byname[names[symbol]];end=exports[i+1][1]if i+1<len(exports)else len(decoded)
        assert frame==i and frames[i]==r['frame']['bytes']+16
        kind=lambda k:{0:0,9:1,4:2,7:3}[k] # Limbo scalar kinds from checked source reports.
        assert sig[1]==kind(r['result'][0])and sig[3:]==tuple(kind(a[1])for a in r['args'])
        spans=[]
        for old,new,oldbytes,width,k in r['frame']['leaves']:
            if k in (9,4,7):spans.append((new,kind(k)))
        if i and any(x[0]not in ('IMOVW','IMOVL','IMOVF','IRET','ILEA')and x[1]&7==5 for x in decoded[first:end]):raise AssertionError('unlowered expression result')
        # Numeric expression scratch is observed from raw opcodes, not supplied to the VM.
        for opname,mode,src,mid,dest,secondary,raw in decoded[first:end]:
            if mode&7==1 and dest==r['frame']['bytes']:
                k=3 if opname=='ICVTWF'or opname.endswith('F')else 2 if opname=='ICVTWL'else 2 if opname.endswith('L')else 1
                if (dest,k)not in spans:spans.append((dest,k))
        locals_.append((symbol,first,frame,tuple(sorted(spans))))
        if True:
            calls=[x for x in decoded[first:end]if x[0]=='ICALL'];assert len(calls)==len(r.get('calls',[]))
            for call,site in zip(calls,r.get('calls',[]))if expected else():
                target=next(e for e in exports if byname[names[e[0]]]['first']==site[2]);assert call[4]==target[1]
    case=(s[2],s[6],s[7],s[3],1048576,0,0,tuple(locals_),256,64,CALLS.LIMIT)
    assert bool(CHAIN['check'](CALLS,case))==expected,'independent raw DAG oracle disagreement'
    return s

def unit():
    s=FILES['unit']();extract=FILES['FILES']['function'];xec=(ROOT/'libinterp/xec.c').read_text()
    names=('cvtwl','subw','mulw','modw','andw','orw','xorw','shrw','beqw','bnew','bltw','blew','bgtw','bgew','jmp','addl','subl','mull','divl','modl','andl','orl','xorl','shll','shrl','beql','bnel','bltl','blel','bgtl','bgel','beqf','bnef','bltf','blef','bgtf','bgef')
    marker='/* Read immutable fixture bytes'
    pos=s.index(marker);s=s[:pos]+extract(xec,'static void\nbranchpc(')+'\n'+extract(xec,'static void\nbranchreal(')+'\n'+'\n'.join(extract(xec,'OP('+n+')')for n in names)+'\n'+s[pos:]
    needle='\toptab[IMOVF] = actual_movf;';s=s.replace(needle,needle+'\n'+'\n'.join('optab[I'+n.upper()+']=actual_'+n+';'for n in names))
    s=s.replace('steps<128','steps<10000').replace('steps < 128','steps < 10000')
    s=s.replace('if(kind != 0){','if(1){').replace('require(value == expected);','if(kind)require(value == expected);')
    return s

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--limbo',type=Path,required=True);p.add_argument('--baseline',type=Path);args=p.parse_args();limbo=args.limbo.resolve();env=dict(os.environ);arch=env.get('NATIVE64_ARCH','amd64')
    with tempfile.TemporaryDirectory(prefix='source-call-file-',dir=ROOT)as td:
        w=Path(td);env['TMPDIR']=str(w);fixtures=[]
        for name,text,kind,value,failure in cases():
            b=w/(name+'.b');b.write_text(text);image=w/(name+'.dis64')
            TYPED['READ']['checked_run']([str(limbo),'-64c','-o',str(image),str(b)],env=env,check=True,capture_output=True)
            _,rows=TYPED['compile_source'](limbo,w,None,filename=b,multi=True)
            try:verify(image.read_bytes(),rows)
            except AssertionError as e:raise AssertionError(name)from e
            if args.baseline:
                old=w/'baseline.dis';TYPED['READ']['checked_run']([str(args.baseline.resolve()),'-i','-o',str(old),str(b)],env=env,check=True,capture_output=True)
                assert old.read_bytes()==(w/'plain.dis').read_bytes()
            second=w/'again.dis64';TYPED['READ']['checked_run']([str(limbo),'-64c','-i','-o',str(second),str(b)],env=env,check=True,capture_output=True)
            assert image.read_bytes()==second.read_bytes()
            fixtures.append((name,image,kind,value,failure))
        for i,text in enumerate(refusals()):
            b=w/('refuse-'+str(i)+'.b');b.write_text(text);image=w/'refused.dis64'
            result=subprocess.run([str(limbo),'-64c','-o',str(image),str(b)],env=env,capture_output=True,text=True)
            assert result.returncode and 'Dis64 calls:'in result.stderr,result.stderr
            assert not image.exists()or image.stat().st_size==0
            assert not any(x in result.stderr for x in ('Sanitizer','runtime error:'))
        print('PASS source call file boundary: ten whole-source refusals publish no bytes',flush=True)
        print('PASS source call file writer: '+str(len(fixtures))+' complete Limbo programs, independent raw oracle and deterministic bytes',flush=True)
        file=w/'filecalls.c';file.write_text(unit());binary=w/'filecalls'
        cc=shlex.split(env.get('CC','cc'))+shlex.split(env.get('NATIVE64_CFLAGS','-O2 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
        if arch=='arm64':cc+=['-DDIS64_TEST_ARM64=1','-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch=='riscv64':cc+=['-DDIS64_TEST_RISCV64=1','-DDIS64_TEST_COMPILER="comp-riscv64.c"']
        elif arch!='amd64':raise RuntimeError(arch)
        cc+=['-DDIS_WORD32=1','-DDIS64_PILOT_LOADER=1']+['-I'+str(ROOT/x)for x in ('Linux/'+arch+'/include','Linux/386/include','emu/port','include','libinterp')]
        subprocess.run(cc+[str(file),str(ROOT/'libinterp/dec.c'),'-Wl,--wrap=free','-lm','-o',str(binary)],env=env,check=True)
        runner=shlex.split(env.get('NATIVE64_RUNNER',''));pilot=w/'pilot.dis64';pilot.write_bytes(bytes.fromhex((ROOT/'libinterp/tests/dis64/pilot42.hex').read_text()))
        for name,image,kind,value,failure in fixtures:
            result=subprocess.run(runner+[str(binary),str(image),'1',str(kind),str(value),str(pilot),str(failure)],env=env,capture_output=True,text=True,timeout=120)
            assert result.returncode==0,name+'\n'+result.stderr
            print('PASS source file readmod/scheduler C/RX, all allocation failures:',arch,name,flush=True)
        first=next(x[1]for x in fixtures if x[0]=='sum-7')
        subprocess.run(runner+[str(binary),str(first),'1','0','0',str(pilot),'0','default'],env=env,check=True,capture_output=True)
        nested=next(x[1]for x in fixtures if x[0]=='nested-forward')
        deep=next(x[1]for x in fixtures if x[0]=='nested-deep')
        for name,data in list(damaged(first.read_bytes()))+list(nested_damaged(nested.read_bytes()))+list(mutual_damaged(deep.read_bytes())):
            path=w/('bad-'+name+'.dis64');path.write_bytes(data)
            result=subprocess.run(runner+[str(binary),str(path),'0','0','0',str(pilot),'0'],env=env,capture_output=True,text=True,timeout=120)
            assert result.returncode==0,name+'\n'+result.stderr
        print('PASS source call file admission:',arch,'eighteen damaged images refused; default profile remains closed',flush=True)
        print('PASS source call file pipeline:',arch,len(fixtures),'files, actual cache/admission/reader/scheduler/teardown',flush=True)
if __name__=='__main__':main()
