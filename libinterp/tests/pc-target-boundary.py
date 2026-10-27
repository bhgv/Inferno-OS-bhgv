#!/usr/bin/env python3
"""Check complete Inst spans through actual C and executable native table entries."""
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning interpreter tree.
SOURCE = Path(os.environ.get('PC_TARGET_SOURCE', ROOT/'libinterp/xec.c')).read_text()


# Extract production code while ignoring braces in strings and comments.
def function(signature):
    start = SOURCE.index(signature)
    brace = SOURCE.index('{', start)
    depth = 0
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern, SOURCE[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return SOURCE[start:brace+token.end()]
    raise RuntimeError(signature)


# Preserve native pointer width independently of VM scalar and wire-cell widths.
def main():
    machine = platform.machine().lower()
    backend = 'amd64' if machine in ('x86_64', 'amd64') else 'arm64' if machine in ('aarch64', 'arm64') else None
    if backend is None:
        print('SKIP native PC boundaries: requires AMD64 or ARM64')
        return
    prefix = (ROOT/'libinterp/tests/pc-tables-native.c').read_text().split('#define OP(name)')[0]
    prefix = prefix.replace('comp-amd64.c', 'comp-'+backend+'.c')
    declarations = '''
void (*optab[256])(void); // Actual complete-entry handler table.
char exOp[]="opcode"; // Invalid opcode error identity.
#define OP(name) void name(void) // Real interpreter opcode definitions.


/* Empty string tables never compare elements; detect an unexpected comparison. */
int
stringcmp(String *left,String *right)
{
    (void)left;(void)right;
    abort();
    return 0;
}
'''
    signatures = ['static Inst*\ncasepc(', 'static uvlong\ncaseccell(']
    signatures += ['OP('+name+')' for name in ('icase', 'casel', 'igoto', 'movpc', 'casec')]
    signatures += ['int\ncjitexec(']
    unit = '#define TEST_LEAF_DISPATCH_RUNTIME // Link the actual complete-entry executor.\n'+prefix+declarations
    unit += '\n\n'.join(function(signature) for signature in signatures)
    unit += (ROOT/'libinterp/tests/pc-target-boundary.c').read_text()
    widths = tuple(map(int, os.environ.get('PC_TARGET_WORD_BITS', '32 64').split()))
    cells = tuple(map(int, os.environ.get('PC_TARGET_CELLS', '4 8').split()))
    if not widths or any(bits not in (32, 64) for bits in widths):
        raise ValueError('PC_TARGET_WORD_BITS must select 32 and/or 64')
    if not cells or any(size not in (4, 8) for size in cells):
        raise ValueError('PC_TARGET_CELLS must select 4 and/or 8')
    with tempfile.TemporaryDirectory(prefix='pc-target-boundary-') as directory:
        td = Path(directory)
        target = td/'targets.c'
        target.write_text(unit)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
        for bits in widths:
            for size in cells:
                model = td/(str(bits)+'-'+str(size))
                model.mkdir()
                header = (ROOT/'include/interp.h').read_text()
                if bits == 32:
                    header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
                    header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
                (model/'interp.h').write_text(header)
                isa = re.sub(r'IBY2WD\s*=\s*4', 'IBY2WD = '+str(size), (ROOT/'include/isa.h').read_text())
                (model/'isa.h').write_text(isa)
                includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
                binary = model/'targets'
                subprocess.run(command+includes+[str(target), '-o', str(binary)], check=True)
                routes = [os.environ['PC_TARGET_ROUTE']] if os.environ.get('PC_TARGET_ROUTE') else ['0', '1', '2']
                for route in routes:
                    env = dict(os.environ, PC_TARGET_ROUTE=route)
                    subprocess.run([str(binary)], env=env, check=True)


# Explicit C predicates and isolated error routes remain active under NDEBUG.
if __name__ == '__main__':
    main()
