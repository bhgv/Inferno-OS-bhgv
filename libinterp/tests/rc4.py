#!/usr/bin/env python3
"""Check actual RC4 builtin frames and native streams against a Python oracle."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
keys=[b'\0',b'Key',bytes(range(1,6)),bytes(range(16)),bytes(range(256))]


# Generate independent reference streams, including the RFC 6229 five-byte key.
def reference(key,size):
    state=list(range(256));j=0
    for i in range(256):
        j=(j+state[i]+key[i%len(key)])%256
        state[i],state[j]=state[j],state[i]
    i=j=0;result=[]
    for _ in range(size):
        i=(i+1)%256;j=(j+state[i])%256
        state[i],state[j]=state[j],state[i]
        result.append(state[(state[i]+state[j])%256])
    return bytes(result)


assert reference(keys[2],16).hex()=='b2396305f03dc027ccc3524a0a1118a8'
vectors='static int key_sizes[]={'+','.join(str(len(k)) for k in keys)+'};\n'
vectors+='static uchar keys[5][256]={'+','.join('{'+','.join(map(str,k))+'}' for k in keys)+'};\n'
vectors+='static uchar streams[5][544]={'+','.join('{'+','.join(map(str,reference(k,544)))+'}' for k in keys)+'};\n'


# Read a complete helper, ignoring braces in comments and string literals.
def function(source,signature):
    start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)


with tempfile.TemporaryDirectory(prefix='rc4-wrappers-') as directory:
    td=Path(directory)
    includes=['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp','libmp/port')]
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for module in os.environ.get('RC4_MODULES','Keyring Crypt').split():
        name=module.lower();source=Path(os.environ.get('RC4_SOURCE',root/('libinterp/'+name+'.c'))).read_text()
        signature='static WORD\n'+name+'_bytearray('
        helper=function(source,signature) if signature in source else ''
        a=source.index('void\n'+module+'_rc4setup(');b=source.index('/*\n *  public/secret keys',a)
        dispatch=f'if(operation==0) {{ F_{module}_rc4setup f={{0}};f.ret=&result;f.seed=seed;{module}_rc4setup(&f);}}'
        dispatch+=f'else if(operation==1) {{F_{module}_rc4 f={{0}};f.state=state;f.buf=input;f.n=count;{module}_rc4(&f);}}'
        for i,fn in [(2,'rc4skip'),(3,'rc4back')]:
            dispatch+=f'else if(operation=={i}) {{F_{module}_{fn} f={{0}};f.state=state;f.n=count;{module}_{fn}(&f);}}'
        unit=(root/'libinterp/tests/rc4.c').read_text()
        unit=unit.replace('HEADER','keyringif.h' if module=='Keyring' else 'runt.h')
        unit=unit.replace('IMPLEMENTATION',helper+'\n'+source[a:b]).replace('DISPATCH',dispatch)
        unit=unit.replace('VECTORS\n',vectors).replace('KEYRING','1' if module=='Keyring' else '0').replace('MOD',module)
        target=td/(name+'.c');target.write_text(unit)
        for model in ('native','word32'):
            override=[]
            if model=='word32':
                modeldir=td/(name+'-word32');modeldir.mkdir()
                header=(root/'include/interp.h').read_text()
                header,n=re.subn(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header,m=re.subn(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
                assert n==m==1
                (modeldir/'interp.h').write_text(header);override=['-I'+str(modeldir)]
            binary=td/(name+'-run-'+model)
            native=Path(os.environ.get('RC4_NATIVE_SOURCE',root/'libsec/port/rc4.c'))
            subprocess.run(cmd+override+includes+[str(target),str(native),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=30)
