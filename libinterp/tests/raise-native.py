#!/usr/bin/env python3
"""Execute real RAISE and message conversion through C and a complete native JIT tail."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]  # Owning interpreter tree.
SOURCE=Path(os.environ.get('RAISE_SLOT_SOURCE',ROOT/'libinterp/xec.c')).read_text()


# Borrow actual operation/runtime bodies without interpreting braces in literals.
def function(signature,source=SOURCE):
    start=source.index(signature)
    brace=source.index('{',start)
    depth=0
    pattern=r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern,source[brace:]):
        if token.group()=='{':
            depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


# Build a real executable complete entry while keeping the existing string-cache oracle.
def main():
    machine=platform.machine().lower()
    backend='amd64' if machine in ('amd64','x86_64') else 'arm64' if machine in ('arm64','aarch64') else None
    if backend is None:
        print('SKIP native RAISE: requires AMD64 or ARM64')
        return
    prefix='''#define TEST_LEAF_DISPATCH_RUNTIME // Use the production C executor.
#include "comp-BACKEND.c"
#include "tests/leaf-cons-stub.h"
#include "tests/leaf-plain-stubs.h"
Type Tarray={1}; // Required builtin identity for compiler publication.
int mutator,nprop; // Required mark-barrier globals; RAISE does not alter them.
void (*optab[256])(void); // Actual complete-entry C handler slots.
char exOp[]="opcode"; // Invalid decoded opcode identity.
static Jit *raisejit; // Owned executable table for the selected native backend.


/* Allocate publication metadata independently of message-cache failure injection. */
void*
mallocz(ulong size,int clear)
{
    void *value=malloc(size);
    if(value && clear)memset(value,0,size);
    return value;
}


/* Compile valid metadata through the production protected-entry contract. */
int
kprotect(int (*callback)(void*),void *argument)
{
    return callback(argument);
}


/* RAISE leaves the native mapping before invoking its ordinary C handler. */
static void
raiseentry(void)
{
    if(atoi(getenv("RAISE_NATIVE_ROUTE"))==0)optab[IRAISE]();
    else raisejit->dispatch[IRAISE](&R);
}
'''.replace('BACKEND',backend)
    fixture=(ROOT/'libinterp/tests/raise-slots.c').read_text()
    fixture=re.sub(r'^#include \"(?:dat|fns|isa|interp|raise)\.h\"\n','',fixture,flags=re.M)
    fixture=fixture.replace('main(void)','raisebaseline(void)',1).replace('iraise(); abort();','raiseentry(); abort();')
    implementation=function('char*\nstring2c(',(ROOT/'libinterp/string.c').read_text())
    implementation+='\n\n'+function('OP(iraise)')+'\n\n'+function('int\ncjitexec(')
    unit=prefix+fixture.replace('IMPLEMENTATION',implementation)
    unit+=(ROOT/'libinterp/tests/raise-native.c').read_text()
    widths=tuple(map(int,os.environ.get('RAISE_NATIVE_WORD_BITS','32 64').split()))
    if not widths or any(bits not in (32,64) for bits in widths):
        raise ValueError('RAISE_NATIVE_WORD_BITS must select 32 and/or 64')
    with tempfile.TemporaryDirectory(prefix='raise-native-') as directory:
        td=Path(directory)
        target=td/'raise.c';target.write_text(unit)
        command=shlex.split(os.environ.get('CC','cc'))
        command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        for bits in widths:
            model=td/str(bits);model.mkdir()
            header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=model/'raise'
            subprocess.run(command+includes+[str(target),str(ROOT/'lib9/rune.c'),'-o',str(binary)],check=True)
            routes=[os.environ['RAISE_NATIVE_ROUTE']] if os.environ.get('RAISE_NATIVE_ROUTE') else ['0','1']
            families=[os.environ['RAISE_NATIVE_CASE']] if os.environ.get('RAISE_NATIVE_CASE') else ['baseline','spans']
            for route in routes:
                for family in families:
                    subprocess.run([str(binary)],env=dict(os.environ,RAISE_NATIVE_ROUTE=route,RAISE_NATIVE_CASE=family),check=True)


# All native fixture checks remain active with NDEBUG.
if __name__=='__main__':
    main()
