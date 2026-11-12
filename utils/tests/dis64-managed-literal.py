#!/usr/bin/env python3
"""Serialized literal roots, independent mutations and self-contained String callers."""
from pathlib import Path
import json
import runpy
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
RAW = runpy.run_path(str(ROOT/'utils/tests/dis64-numeric-module.py'))


# Preserve all required sections while replacing one payload in a compiler image.
def replace(image, kind, payload):
    sections = []
    for i in range(struct.unpack_from('>I', image, 28)[0]):
        tag, flags, offset, size = struct.unpack_from('>HHII', image, 32+12*i)
        if flags != 1: raise RuntimeError('unexpected optional literal section')
        sections.append((tag, payload if tag == kind else image[offset:offset+size]))
    return RAW['IMAGE'](sections)


# Resolve a payload independently of the C decoder under test.
def section(image, kind):
    for i in range(struct.unpack_from('>I', image, 28)[0]):
        tag, _, offset, size = struct.unpack_from('>HHII', image, 32+12*i)
        if tag == kind: return image[offset:offset+size]
    raise RuntimeError('missing literal section')


# Compile bounded callers, including Unicode, embedded NUL and repeated/reloaded inputs.
def rows(limbo, work, path='/managed-provider.dis64'):
    groups = runpy.run_path(str(ROOT/'utils/tests/dis64-managed-groups.py'))
    _, provider_text = groups['sources']()
    source, output = work/'literal-provider.b',work/'literal-provider.dis64'
    source.write_text(provider_text)
    subprocess.run([str(limbo.resolve()),'-64m','-o',str(output),str(source)],check=True,timeout=30)
    provider = output.read_bytes()
    results = []
    for text in ('plain', 'Grüße 世界', '🙂𝄞', 'a\0b', '/a/path', 'x'*255):
        # Limbo accepts the same escapes for these characters; JSON keeps NUL explicit.
        quoted = json.dumps(text,ensure_ascii=False)
        for alias,reload in ((False,False),(True,True)):
            body = f'lib := load String "{path}"; '
            if alias: body += f'seed := {quoted}; alias := seed; seed = nil; items := lib->unquoted(alias); '
            else: body += f'items := lib->unquoted({quoted}); '
            body += 'value := lib->quoted(items); '
            if reload: body += f'lib = load String "{path}"; '
            body += f'items = lib->unquoted({quoted}); value = lib->quoted(items);'
            source.write_text('implement Caller; include "string.m"; Caller: module { init: fn(); }; init(){'+body+'}')
            compiled=subprocess.run([str(limbo.resolve()),'-64l','-I',str(ROOT/'module'),'-o',str(output),str(source)],capture_output=True,text=True,timeout=30)
            if compiled.returncode: raise RuntimeError(body+'\n'+compiled.stdout+compiled.stderr)
            image = output.read_bytes()
            pool = section(image,7); at=4; records=[]
            for _ in range(struct.unpack_from('>I',pool)[0]):
                n=struct.unpack_from('>I',pool,at)[0]; records.append(pool[at:at+4+4*n]); at+=4+4*n
            values = tuple(ord(c) for c in text)
            expected = struct.pack('>I',len(values))+b''.join(struct.pack('>I',v) for v in values)
            if records[-1]!=expected: raise RuntimeError('compiler changed literal Rune32 bits: '+repr(text)+' '+records[-1].hex()+' != '+expected.hex())
            results.append((image,provider,values))
    for calls in (1,32):
        body = f'lib := load String "{path}"; items := lib->unquoted("single binding");'
        body += (f'lib = load String "{path}"; items = lib->unquoted("single binding");')*(calls-1)
        source.write_text('implement Caller; include "string.m"; Caller: module {init: fn();}; init(){'+body+'}')
        subprocess.run([str(limbo.resolve()),'-64l','-I',str(ROOT/'module'),'-o',str(output),str(source)],check=True,timeout=30)
        results.append((output.read_bytes(),provider,tuple(map(ord,'single binding'))))
    # A non-nil empty pool record is distinct from Limbo's source-level nil cell.
    image,provider,_=results[0]
    pool=section(image,7); at=4
    for _ in range(struct.unpack_from('>I',pool)[0]-1): at+=4+4*struct.unpack_from('>I',pool,at)[0]
    results.append((replace(image,7,pool[:at]+struct.pack('>I',0)),provider,()))
    # Raw Rune32 values are lossless; they are not restricted to Unicode scalar values.
    image,provider,_=results[2]
    pool=section(image,7); at=4
    for _ in range(struct.unpack_from('>I',pool)[0]-1): at+=4+4*struct.unpack_from('>I',pool,at)[0]
    values=(0,0xffffffff,0x80000000,0x1f642)
    encoded=struct.pack('>I',len(values))+b''.join(struct.pack('>I',v) for v in values)
    results.append((replace(image,7,pool[:at]+encoded),provider,values))
    multi=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-multiliteral.py'))
    results+=multi['rows'](limbo,work,path,provider,RAW,section,ROOT)
    return results


# Mutate one independent obligation at a time, retaining a complete container.
def refusals(image):
    code=section(image,2)
    literal=RAW['I']('IMOVP',1,source=24,destination=64)
    at=code.find(literal)
    if at<0: raise RuntimeError('literal operand pattern drift')
    for source in (0,8,16,32):
        changed=RAW['I']('IMOVP',1,source=source,destination=64)
        yield replace(image,2,code.replace(literal,changed))
    types=section(image,3)
    if types[-1]!=0xf0: raise RuntimeError('literal map drift')
    yield replace(image,3,types[:-1]+b'\xe0')
    pool=section(image,7); last=4
    count=struct.unpack_from('>I',pool)[0]
    for _ in range(count-1): last+=4+4*struct.unpack_from('>I',pool,last)[0]
    yield replace(image,7,struct.pack('>I',count-1)+pool[4:last])
    yield replace(image,7,pool[:last]+struct.pack('>I',256)+b'\0'*1024)
    # A literal cannot be bound to quoted(list of string), even for the empty string.
    frame=RAW['I']('IMFRAME',65,source=0,middle=1,destination=40)
    call=RAW['I']('IMCALL',72,source=40,middle=1,destination=0)
    changed=code.replace(frame,RAW['I']('IMFRAME',65,source=0,middle=0,destination=40),1)
    changed=changed.replace(call,RAW['I']('IMCALL',72,source=40,middle=0,destination=0),1)
    if changed==code: raise RuntimeError('literal binding pattern drift')
    yield replace(image,2,changed)


# Generate constructor fixtures with independent expected payload bits.
def header(limbo, work):
    multi=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-multiliteral.py'))
    output=['typedef struct ManagedLiteralRoot { unsigned int count; const unsigned int *values; } ManagedLiteralRoot;',
            'typedef struct ManagedLiteralCase { const uchar *consumer; unsigned int consumerbytes; const uchar *provider; unsigned int providerbytes; unsigned int count; const ManagedLiteralRoot *roots; unsigned int final; unsigned int listresult; } ManagedLiteralCase;']
    cases=rows(limbo,work); roots=[]; finals=[]
    for i,(consumer,provider,values) in enumerate(cases):
        bindings=struct.unpack_from('>I',section(consumer,5))[0]
        records=multi['pool_records'](consumer,section)[3+bindings:]
        roots.append(records)
        code=section(consumer,2)
        final=max(range(len(records)),key=lambda index:code.rfind(RAW['I']('IMOVP',1,source=24+8*index,destination=64)))
        if records[final]!=values: raise RuntimeError('final argument identity differs from independent expectation')
        finals.append(final)
        for label,data in (('c',consumer),('p',provider)):
            output.append('static const uchar literal_'+label+str(i)+'[]={'+','.join(map(str,data))+'};')
        for j,record in enumerate(records):
            output.append('static const unsigned int literal_v'+str(i)+'_'+str(j)+'[]={'+(','.join(str(v)+'U' for v in record) or '0')+'};')
        output.append('static const ManagedLiteralRoot literal_r'+str(i)+'[]={'+','.join('{'+str(len(record))+',literal_v'+str(i)+'_'+str(j)+'}' for j,record in enumerate(records))+'};')
    output.append('static const ManagedLiteralCase literal_cases[]={')
    for i,(consumer,_,_) in enumerate(cases):
        listresult=int(struct.unpack_from('>I',section(consumer,5))[0]==1)
        output.append('{literal_c'+str(i)+',sizeof(literal_c'+str(i)+'),literal_p'+str(i)+',sizeof(literal_p'+str(i)+'),'+str(len(roots[i]))+',literal_r'+str(i)+','+str(finals[i])+','+str(listresult)+'},')
    output.append('};')
    return '\n'.join(output)+'\n'


# Run actual serialized callers through the full scheduler, without an input seed.
def boot(command, work, consumer, provider, pair, limbo, execute, retired):
    cases=rows(limbo,work,'/'+str(provider.relative_to(work.parent)))
    trial=['-X','dis64-managed-trial']
    for image,dependency,_ in cases:
        consumer.write_bytes(image); provider.write_bytes(dependency)
        for jit in (0,1):
            for seed in ([],['-A','ignored external seed 世界']):
                output=execute(command+['-c',str(jit)]+pair+trial+seed)
                if 'native64 scalar module Caller: jit=0, exit=""' not in output or retired not in output or 'panic:' in output:
                    raise RuntimeError('serialized literal did not execute/retire: '+output)
    for malformed in refusals(cases[0][0]):
        consumer.write_bytes(malformed)
        output=execute(command+pair+trial)
        if 'panic: loading Dis64 pair' not in output or retired in output:
            raise RuntimeError('invalid serialized literal reached scheduler: '+output)
    multi=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-multiliteral.py'))
    for malformed in multi['refusals'](next(image for image,_,_ in reversed(cases) if struct.unpack_from('>I',section(image,1),8)[0]==280),RAW,section,replace):
        consumer.write_bytes(malformed)
        output=execute(command+pair+trial)
        if 'panic: loading Dis64 pair' not in output or retired in output:
            raise RuntimeError('invalid multi-root literal reached scheduler: '+output)
    source=work/'void-nil.b'
    path='/'+str(provider.relative_to(work.parent))
    bodies=(
        ('items := lib->unquoted(nil); value := lib->quoted(items);',''),
        ('items := lib->unquoted(""); value := lib->quoted(items);',''),
        ('value := lib->quoted(nil);','dereference of nil'),
        ('seed := ""; alias := seed; seed = nil; items := lib->unquoted(alias); value := lib->quoted(items);',''),
        ('items := lib->unquoted("x"); value := lib->quoted(nil);','dereference of nil'),
    )
    for body,status in bodies:
        source.write_text('implement Caller; include "string.m"; Caller: module {init: fn();}; init(){lib:=load String "'+path+'";'+body+'}')
        subprocess.run([str(limbo.resolve()),'-64l','-I',str(ROOT/'module'),'-o',str(consumer),str(source)],check=True,timeout=30)
        for jit in (0,1):
            output=execute(command+['-c',str(jit)]+pair+trial)
            module='String' if status else 'Caller'
            marker=f'native64 scalar module {module}: jit={jit if status else 0}, exit="{status}"'
            if marker not in output or retired not in output or 'panic:' in output:
                raise RuntimeError('void/nil managed entry changed behavior: '+output)
    # Aggregate/call bounds and ordinary scalar mode remain explicit refusals.
    for flag,body in (('-64l','items := lib->unquoted("a"); '+'items = lib->unquoted("a");'*32),
                      ('-64l','items := lib->unquoted("'+'x'*256+'");'),
                      ('-64','items := lib->unquoted("x");')):
        source.write_text('implement Caller; include "string.m"; Caller: module {init: fn();}; init(){lib:=load String "'+path+'";'+body+'}')
        result=subprocess.run([str(limbo.resolve()),flag,'-I',str(ROOT/'module'),'-o',str(consumer),str(source)],capture_output=True,text=True,timeout=30)
        if result.returncode!=1 or consumer.exists() or 'Dis64' not in result.stderr or any(m in result.stderr for m in ('AddressSanitizer','runtime error:')):
            raise RuntimeError('managed literal bound/mode did not refuse cleanly: '+result.stderr)
    oversized=' '.join('items '+(':=' if i==0 else '=')+' lib->unquoted('+json.dumps('x'*254+str(i))+');' for i in range(5))
    source.write_text('implement Caller; include "string.m"; Caller: module {init: fn();}; init(){lib:=load String '+json.dumps(path)+';'+oversized+'}')
    result=subprocess.run([str(limbo.resolve()),'-64l','-I',str(ROOT/'module'),'-o',str(consumer),str(source)],capture_output=True,text=True,timeout=30)
    if result.returncode!=1 or consumer.exists() or 'aggregate String literal limit' not in result.stderr:
        raise RuntimeError('aggregate literal source bound did not refuse: '+result.stderr)
    source.write_text('implement Caller; include "string.m"; Caller: module {init: fn();}; init(){lib:=load String '+json.dumps(path+'\0suffix')+'; items:=lib->unquoted("x");}')
    result=subprocess.run([str(limbo.resolve()),'-64l','-I',str(ROOT/'module'),'-o',str(consumer),str(source)],capture_output=True,text=True,timeout=30)
    if result.returncode!=1 or consumer.exists() or 'Dis64' not in result.stderr:
        raise RuntimeError('embedded NUL in LOAD path was silently truncated: '+result.stderr)
    print(f'PASS serialized managed literals: {len(cases)} payloads, unseeded/seeded C/JIT requests, void/nil entries, 1/32-call single bindings and single/multi-root pre-scheduler refusals',flush=True)
