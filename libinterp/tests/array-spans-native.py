#!/usr/bin/env python3
"""Check array operand cells, headers and captured allocation outputs."""
import os
import platform
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

if platform.machine().lower() not in ('x86_64', 'amd64'):
    raise SystemExit('array-spans-native requires an AMD64 host for real RX execution')

ROOT = Path(__file__).resolve().parents[2]  # Owning VM source tree.
XEC = Path(os.environ.get('ARRAY_SPAN_SOURCE', ROOT/'libinterp/xec.c')).read_text()
HEAP = (ROOT/'libinterp/heap.c').read_text()  # Real allocation and map consumers.


# Extract a production body without treating literal or comment braces as syntax.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


unit = '#define TEST_LEAF_DISPATCH_RUNTIME // Link the real complete C executor.\n'
unit += (ROOT/'libinterp/tests/array-reuse-native.c').read_text().split('#define OP(name)')[0]
unit += '''
void (*optab[256])(void);
char exBounds[]="bounds";
char exOp[]="opcode";
#define OP(name) void name(void) // Real decoded opcode entry.
'''
unit += '\n\n'.join(function(HEAP, signature) for signature in (
    'void\nincmem(', 'void\nfreeptrs(', 'void\ninitmem(', 'void\nfreearray(',
    'void\nacheck(', 'Heap*\nnheap(', 'void\ninitarray(', 'Heap*\nheaparray(', 'Heap*\nheap(',
))
unit += '\n\n'.join(function(XEC, signature) for signature in (
    'static Type*\nmoduletype(', 'static void\nnewarray(', 'static uchar*\narrayspan(',
    'OP(newa)', 'OP(newaz)', 'OP(slicea)', 'OP(slicela)', 'int\ncjitexec(',
))
unit += (ROOT/'libinterp/tests/array-spans-native.c').read_text()

with tempfile.TemporaryDirectory(prefix='array-spans-native-') as temporary:
    directory = Path(temporary)
    source = directory/'arrays.c'
    source.write_text(unit)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get(
        'CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all',
    ))
    for wordbytes in (4, 8):
        model = directory/str(wordbytes)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if wordbytes == 4:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;',
                            'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        binary = model/'arrays'
        includes = ['-I'+str(model)]
        includes += ['-I'+str(ROOT/path) for path in (
            'Linux/arm/include', 'emu/port', 'include', 'libinterp',
        )]
        subprocess.run(command+includes+[str(source), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=180)
