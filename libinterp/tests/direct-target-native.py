#!/usr/bin/env python3
"""Run all direct branch families through actual C and complete native entries."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning VM tree.
SOURCE = Path(os.environ.get('DIRECT_TARGET_SOURCE', ROOT/'libinterp/xec.c')).read_text()
NAMES = tuple('b'+relation+kind for kind in ('b','w','l','f','c')
              for relation in ('eq','ne','lt','le','gt','ge'))+('jmp',)  # All 31 direct transfers.


# Borrow complete production definitions without counting braces in literals.
def function(signature, source=SOURCE):
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


# Retain native pointers while independently selecting VM scalar widths.
def main():
    machine=platform.machine().lower()
    backend='amd64' if machine in ('x86_64','amd64') else 'arm64' if machine in ('aarch64','arm64') else None
    if backend is None:
        print('SKIP native direct targets: requires AMD64 or ARM64')
        return
    prefix=(ROOT/'libinterp/tests/pc-tables-native.c').read_text().split('#define OP(name)')[0]
    prefix=prefix.replace('comp-amd64.c','comp-'+backend+'.c')
    declarations='''
void (*optab[256])(void); // Actual complete-entry C handler slots.
char exOp[]="opcode",exHeap[]="heap",exNilref[]="nil"; // Production failure identities.
#define OP(name) void name(void) // Borrow actual interpreter operations.
'''
    parts=[next(line for line in SOURCE.splitlines() if line.startswith('#define JMP('))]
    if 'static void\nbranchpc(' in SOURCE:
        parts.append(function('static void\nbranchpc('))
    parts.append(function('int\nstringcmp(', (ROOT/'libinterp/string.c').read_text()))
    parts += [function('static void\n'+name+'(') for name in ('branchreal','branchstring')]
    parts += [function('OP('+name+')') for name in NAMES]
    parts.append(function('int\ncjitexec('))
    unit='#define TEST_LEAF_DISPATCH_RUNTIME // Link the real C/fast complete-entry executor.\n'+prefix+declarations
    unit+='\n\n'.join(parts)
    body=(ROOT/'libinterp/tests/direct-target-native.c').read_text()
    body=body.replace('HANDLERS',','.join(NAMES)).replace('OPCODES',','.join('I'+name.upper() for name in NAMES))
    unit+=body
    widths=tuple(map(int,os.environ.get('DIRECT_TARGET_WORD_BITS','32 64').split()))
    if not widths or any(bits not in (32,64) for bits in widths):
        raise ValueError('DIRECT_TARGET_WORD_BITS must select 32 and/or 64')
    with tempfile.TemporaryDirectory(prefix='direct-target-native-') as directory:
        td=Path(directory)
        target=td/'targets.c'
        target.write_text(unit)
        command=shlex.split(os.environ.get('CC','cc'))
        command+=shlex.split(os.environ.get('CFLAGS','-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        for bits in widths:
            model=td/str(bits)
            model.mkdir()
            header=(ROOT/'include/interp.h').read_text()
            if bits==32:
                header=re.sub(r'typedef long\s+WORD;','typedef int WORD;',header,count=1)
                header=re.sub(r'typedef unsigned long\s+UWORD;','typedef unsigned int UWORD;',header,count=1)
            (model/'interp.h').write_text(header)
            includes=['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
            binary=model/'targets'
            subprocess.run(command+includes+[str(target),'-o',str(binary)],check=True)
            routes=[os.environ['DIRECT_TARGET_ROUTE']] if os.environ.get('DIRECT_TARGET_ROUTE') else ['0','1','2']
            for route in routes:
                subprocess.run([str(binary)],env=dict(os.environ,DIRECT_TARGET_ROUTE=route),check=True)


# Every C predicate remains enabled with NDEBUG.
if __name__=='__main__':
    main()
