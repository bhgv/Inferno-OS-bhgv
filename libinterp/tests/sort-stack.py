#!/usr/bin/env python3
"""Check native and Math sort stack bounds on fixed adversarial permutations."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
mathsource = Path(os.environ.get('SORT_MATH_SOURCE', root / 'libinterp/math.c')).read_text()


# Retain complete function bodies while ignoring comment and literal braces.
def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 0
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]'
    for token in re.finditer(tokens, text[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return text[start:brace+token.end()]
    raise ValueError(signature)


mathparts = [function(mathsource, s) for s in
             ('static int\ncmp(', 'static void\nswap(', 'static void\nvecswap(',
              'static WORD*\nmed3(', 'static void\nrqsort(')]
mathparts[0] = mathparts[0].replace('cmp(WORD *u,', 'mathcompare(WORD *u,', 1)
mathparts[-1] = mathparts[-1].replace('rqsort(WORD *a,', 'rqsort_body(WORD *a,', 1)
mathbody = '\n'.join(mathparts)
with tempfile.TemporaryDirectory(prefix='sort-stack-') as directory:
    td = Path(directory)
    for library in ('lib9', 'libkern'):
        path = Path(os.environ.get('SORT_' + library.upper() + '_SOURCE', root / library / 'qsort.c'))
        source = path.read_text().replace('#include "lib9.h"', '')
        source = re.sub(r'\bqsort\b', 'native_qsort', source)
        source = source.replace('} Sort;', '} Sort;\nstatic void qsorts(char*, long, Sort*);')
        source = source.replace('qsorts(char *a,', 'qsorts_body(char *a,', 1)
        unit = (root / 'libinterp/tests/sort-stack.c').read_text()
        target = td / (library + '.c')
        target.write_text(unit.replace('NATIVE_IMPLEMENTATION', source)
                         .replace('MATH_IMPLEMENTATION', mathbody))
        cmd = shlex.split(os.environ.get('CC', 'cc'))
        cmd += shlex.split(os.environ.get('CFLAGS',
                           '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        for width in (32, 64):
            binary = td / (library + str(width))
            subprocess.run(cmd + ['-DWORD_BITS=' + str(width), str(target), '-lm',
                                  '-o', str(binary)], check=True)
            subprocess.run([str(binary)] + shlex.split(os.environ.get('SORT_CASE', '')),
                           check=True, timeout=120)
