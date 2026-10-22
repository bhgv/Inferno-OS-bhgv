#!/usr/bin/env python3
"""Run both actual-header digest modules with production hashes and Python vectors."""
import hashlib
import hmac
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
algorithms=['sha1','sha224','sha256','sha384','sha512','md5','md4','hmac_sha1','hmac_md5']
sizes=[3,0,1,55,56,63,64,65,127,128,129]
data=bytes((i*17+3)&255 for i in range(129))
expected=[]
for algo in algorithms:
    row=[]
    for i,n in enumerate(sizes):
        payload=b'abc' if i==0 else data[:n]
        if algo=='md4': digest=bytes.fromhex('a448017aaf21d8525fc10ae87aa6729d' if i==0 else '31d6cfe0d16ae931b73c59d7e0c089c0') if i<=1 else bytes(16)
        elif algo.startswith('hmac_'):
            key=b'' if i and i%3==0 else data[:64] if i%3==1 else b'key'
            digest=hmac.new(key,payload,algo[5:]).digest()
        else: digest=hashlib.new(algo,payload).digest()
        row.append('{'+','.join(str(b) for b in digest)+'}')
    expected.append('{'+','.join(row)+'}')
vectors='enum {NVECTORS='+str(len(sizes))+'};\n'
vectors+='static int vector_sizes[]={'+','.join(map(str,sizes))+'};\n'
vectors+='static uchar expected[9][NVECTORS][64]={'+','.join(expected)+'};\n'

with tempfile.TemporaryDirectory(prefix='digest-wrappers-') as directory:
    td=Path(directory)
    includes=['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
    compiler=shlex.split(os.environ.get('CC','cc'))
    flags=shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    units=[]
    for name in ['sha1','sha1block','sha2','sha256block','sha512block','md4','md5','md5block','hmac']:
        sourcepath=Path(os.environ.get('HMAC_SOURCE',root/'libsec/port/hmac.c')) if name=='hmac' else root/('libsec/port/'+name+'.c')
        text=re.sub(r'^#include[^\n]*\n','',sourcepath.read_text(),flags=re.M)
        target=td/(name+'.c')
        target.write_text('#include "lib9.h"\n#include "libsec.h"\nvoid *nativealloc(size_t); void nativefree(void*);\n#define malloc nativealloc\n#define free nativefree\n'+text)
        units.append(str(target))
    for module in os.environ.get('DIGEST_MODULES','Keyring Crypt').split():
        name=module.lower()
        sourcepath=Path(os.environ.get('DIGEST_SOURCE',root/('libinterp/'+name+'.c')))
        source=sourcepath.read_text()
        copy='Keyring_DigestState_copy' if module=='Keyring' else 'DigestState_copy'
        a=source.index('void\n'+copy+'(')
        b=source.index('\nvoid\n'+module+'_dhparams(',a)
        implementation=source[a:b]
        header='keyringif.h' if module=='Keyring' else 'runt.h'
        dispatch=[]
        for i,algo in enumerate(algorithms):
            fields='f.data=input;f.key=key;' if algo.startswith('hmac_') else 'f.buf=input;'
            dispatch.append(f'case {i}: {{ F_{module}_{algo} f={{0}};f.ret=&result;{fields}f.n=count;f.digest=output;f.state=state;{module}_{algo}(&f);break;}}')
        dispatch.append(f'case 9: {{F_DigestState_copy f={{0}};f.ret=&result;f.d=state;{copy}(&f);break;}}')
        unit=(root/'libinterp/tests/digests.c').read_text().replace('HEADER',header)
        unit=unit.replace('IMPLEMENTATION',implementation).replace('DISPATCH','\n'.join(dispatch)).replace('VECTORS\n',vectors).replace('MOD',module)
        target=td/(name+'-test.c');target.write_text(unit)
        for model in ('native','word32'):
            override=[]
            if model=='word32':
                modeldir=td/(name+'-word32');modeldir.mkdir()
                text=(root/'include/interp.h').read_text()
                text,n=re.subn(r'typedef long\s+WORD;','typedef int WORD;',text,count=1)
                text,m=re.subn(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',text,count=1)
                assert n==m==1
                (modeldir/'interp.h').write_text(text);override=['-I'+str(modeldir)]
            binary=td/(name+'-run-'+model)
            subprocess.run(compiler+flags+override+includes+[str(target)]+units+['-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
