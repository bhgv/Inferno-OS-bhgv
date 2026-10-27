#!/usr/bin/env python3
"""Check string conversion operands and captured C allocation outputs."""
import os
import platform
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

if platform.machine().lower() not in ('x86_64', 'amd64'):
    raise SystemExit('string-capture-native requires an AMD64 host for real RX execution')

ROOT = Path(__file__).resolve().parents[2]  # Owning VM source tree.
STRING = Path(os.environ.get('STRING_CAPTURE_SOURCE', ROOT/'libinterp/string.c')).read_text()


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


unit = '#define TEST_LEAF_DISPATCH_RUNTIME // Exercise real complete JIT dispatch.\n'
prefix = (ROOT/'libinterp/tests/string-codecs.c').read_text().split('IMPLEMENTATION')[0]
prefix = re.sub(r'^#include "[^"\n]+"\n', '', prefix, flags=re.M)
prefix = prefix.replace('#include <assert.h>', '#include "comp-amd64.c"\n#include "pool.h"\n#include "tests/leaf-cons-stub.h"\n#include "tests/leaf-plain-stubs.h"')
prefix = prefix.replace('static int failure = -1, checks;', 'static int failure = -1, checks;\nstatic void (*allocationhook)(void); // Bounded allocator follows the real nheap callback contract.')
prefix = prefix.replace('Heap*\nnheap(int n)', 'static Heap*\ncodecstorage(int n)', 1)
# Imported fixture predicates remain active in release builds.
while 'assert(' in prefix:
    start = prefix.index('assert(')
    end, depth = start+7, 1
    while depth:
        if prefix[end] == '(':
            depth += 1
        elif prefix[end] == ')':
            depth -= 1
        end += 1
    prefix = prefix[:start]+'if(!('+prefix[start+7:end-1]+')) abort()'+prefix[end:]
unit += prefix
unit += '\nvoid (*optab[256])(void);\nchar exOp[]="opcode";\nPool *heapmem;\nvoid (*heapmonitor)(int, void*, ulong);\nvoid *poolalloc(Pool*, ulong);\n'
unit += function((ROOT/'libinterp/heap.c').read_text(), 'Heap*\nnheap(')
unit += '\n\n'.join(function(STRING, signature) for signature in (
    'String*\nnewstring(', 'String*\nnewrunes(', 'void\ncvtup(',
    'String*\nstringdup(', 'String*\naddstring(', 'String*\nslicer(',
    'OP(slicec)', 'OP(addc)', 'OP(cvtca)', 'OP(cvtac)',
    'OP(cvtwc)', 'OP(cvtlc)', 'OP(cvtfc)', 'char*\nstring2c(', 'String*\nc2string(',
))
unit += function((ROOT/'libinterp/runt.c').read_text(), 'Array*\nmem2array(')
unit += function((ROOT/'libinterp/xec.c').read_text(), 'int\ncjitexec(')
unit += '\n#undef malloc\n'
unit += (ROOT/'libinterp/tests/string-capture-native.c').read_text()

with tempfile.TemporaryDirectory(prefix='string-capture-native-') as temporary:
    directory = Path(temporary)
    source = directory/'codecs.c'
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
        binary = model/'codecs'
        includes = ['-I'+str(model)]
        includes += ['-I'+str(ROOT/path) for path in (
            'Linux/arm/include', 'emu/port', 'include', 'libinterp',
        )]
        subprocess.run(command+includes+[str(source), '-lm', str(ROOT/'lib9/rune.c'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=180)
