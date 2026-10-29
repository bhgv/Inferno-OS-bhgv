#!/usr/bin/env python3
"""Exercise real FRAME/MFRAME, initmem and complete AMD64 fast-plus-C entries."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]  # Owning interpreter source tree.
SOURCE=Path(os.environ.get('FRAME_ALLOCATION_SOURCE',ROOT/'libinterp/xec.c')).read_text()
if platform.machine().lower() not in ('amd64','x86_64'):
    print('SKIP native frame allocation: requires AMD64')
    sys.exit(0)


# Borrow one complete production definition while ignoring comment and literal braces.
def function(source,signature):
    start=source.index(signature);brace=source.index('{',start);depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise RuntimeError(signature)


prefix=(ROOT/'libinterp/tests/frame-native.c').read_text().split('#define OP(name)')[0]
realinit=function((ROOT/'libinterp/heap.c').read_text(),'void\ninitmem(')
realinit=realinit.replace('\tx = H;','\tinitialized++;\n\tx = H;',1)
prefix=prefix.replace(function(prefix,'void\ninitmem('),realinit)
unit='#define TEST_LEAF_DISPATCH_RUNTIME // Link the real complete-entry executor.\n'+prefix
unit+='\nvoid (*optab[256])(void);\nchar exOp[]="opcode";\n#define OP(name) void name(void) // Production decoded instruction entry.\n'
unit+='\n\n'.join(function(SOURCE,s) for s in ('static Type*\nmoduletype(','static int\nmoduleentry(','static int\nframefits(','OP(frame)','OP(mframe)','int\ncjitexec('))
unit+=(ROOT/'libinterp/tests/frame-allocation-native.c').read_text()
with tempfile.TemporaryDirectory(prefix='frame-allocation-native-') as temporary:
    td=Path(temporary);source=td/'frames.c';source.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (32,64):
        model=td/str(bits);model.mkdir()
        header=(ROOT/'include/interp.h').read_text()
        if bits==32:
            header = '#define DIS_WORD32 1 // Select the production experimental scalar model.\n'+header
        (model/'interp.h').write_text(header)
        includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary=model/'frames'
        subprocess.run(command+includes+[str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=120)
