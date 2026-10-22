#!/usr/bin/env python3
"""Exercise production AES wrappers/CBC against independent ECB-based vectors."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
from cryptography.hazmat.primitives.ciphers import Cipher,algorithms,modes

root=Path(__file__).resolve().parents[2]
keys=[bytes(range(n)) for n in (16,24,32)]
plain=bytes((i*13+7)&255 for i in range(129))
iv=bytes(range(16))


# Model the repository's partial-tail convention using an independent AES primitive.
def expected(key,data):
    encrypt=Cipher(algorithms.AES(key),modes.ECB()).encryptor()
    chain=iv;result=b''
    while len(data)>=16:
        chain=encrypt.update(bytes(a^b for a,b in zip(data[:16],chain)))
        result+=chain;data=data[16:]
    if data:
        chain=encrypt.update(chain)
        result+=bytes(a^b for a,b in zip(data,chain))
    encrypt.finalize()
    return result,chain


enc=Cipher(algorithms.AES(keys[0]),modes.ECB()).encryptor()
assert enc.update(bytes.fromhex('00112233445566778899aabbccddeeff')).hex()=='69c4e0d86a7b0430d8cdb78070b4c55a'


# Serialize independent vector bytes into the temporary C fixture, not the repository.
def array(data):
    return '{'+','.join(map(str,data))+'}'


vectors='static int key_sizes[]={16,24,32};\nstatic uchar keys[3][32]={'+','.join(array(k) for k in keys)+'};\n'
vectors+='static uchar ciphertext[3][130][129]={'+','.join('{'+','.join(array(expected(k,plain[:n])[0]) for n in range(130))+'}' for k in keys)+'};\n'
vectors+='static uchar finaliv[3][130][16]={'+','.join('{'+','.join(array(expected(k,plain[:n])[1]) for n in range(130))+'}' for k in keys)+'};\n'


# Extract complete helpers without counting comment/literal braces.
def function(source,signature):
    start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[start:brace+token.end()]
    raise ValueError(signature)


with tempfile.TemporaryDirectory(prefix='aes-wrappers-') as directory:
    td=Path(directory)
    includes=['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp','libmp/port')]
    cmd=shlex.split(os.environ.get('CC','cc'))
    cmd+=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for module in os.environ.get('AES_MODULES','Keyring Crypt').split():
        name=module.lower();source=Path(os.environ.get('AES_SOURCE',root/('libinterp/'+name+'.c'))).read_text()
        helper=function(source,'static WORD\n'+name+'_bytearray(')
        a=source.index('void\n'+module+'_aessetup(');b=source.index('void\n'+module+'_blowfishsetup(',a)
        dispatch=f'if(operation==0) {{F_{module}_aessetup f={{0}};f.ret=&result;f.key=key;f.ivec=iv;{module}_aessetup(&f);}}'
        dispatch+=f'else {{F_{module}_aescbc f={{0}};f.state=state;f.buf=input;f.n=count;f.direction=direction;{module}_aescbc(&f);}}'
        unit=(root/'libinterp/tests/aes.c').read_text().replace('HEADER','keyringif.h' if module=='Keyring' else 'runt.h')
        unit=unit.replace('IMPLEMENTATION',helper+'\n'+source[a:b]).replace('DISPATCH',dispatch).replace('VECTORS\n',vectors).replace('MOD',module)
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
            native=Path(os.environ.get('AES_NATIVE_SOURCE',root/'libsec/port/aes.c'))
            if os.environ.get('AES_COMPACT'):
                compact=td/'compact.c'
                compact.write_text(native.read_text().replace('#define FULL_UNROLL','#undef FULL_UNROLL'))
                native=compact
            subprocess.run(cmd+override+includes+[str(target),str(native),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
