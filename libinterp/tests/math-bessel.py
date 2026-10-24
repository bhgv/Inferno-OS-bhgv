#!/usr/bin/env python3
"""Exercise production Bessel orders with native libm and analytic boundary oracles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
vm = Path(os.environ.get('BESSEL_VM_SOURCE', root/'libinterp/math.c')).read_text()
native = Path(os.environ.get('BESSEL_NATIVE_SOURCE', root/'libmath/fdlibm/e_jn.c')).read_text()

case = os.environ.get('BESSEL_CASE', '')
if case not in ('', 'ordinary', 'extreme', 'indices', 'wide'):
    raise ValueError('unknown BESSEL_CASE: '+case)


def function(source, signature):
    """Extract a complete function without counting comment or literal braces."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[start:brace+token.end()]
    raise ValueError(signature)


parts = [re.sub(r'^#include[^\n]*\n', '', native, flags=re.M)]
if 'static int\nmathorder(' in vm:
    parts += [function(vm, 'static int\nmathorder(')]
parts += [function(vm, 'void\nMath_'+name+'(') for name in ('jn', 'yn')]
implementation = '\n'.join(parts)
implementation = implementation.replace('__ieee754_jn(', 'tested_jn(').replace('__ieee754_yn(', 'tested_yn(')
frames = (root/'libinterp/runt.h').read_text()
declarations = []
for name in ('jn', 'yn'):
    kind = 'F_Math_'+name
    a = frames.index('struct '+kind+'\n')
    b = frames.index('\n};', a)+3
    declarations.append('typedef struct '+kind+' '+kind+';\n'+frames[a:b])
unit = (root/'libinterp/tests/math-bessel.c').read_text()
unit = unit.replace('FRAMES', '\n'.join(declarations)).replace('IMPLEMENTATION', implementation)
hi = 1 if sys.byteorder == 'little' else 0
unit = unit.replace('HI_INDEX', str(hi)).replace('LO_INDEX', str(1-hi))
with tempfile.TemporaryDirectory(prefix='math-bessel-') as directory:
    td = Path(directory)
    command = shlex.split(os.environ.get('CC', 'cc'))
    command += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    for bits in (64, 32):
        source = td/('bessel'+str(bits)+'.c')
        source.write_text(unit.replace('WORD_TYPE', 'int'+str(bits)+'_t'))
        binary = td/('bessel'+str(bits))
        subprocess.run(command+['-fno-strict-aliasing', str(source), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+shlex.split(os.environ.get('BESSEL_CASE', '')), check=True, timeout=30)
