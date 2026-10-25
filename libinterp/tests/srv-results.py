#!/usr/bin/env python3
"""Exercise actual Srv return slots and error ownership with native resolver adapters."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]


def function(text, signature):
    """Extract C functions without treating literal or comment braces as code."""
    start=text.index(signature);brace=text.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',text[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return text[start:brace+token.end()]
    raise ValueError(signature)


srv=Path(os.environ.get('SRV_SOURCE',ROOT/'emu/port/srv.c')).read_text()
parts=['static QLock dbq;enum { SrvMaxHosts=10 };']
for signature in ('static void\nsrvclear(', 'static void\nsrvfreehosts(', 'static void\nsrvhostlist('):
    if signature in srv:parts.append(function(srv,signature))
parts += [function(srv,'void\nSrv_'+name+'(') for name in ('iph2a','ipa2h','ipn2p')]
fixture=(ROOT/'libinterp/tests/srv-results.c').read_text()
# Allocation and exception functions run with their actual production implementations.
prefix=function((ROOT/'emu/port/dis.c').read_text(),'int\nkprotect(')+'\n'+function((ROOT/'libinterp/xec.c').read_text(),'List*\ncons(')+'\n'+function((ROOT/'libinterp/string.c').read_text(),'String*\nnewstring(')
unit=fixture.replace('#define malloc srvfixturemalloc',prefix+'\n#define malloc srvfixturemalloc').replace('IMPLEMENTATION','\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='srv-results-') as directory:
    td=Path(directory);target=td/'results.c';target.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64,32):
        model=td/str(bits);model.mkdir();header=(ROOT/'include/interp.h').read_text()
        if bits==32:
            header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
            header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
        (model/'interp.h').write_text(header)
        includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','emu/Android')]
        binary=td/('results'+str(bits))
        subprocess.run(command+includes+[str(target),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
