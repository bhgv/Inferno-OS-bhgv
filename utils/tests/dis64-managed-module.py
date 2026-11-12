#!/usr/bin/env python3
"""Named managed interfaces: generated ABI witnesses, byte oracles and real file boot."""
from pathlib import Path
import json
import os
import shlex
import re
import runpy
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[2]
LITERAL=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-literal.py'))
MULTI=runpy.run_path(str(ROOT/'utils/tests/dis64-managed-multiliteral.py'))
RAW=LITERAL['RAW']; section=LITERAL['section']; replace=LITERAL['replace']


# Capture real compiler failures with the source that produced them.
def compile_file(limbo,work,code,flag,output):
    source=work/'named-module.b'; source.write_text(code)
    result=subprocess.run([str(limbo.resolve()),flag,'-I',str(work),'-I',str(ROOT/'module'),'-o',str(output),str(source)],capture_output=True,text=True,timeout=30)
    if result.returncode: raise RuntimeError('named managed source failed: '+code+'\n'+result.stdout+result.stderr)
    return output.read_bytes()


# Generate a user interface and compare its actual -H layout/hash with the canonical types.
def interface(limbo,work,module,wrap,unwrap):
    path=work/'named.m'
    text=module+': module {\n'+wrap+': fn(payload: string): list of string;\n'+unwrap+': fn(items: list of string): string;\n};\n'
    path.write_text(text)
    header=subprocess.check_output([str(limbo.resolve()),'-H','-M',module,str(path)],text=True,timeout=30)
    canonical=subprocess.check_output([str(limbo.resolve()),'-H','-M','String',str(ROOT/'module/string.m')],text=True,timeout=30)
    for name,reference in ((wrap,'unquoted'),(unwrap,'quoted')):
        value=re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_'+re.escape(module+'_'+name)+r' (0x[0-9a-f]+)U',header,re.M)
        expected=re.search(r'^#define LIMBO_NATIVE64_SIGNATURE_String_'+reference+r' (0x[0-9a-f]+)U',canonical,re.M)
        if not value or not expected or value[1]!=expected[1]: raise RuntimeError('generated structural hash differs')
        if not re.search(r'^#define F_'+re.escape(module+'_'+name)+r'_size 72\b',header,re.M): raise RuntimeError('generated named frame differs')
    generated=work/'named-abi.h'; generated.write_text(header)
    probe=work/'named-abi.c'
    checks=[]
    for name,field in ((wrap,'payload'),(unwrap,'items')):
        frame='F_'+module+'_'+name
        checks.append('typedef char size_'+field+'[sizeof('+frame+')==72 ? 1 : -1];')
        checks.append('typedef char offset_'+field+'[offsetof('+frame+','+field+')==64 ? 1 : -1];')
        if not re.search(r'^#define '+re.escape(frame)+r'_map \{0x00,0x80\}',header,re.M): raise RuntimeError('generated named map differs')
    probe.write_text('#include "lib9.h"\n#include "isa.h"\n#include "interp.h"\n#include "named-abi.h"\n'+'\n'.join(checks)+'\n')
    subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-DDIS_WORD32=1','-I'+str(ROOT/'Linux/386/include'),'-I'+str(ROOT/'include'),'-I'+str(work),'-c',str(probe),'-o',str(work/'named-abi.o')],check=True,timeout=30)
    return text


# Providers are ordinary source bodies; export order and local spelling are independently varied.
def provider_source(module,wrap,unwrap,reverse=False,locals=False):
    a=('copy := payload; payload = nil; return copy :: nil;' if locals else 'return payload :: nil;')
    b=('copy := items; items = nil; value := hd copy; return value;' if locals else 'return hd items;')
    definitions=[wrap+'(payload: string): list of string {'+a+'}',unwrap+'(items: list of string): string {'+b+'}']
    return 'implement '+module+'; include "named.m";\n'+'\n'.join(definitions[::-1] if reverse else definitions)


# Map source method names to import ordinals, verifying complete structural records.
def import_symbols(image,module,wrap,unwrap):
    records=MULTI['pool_records'](image,section); data=section(image,5); at=4; result={}
    expected={wrap:RAW['W'](0xface0010,1,0x72,0x71,0x71),unwrap:RAW['W'](0xface0010,1,0x71,0x72,0x71)}
    for i in range(struct.unpack_from('>I',data)[0]):
        owner,symbol,size=struct.unpack_from('>III',data,at)
        name=''.join(map(chr,records[symbol]))
        if records[owner]!=tuple(map(ord,module)) or name not in expected or data[at+12:at+12+size]!=expected[name]: raise RuntimeError('named import identity/type changed')
        result[name]=i; at+=12+size
    return result


# Compile arbitrary names, reversed method spelling, export order and the maximal metadata case.
def rows(limbo,work,path='/managed-provider.dis64'):
    result=[]; output=work/'named.dis64'
    variants=[('Box','pack','unpack','Caller',False,False),('Other','zpack','ahead','Caller',True,True),
              ('String','quoted','unquoted','Caller',True,False),
              ('M'+'x'*254,'w'+'x'*254,'u'+'x'*254,'C'+'x'*254,False,True)]
    for module,wrap,unwrap,caller,reverse,locals in variants:
        interface(limbo,work,module,wrap,unwrap)
        provider=compile_file(limbo,work,provider_source(module,wrap,unwrap,reverse,locals),'-64m',output)
        values=['first\0value','Grüße 世界'] if len(module)<255 else [f'{i:02d}:'+'x'*61 for i in range(16)]
        body=f'lib := load {module} {json.dumps(path)}; '
        calls=[]; reloads=set()
        for i,value in enumerate(values):
            if i: body+=f'lib = load {module} {json.dumps(path)}; '; reloads.add(2*i)
            body+=('items := ' if i==0 else 'items = ')+f'lib->{wrap}({json.dumps(value,ensure_ascii=False)}); '
            body+=('text := ' if i==0 else 'text = ')+f'lib->{unwrap}(items); '
            calls.extend(((wrap,value),(unwrap,None)))
        source='implement '+caller+'; include "named.m"; '+caller+': module {init: fn();}; init(){'+body+'}'
        image=compile_file(limbo,work,source,'-64l',output)
        symbols=import_symbols(image,module,wrap,unwrap); records=MULTI['pool_records'](image,section)
        roots=records[3+len(symbols):]; rootids={value:i for i,value in enumerate(roots)}
        if set(roots)!={tuple(map(ord,v)) for v in values}: raise RuntimeError('named literal roots changed')
        expected=MULTI['expected_code'](RAW,calls,rootids,symbols,reloads)
        if section(image,2)!=expected: raise RuntimeError('named CODE changed method/argument order')
        if len(module)==255 and len(section(image,7))<=8192: raise RuntimeError('large name fixture no longer exercises pool growth')
        result.append((image,provider,tuple(map(ord,values[-1]))))
    return result


# Input-only entries exercise both seed kinds, including intentionally reversed familiar names.
def seeds(limbo,work,path='/managed-provider.dis64'):
    result=[]; output=work/'seed-named.dis64'
    for module,wrap,unwrap in (('Box','pack','unpack'),('Reversed','quoted','unquoted')):
        interface(limbo,work,module,wrap,unwrap)
        provider=compile_file(limbo,work,provider_source(module,wrap,unwrap),'-64m',output)
        for islist,name in ((0,wrap),(1,unwrap)):
            kind='list of string' if islist else 'string'
            code='implement Caller; include "named.m"; Caller: module {init: fn(value: '+kind+');}; init(value: '+kind+'){ lib:=load '+module+' '+json.dumps(path)+'; result:=lib->'+name+'(value);}'
            image=compile_file(limbo,work,code,'-64l',output); import_symbols(image,module,wrap,unwrap)
            result.append((image,provider,islist,name,module))
    return result


# Emit C inputs for the actual seed routine, with independently expected heap classes/names.
def seed_header(limbo,work):
    output=['typedef struct NamedSeedCase { const uchar *consumer; unsigned int consumerbytes; const uchar *provider; unsigned int providerbytes; int islist; const char *name; } NamedSeedCase;']
    cases=seeds(limbo,work)
    for i,(consumer,provider,_,_,_) in enumerate(cases):
        for name,data in (('c',consumer),('p',provider)): output.append('static const uchar named_'+name+str(i)+'[]={'+','.join(map(str,data))+'};')
    output.append('static const NamedSeedCase named_seeds[]={')
    for i,(_,_,islist,name,_) in enumerate(cases): output.append('{named_c'+str(i)+',sizeof(named_c'+str(i)+'),named_p'+str(i)+',sizeof(named_p'+str(i)+'),'+str(islist)+','+json.dumps(name)+'},')
    output.append('};'); return '\n'.join(output)+'\n'


# Rename one pool record without touching the checked code, maps or signatures.
def rename(image,index,text):
    records=MULTI['pool_records'](image,section); records[index]=tuple(map(ord,text))
    pool=RAW['W'](len(records))+b''.join(RAW['W'](len(record),*record) for record in records)
    return replace(image,7,pool)


# Execute named modules and ensure same-shape names never bypass exact identity matching.
def boot(command,work,consumer,provider,pair,limbo,execute,retired):
    path='/'+str(provider.relative_to(work.parent)); cases=rows(limbo,work,path); trial=['-X','dis64-managed-trial']
    for image,dependency,_ in cases:
        records=MULTI['pool_records'](image,section); caller=''.join(map(chr,records[2]))
        consumer.write_bytes(image); provider.write_bytes(dependency)
        for jit in (0,1):
            for seed in ([],['-A','ignored external seed']):
                output=execute(command+['-c',str(jit)]+pair+trial+seed)
                if f'native64 scalar module {caller}: jit=0, exit=""' not in output or retired not in output or 'panic:' in output: raise RuntimeError('named managed caller failed: '+output)
    for image,dependency,islist,name,module in seeds(limbo,work,path):
        consumer.write_bytes(image); provider.write_bytes(dependency)
        for jit in (0,1):
            for seed in ([],['-A','seed 世界']):
                output=execute(command+['-c',str(jit)]+pair+trial+seed)
                failure=bool(islist and not seed); owner=module if failure else 'Caller'; status='dereference of nil' if failure else ''
                marker=f'native64 scalar module {owner}: jit={jit if failure else 0}, exit="{status}"'
                if marker not in output or retired not in output or 'panic:' in output: raise RuntimeError('typed named seed failed: '+name+' '+output)
    image,dependency,_=cases[0]
    # These are individually well-formed providers; their identities mismatch the caller.
    for bad in (rename(dependency,0,'WrongOwner'),rename(dependency,1,'wrong_export'),rename(dependency,1,'bad\0suffix'),rename(dependency,1,'λ')):
        consumer.write_bytes(image); provider.write_bytes(bad)
        output=execute(command+pair+trial)
        if 'panic: loading Dis64 pair' not in output or retired in output: raise RuntimeError('named mismatch reached scheduling: '+output)
    for module,wrap,unwrap in (('M'+'x'*255,'wrap','unwrap'),('Box','w'+'x'*255,'unwrap')):
        interface_path=work/'named.m'; interface_path.write_text(module+': module {'+wrap+': fn(value: string): list of string;'+unwrap+': fn(value: list of string): string;};')
        source=work/'bad-name.b'; source.write_text(provider_source(module,wrap,unwrap))
        result=subprocess.run([str(limbo.resolve()),'-I',str(work),'-64m','-o',str(provider),str(source)],capture_output=True,text=True,timeout=30)
        if result.returncode!=1 or provider.exists() or 'Dis64' not in result.stderr: raise RuntimeError('oversized managed name did not refuse cleanly: '+result.stderr)
    print('PASS named managed modules: generated hashes/layouts, CODE oracles, 255-character names, typed seeds and exact identity refusals',flush=True)
