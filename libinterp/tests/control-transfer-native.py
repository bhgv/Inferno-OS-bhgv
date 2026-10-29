#!/usr/bin/env python3
"""Check real CALL/MCALL/RET and complete AMD64 fast-plus-C entries."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning VM source tree.
SOURCE = Path(os.environ.get('CONTROL_SOURCE',ROOT/'libinterp/xec.c')).read_text()
machine=platform.machine().lower()
backend='amd64' if machine in ('amd64','x86_64') else 'arm64' if machine in ('aarch64','arm64') else None
if backend is None:
    print('SKIP native control transfers: requires AMD64 or ARM64')
    sys.exit(0)


# Extract a complete real definition without counting braces in literals or comments.
def function(signature):
    start=SOURCE.index(signature)
    brace=SOURCE.index('{',start)
    depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]',SOURCE[brace:]):
        if token.group()=='{': depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0: return SOURCE[start:brace+token.end()]
    raise RuntimeError(signature)


prefix=(ROOT/'libinterp/tests/mcall-native.c').read_text().split('#define OP(name)')[0]
prefix=prefix.replace('comp-amd64.c','comp-'+backend+'.c')
unit='#define TEST_LEAF_DISPATCH_RUNTIME // Link the real decoded executor.\n'+prefix
unit+='''void (*optab[256])(void);
char exOp[]="opcode";
#define OP(name) void name(void) // Real interpreter instruction entry.


/* Only pointer-free, retained module owners reach this fixture destructor. */
void destroy(void *owner)
{
    Heap *header=D2H(owner);
    ulong references;
    memmove(&references,(uchar*)header+O(Heap,ref),sizeof(references));
    if(references<2)abort();
    references--;
    memmove((uchar*)header+O(Heap,ref),&references,sizeof(references));
}
'''
unit+='\n\n'.join(function(s) for s in ('static int\nmoduleentry(','OP(call)','OP(mcall)','OP(ret)','int\ncjitexec('))
unit+=(ROOT/'libinterp/tests/control-transfer-native.c').read_text()
with tempfile.TemporaryDirectory(prefix='control-transfer-native-') as temporary:
    td=Path(temporary)
    source=td/'control.c'
    source.write_text(unit)
    command=shlex.split(os.environ.get('CC','cc'))+shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    widths=tuple(map(int,os.environ.get('CONTROL_WORD_BITS','32 64').split()))
    if not widths or any(bits not in (32,64) for bits in widths):
        raise ValueError('CONTROL_WORD_BITS must select 32 and/or 64')
    for bits in widths:
        model=td/str(bits)
        model.mkdir()
        header=(ROOT/'include/interp.h').read_text()
        if bits==32:
            header = '#define DIS_WORD32 1 // Select the production experimental scalar model.\n'+header
        (model/'interp.h').write_text(header)
        includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary=model/'control'
        subprocess.run(command+includes+[str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=120)
