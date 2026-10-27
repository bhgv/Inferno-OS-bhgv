#!/usr/bin/env python3
"""Load real canonical words, then execute real C and host-native 64-bit leaves in both WORD models."""
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]  # Owning source tree.


# Borrow complete handler bodies while excluding braces inside comments and strings.
def function(source, signature):
    start=source.index(signature);brace=source.index('{',start);depth=0
    pattern=r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(pattern,source[brace:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if depth==0:return source[start:brace+token.end()]
    raise RuntimeError(signature)


# The existing parser fixture supplies real Type/error-frame ownership boundaries.
def main():
    machine=platform.machine().lower()
    backend='amd64' if machine in ('x86_64','amd64') else 'arm64' if machine in ('aarch64','arm64') else None
    if backend is None:
        print('SKIP native literal-to-JIT integration: requires AMD64 or ARM64')
        return
    fixture=(ROOT/'libinterp/tests/dis-loader.c').read_text()
    fixture=fixture.replace('#include <assert.h>', '''/* Retain inherited fixture checks even with NDEBUG. */
static void
numericcondition(int condition)
{
    if(!condition)abort();
}
#define numericcheck(condition) numericcondition(!!(condition)) // Normalize inherited pointer and scalar conditions.
''').replace('assert(', 'numericcheck(')
    fixture=fixture.replace('main(void)', 'numericlegacy(void)',1)
    fixture=fixture.replace('static int at,map_at,','static int literal_at; // Canonical DEFW bytes in the actual module image.\nstatic int at,map_at,',1)
    fixture=fixture.replace('integer(0);word(42);word(99);','integer(0);literal_at=at;word(42);word(99);',1)
    fixture += '\n#include <stddef.h>\n#include "'+backend+'-emit.h"\n#include "leaf-code.h"\n'
    fixture += '\nREG R; // Actual decoded register file consumed by both execution routes.\nchar exBounds[]="bounds"; // Handler validation error boundary.\n#define OP(name) static void C_##name(void) // Borrow production operation bodies.\n'
    source=Path(os.environ.get('NUMERIC_XEC_SOURCE',ROOT/'libinterp/xec.c')).read_text()
    for name in ('movw','addw','subw','mulw','andw','orw','xorw'):
        fixture+='\n\n'+function(source,'OP('+name+')')
    fixture += (ROOT/'libinterp/tests/dis-numeric-literals.c').read_text()
    with tempfile.TemporaryDirectory(prefix='dis-numeric-literals-') as directory:
        target=Path(directory)/'fixture.c';target.write_text(fixture)
        for width in ('4','8'):
            env=dict(os.environ,DIS_LOAD_FIXTURE=str(target),DIS_WORD_BYTES=width)
            env.setdefault('CFLAGS','-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all')
            subprocess.run([sys.executable,str(ROOT/'libinterp/tests/dis-loader.py')],env=env,check=True)


# Reject any parser, ownership, byte-image or native execution mismatch.
if __name__ == '__main__':
    main()
