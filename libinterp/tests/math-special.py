#!/usr/bin/env python3
"""Exercise production scalar Math functions against independent host-libm oracles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]


def function(source, signature):
    """Extract C functions while ignoring braces inside literals and comments."""
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
    raise ValueError(signature)


vm = Path(os.environ.get('MATH_SPECIAL_SOURCE', root / 'libinterp/math.c')).read_text()
scalar = Path(os.environ.get('MATH_SCALAR_SOURCE', root / 'libmath/fdim.c')).read_text()
modf = Path(os.environ.get('MATH_MODF_SOURCE', root / 'libmath/fdlibm/s_modf.c')).read_text()
parts = [re.sub(r'^#include[^\n]*\n', '', text, flags=re.M) for text in (scalar, modf)]
parts += [function(vm, 'void\nMath_'+name+'(') for name in ('fdim', 'fmin', 'fmax', 'modf')]
implementation = '\n'.join(parts)
for name in ('fdim', 'fmin', 'fmax', 'modf'):
    implementation = re.sub(r'\b'+name+r'\s*\(', 'tested_'+name+'(', implementation)
frames = (root / 'libinterp/runt.h').read_text()
declarations = []
for name in ('fdim', 'fmin', 'fmax', 'modf'):
    typename = 'F_Math_'+name
    a = frames.index('struct '+typename+'\n')
    b = frames.index('\n};', a)+3
    declarations.append('typedef struct '+typename+' '+typename+';\n'+frames[a:b])
unit = (root / 'libinterp/tests/math-special.c').read_text()
unit = unit.replace('FRAMES', '\n'.join(declarations)).replace('IMPLEMENTATION', implementation)
hi = 1 if sys.byteorder == 'little' else 0
unit = unit.replace('HI_INDEX', str(hi)).replace('LO_INDEX', str(1-hi))
with tempfile.TemporaryDirectory(prefix='math-special-') as directory:
    td = Path(directory)
    target = td / 'special.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC', 'cc'))
    cmd += shlex.split(os.environ.get('CFLAGS', '-O2 -g -fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all'))
    cmd += ['-fno-strict-aliasing', '-fno-builtin', '-I'+str(root / 'include')]
    for width in (32, 64):
        binary = td / ('special'+str(width))
        subprocess.run(cmd+['-DWORD_BITS='+str(width), str(target), '-lm', '-o', str(binary)], check=True)
        subprocess.run([str(binary)]+os.environ.get('MATH_SPECIAL_CASE', '').split(), check=True, timeout=30)
