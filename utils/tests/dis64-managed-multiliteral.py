#!/usr/bin/env python3
"""Independent multi-root String programs, byte oracles and malformed images."""
from pathlib import Path
import json
import struct
import subprocess


# Decode pool framing independently of the native reader.
def pool_records(image, section):
    data=section(image,7); at=4; records=[]
    for _ in range(struct.unpack_from('>I',data)[0]):
        count=struct.unpack_from('>I',data,at)[0]; at+=4
        records.append(struct.unpack_from('>'+'I'*count,data,at)); at+=4*count
    if at!=len(data): raise RuntimeError('invalid independent pool framing')
    return records


# Serialize the exact expected call stream from source-level argument identities.
def expected_code(raw, calls, rootids, symbols, reloads):
    words,ins=raw['W'],raw['I']; code=b''; count=1
    indirect=__import__('runpy').run_path(str(Path(__file__).with_name('dis64-bound-call.py')))['indirect']
    for i,(method,value) in enumerate(calls):
        if value is None:
            code+=ins('IMOVP',9,source=72,destination=64); count+=1
        if i:
            code+=ins('IMOVP',1,source=8,destination=72); count+=1
        if not i or i in reloads:
            code+=ins('ILOAD',64,source=16,middle=0,destination=0); count+=1
        if value is False:
            code+=ins('IMOVP',1,source=8,destination=64); count+=1
        elif value is not None:
            code+=ins('IMOVP',1,source=24+8*rootids[tuple(map(ord,value))],destination=64); count+=1
        ordinal=symbols[method]
        code+=ins('IMFRAME',65,source=0,middle=ordinal,destination=40)
        code+=indirect('IMOVP',64,40,64)
        code+=indirect('ILEA',72,40,32)
        code+=ins('IMCALL',72,source=40,middle=ordinal,destination=0); count+=4
    return words(count)+code+raw['EXIT']


# Compile sources and compare root sets, maps and full CODE against independent expectations.
def rows(limbo, work, path, provider, raw, section, module_root):
    cases=[]; source=work/'multi-literal.b'; output=work/'multi-literal.dis64'
    programs=[]
    for n in (2,5,6,13,14,21,22,29,30,32):
        values=[f'root-{i:02d}' for i in range(n)]
        programs.append((values,[('unquoted',v) for v in values],set(range(1,n)),False))
    values=['A\0B','Grüße 世界','🙂𝄞','/not-a-load-path']
    programs.append((values,[('unquoted',v) if i%2==0 else ('quoted',None) for i,v in enumerate(sum(([v,v] for v in values),[]))],{2,4,6},False))
    programs.append((['earlier','later'],[('unquoted','later'),('unquoted','earlier'),('quoted',None),('unquoted','later'),('quoted',None)],{3},True))
    programs.append((['first','last'],[('unquoted','first'),('quoted',None),('unquoted',False),('quoted',None),('unquoted','last'),('quoted',None)],{4},False))
    maximal=['🙂'*254+chr(0x1f600+i) for i in range(4)]
    programs.append((maximal+['tail'],[('unquoted',value) for value in maximal]+[('unquoted','tail')],set(),False))
    values=[f'S{i:02d}:'+'x'*28 for i in range(32)]
    programs.append((values,[('unquoted',v) for v in values],set(range(1,32)),False))
    for values,calls,reloads,alias in programs:
        body=f'lib := load String {json.dumps(path)}; '
        if alias: body+='original := "earlier"; saved := original; original = nil; '
        declared=set()
        for i,(method,value) in enumerate(calls):
            if i in reloads: body+=f'lib = load String {json.dumps(path)}; '
            destination='items' if method=='unquoted' else 'text'
            argument=('saved' if alias and value=='earlier' else json.dumps(value,ensure_ascii=False)) if isinstance(value,str) else 'nil' if value is False else ('items' if method=='quoted' else 'text')
            body+=destination+(' = ' if destination in declared else ' := ')+f'lib->{method}({argument}); '
            declared.add(destination)
        source.write_text('implement Caller; include "string.m"; Caller: module {init: fn();}; init(){'+body+'}')
        result=subprocess.run([str(limbo.resolve()),'-64l','-I',str(module_root/'module'),'-o',str(output),str(source)],capture_output=True,text=True,timeout=30)
        if result.returncode: raise RuntimeError('multi-literal source failed: '+body+'\n'+result.stderr)
        image=output.read_bytes(); records=pool_records(image,section); imports=section(image,5); bindings=struct.unpack_from('>I',imports)[0]
        roots=records[3+bindings:]; expected={tuple(map(ord,v)) for v in values}
        if set(roots)!=expected or len(roots)!=len(expected): raise RuntimeError('literal roots changed source values')
        rootids={v:i for i,v in enumerate(roots)}; symbols={}; at=4
        for i in range(bindings):
            _,symbol,size=struct.unpack_from('>III',imports,at); symbols[''.join(map(chr,records[symbol]))]=i; at+=12+size
        bitmap=bytearray((3+len(roots)+7)//8)
        for i in range(3+len(roots)): bitmap[i//8]|=0x80>>(i%8)
        types=raw['W'](2)+raw['D'](80,8,b'\0\xc0')+raw['D'](24+8*len(roots),8,bitmap)
        if section(image,3)!=types: raise RuntimeError('multi-literal pointer map mismatch')
        if section(image,2)!=expected_code(raw,calls,rootids,symbols,reloads): raise RuntimeError('multi-literal CODE changed argument identities/order')
        last=next(value for method,value in reversed(calls) if isinstance(value,str))
        cases.append((image,provider,tuple(map(ord,last))))
    # Equal serialized contents still get independent owned roots.
    image,provider,_=cases[0]; records=pool_records(image,section)
    base=4; duplicate=tuple(map(ord,'duplicate bits'))
    records[base:]=[duplicate]*(len(records)-base)
    pool=struct.pack('>I',len(records))+b''.join(struct.pack('>I',len(runes))+b''.join(struct.pack('>I',v) for v in runes) for runes in records)
    cases.append((__import__('runpy').run_path(str(Path(__file__).with_name('dis64-managed-literal.py')))['replace'](image,7,pool),provider,duplicate))
    return cases


# Reject a bad upper root, missing root use and aggregate overflow before allocation.
def refusals(image,raw,section,replace):
    code=section(image,2); types=section(image,3)
    n=(struct.unpack_from('>I',section(image,1),8)[0]-24)//8
    if n!=32: raise RuntimeError('upper-root fixture drift')
    for offset in (24+8*32,25,16,0x100000):
        old=raw['I']('IMOVP',1,source=24+8*31,destination=64)
        new=raw['I']('IMOVP',1,source=offset,destination=64)
        if old not in code: raise RuntimeError('upper-root operand drift')
        yield replace(image,2,code.replace(old,new))
    old=raw['I']('IMOVP',1,source=24+8*31,destination=64)
    yield replace(image,2,code.replace(old,raw['I']('IMOVP',1,source=24,destination=64)))
    yield replace(image,3,types[:-1]+bytes((types[-1]&~0x20,)))
    yield replace(image,3,types[:-1]+bytes((types[-1]|1,)))
    records=pool_records(image,section); encoded=[]
    for i,runes in enumerate(records):
        if i>=4: runes=tuple([ord('a')]*(33 if i<35 else 1))
        encoded.append(struct.pack('>I',len(runes))+b''.join(struct.pack('>I',v) for v in runes))
    # 31*33 + 1 = 1024 is valid; one extra rune exceeds the aggregate budget.
    encoded[-1]=struct.pack('>III',2,ord('a'),ord('b'))
    yield replace(image,7,struct.pack('>I',len(records))+b''.join(encoded))
