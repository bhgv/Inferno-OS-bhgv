#!/usr/bin/env python3
"""Run real LOAD and builtin MCALL through C and executable complete JIT entries."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]  # Owning interpreter tree.
SOURCE=Path(os.environ.get('TRANSITION_SOURCE',ROOT/'libinterp/xec.c')).read_text()


# Borrow real operation bodies without interpreting braces in comments or strings.
def function(signature, source=SOURCE):
    start=source.index(signature);brace=source.index('{',start);depth=0
    pattern=r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern,source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise RuntimeError(signature)


# Use actual VM headers, complete-entry compiler and production C handlers.
def main():
    machine=platform.machine().lower()
    backend='amd64' if machine in ('x86_64','amd64') else 'arm64' if machine in ('aarch64','arm64') else None
    if backend is None:
        print('SKIP native module transitions: requires AMD64 or ARM64')
        return
    prefix=(ROOT/'libinterp/tests/mcall-native.c').read_text().split('#define OP(name)')[0]
    prefix=prefix.replace('comp-amd64.c','comp-'+backend+'.c')
    prefix=prefix.replace('static int callbacks;', 'static int callbacks;\nvoid transitionfreeptrs(void*,Type*); // Actual heap-map cleanup borrowed below.\nstatic int transitionfreecalls; // Observed mapped-cleanup calls at the native boundary.\nstatic int transitionbuiltinmode; // Callback register mutation selected by the oracle.\nstatic uchar *transitionbuiltinfp; // Independent frame identity published by a callback.\n')
    prefix=prefix.replace('(void)frame;(void)type;\n\tabort();','transitionfreecalls++;\n\ttransitionfreeptrs(frame,type);',1)
    prefix=prefix.replace('callbacks++;','callbacks++;\n\tif(transitionbuiltinmode==1)R.FP=transitionbuiltinfp;\n\tif(transitionbuiltinmode==2)D2H(R.M)->ref=0;',1)
    heap=Path(os.environ.get('TRANSITION_HEAP_SOURCE',ROOT/'libinterp/heap.c')).read_text()
    cleanup=function('void\nfreeptrs(',heap).replace('freeptrs(', 'transitionfreeptrs(',1)
    body=(ROOT/'libinterp/tests/module-transition-native.c').read_text()
    parts='\n\n'.join(function(s) for s in ('static Import*\nmoduleimports(','OP(iload)','static int\nmoduleentry(','OP(mcall)','int\ncjitexec('))
    unit='#define TEST_LEAF_DISPATCH_RUNTIME // Use the production complete-entry executor.\n'+prefix+cleanup+'\n'+body.replace('IMPLEMENTATION',parts)
    with tempfile.TemporaryDirectory(prefix='module-transition-native-') as directory:
        td=Path(directory);source=td/'transition.c';source.write_text(unit)
        command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        widths=tuple(map(int,os.environ.get('TRANSITION_WORD_BITS','32 64').split()))
        if not widths or any(bits not in (32,64) for bits in widths):raise ValueError('TRANSITION_WORD_BITS must select 32 and/or 64')
        for bits in widths:
            model=td/str(bits);model.mkdir()
            header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=model/'transition'
            subprocess.run(command+includes+[str(source),'-o',str(binary)],check=True)
            families=[os.environ['TRANSITION_CASE']] if os.environ.get('TRANSITION_CASE') else ['loads','errors','builtins','builtinerrors','mappedbuiltins']
            for family in families:
                routes=['0','1'] if family=='builtinerrors' and not os.environ.get('TRANSITION_ERROR_ROUTE') else [os.environ.get('TRANSITION_ERROR_ROUTE')]
                for route in routes:
                    env=dict(os.environ,TRANSITION_CASE=family)
                    if route is not None:env['TRANSITION_ERROR_ROUTE']=route
                    subprocess.run([str(binary)],env=env,check=True)


# All image, ownership and callback checks remain active with NDEBUG.
if __name__ == '__main__':
    main()
