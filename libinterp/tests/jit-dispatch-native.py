#!/usr/bin/env python3
"""Test complete tail-dispatch through real AMD64 code, C executor and VM loop."""
import importlib.util
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning repository.
SOURCE = Path(os.environ.get('BRIDGE_XEC_SOURCE',ROOT/'libinterp/xec.c')).read_text()  # Production decoded execution and handlers.
if platform.machine().lower() not in ('amd64','x86_64'):
    print('SKIP native tail-dispatch: requires AMD64')
    sys.exit(0)


# Extract one complete borrowed C definition, excluding comment/literal braces.
def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{',start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',SOURCE[brace:]):
        if token.group()=='{': depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0: return SOURCE[start:brace+token.end()]
    raise RuntimeError(signature)


spec = importlib.util.spec_from_file_location('coverage',ROOT/'libinterp/tests/jit-coverage.py')
coverage = importlib.util.module_from_spec(spec)
exec(compile((ROOT/'libinterp/tests/jit-coverage.py').read_bytes(),
             str(ROOT/'libinterp/tests/jit-coverage.py'), 'exec'), coverage.__dict__)
names = coverage.opnames()
callbacks = '\n\n\n'.join('/* Identify the exact optab slot selected for '+name+'. */\nstatic void\nbridgeop'+str(i)+'(void)\n{\n    bridgerecord('+str(i)+');\n}' for i,name in enumerate(names))
callbacks += '\n\nstatic void (*bridgehandlers[])(void) = {'+','.join('bridgeop'+str(i) for i in range(len(names)))+'};\n'
parts = [function(s) for s in ('static Type*\nmoduletype(','static int\nmappedcopytype(','static List*\nlisthead(','OP(movmp)','OP(headmp)','OP(addw)','OP(divw)','int\ncjitexec(','void\nxec(')]
unit = '#define TEST_LEAF_DISPATCH_RUNTIME // Link the real shared executor.\nstatic unsigned long bridgeallocations;\n'+(ROOT/'libinterp/tests/mapped-copy-native.c').read_text()
unit = unit.replace('main(void)','mappedlegacy(void)',1).replace('p = malloc(n);','bridgeallocations++;\n\tp = malloc(n);',1)
prelude = '''void (*optab[256])(void);
void (*dec[256])(void);
char exOp[]="opcode", exZdiv[]="divide", bridgeexception[]="bridge exception";


/* Each generated callback reports its own real opcode identity. */
static void bridgerecord(int);
'''
unit = unit.replace('IMPLEMENTATION',prelude+callbacks+'\n\n'+'\n\n'.join(parts))
unit += (ROOT/'libinterp/tests/jit-dispatch-native.c').read_text()
backend = os.environ.get('BRIDGE_BACKEND','amd64')
if backend not in ('amd64','arm64'): raise ValueError(backend)
if backend!='amd64':
    if not os.environ.get('BRIDGE_PREFLIGHT_ONLY'):
        raise RuntimeError('foreign compiler fixture must not execute guest code on the AMD64 host')
    unit = unit.replace('comp-amd64.c','comp-'+backend+'.c')
with tempfile.TemporaryDirectory(prefix='jit-dispatch-native-') as temporary:
    td = Path(temporary)
    if os.environ.get('BRIDGE_COMPILER_SOURCE'):
        (td/'parent-compiler.h').write_text(Path(os.environ['BRIDGE_COMPILER_SOURCE']).read_text())
        prefix = '#define LEAF_EMITTER "'+backend+'-emit.h" // Select current emitter declarations for parent replay.\n'
        (td/('comp-'+backend+'.c')).write_text(prefix+'#include "parent-compiler.h"\n')
    source = td/'fixture.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    widths = tuple(map(int, os.environ.get("BRIDGE_WORD_BITS", "32 64").split()))
    if not widths or any(width not in (32,64) for width in widths):
        raise ValueError("BRIDGE_WORD_BITS must select 32 and/or 64")
    for wb in widths:
        model = td/str(wb)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wb==32:
            header = re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('word'+str(wb))
        subprocess.run(command+includes+[str(source),'-lm','-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
