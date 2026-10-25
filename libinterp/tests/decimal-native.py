#!/usr/bin/env python3
"""Compare short decimal native paths with C and the FP environment."""
import os
import json
import platform
import sys
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

if platform.machine().lower() not in ('x86_64','amd64'):
    print('SKIP native AMD64 decimal parsing: requires an x86-64 host')
    sys.exit(0)

root = Path(__file__).resolve().parents[2]
source = Path(os.environ.get('DECIMAL_XEC_SOURCE', root/'libinterp/xec.c')).read_text()


def function(signature):
    """Extract production C bodies while ignoring braces in comments and literals."""
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:return source[start:brace+token.end()]
    raise ValueError(signature)


parts=[function('OP(cvtcl)')]
source=Path(os.environ.get('DECIMAL_STRING_SOURCE',root/'libinterp/string.c')).read_text()
realbody = function('OP(cvtcf)')
production_parser = bool(os.environ.get('DECIMAL_VM_PARSER'))
if production_parser:
    realbody = realbody.replace('strtod(', 'fixture_vm_strtod(')
parts += [function('char*\nstring2c('), function('OP(cvtcw)'), realbody]
unit=(root/'libinterp/tests/decimal-native.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
if production_parser:
    unit = 'extern double fixture_vm_strtod(const char*, char**);\n'+unit
with tempfile.TemporaryDirectory(prefix='decimal-native-') as directory:
    td = Path(directory)
    target = td/'branches.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    objects = []
    if production_parser:
        minimal = td/'parser-headers'
        minimal.mkdir()
        header = '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <math.h>\n'
        header += '#define nil NULL // Production parser null pointer.\n'
        if sys.byteorder == 'little':
            header += '#ifndef __LITTLE_ENDIAN\n#define __LITTLE_ENDIAN 1 // Native binary64 word order.\n#endif\n'
        (minimal/'lib9.h').write_text(header)
        wrapper = td/'vm-parser.c'
        wrapper.write_text('#include "lib9.h"\n#define strtod fixture_vm_strtod // Keep the libc oracle independent.\n'
                           '#include '+json.dumps(str(root/'libmath/dtoa.c'))+'\n')
        parser = td/'vm-parser.o'
        subprocess.run(cmd+['-fno-strict-aliasing', '-I'+str(minimal), '-c', str(wrapper), '-o', str(parser)], check=True)
        objects = [str(parser)]
    for bits in (64,32):
        model = td/('word'+str(bits))
        model.mkdir()
        header = (root/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(root/p) for p in ('Linux/arm/include','emu/port','include','libinterp')]
        binary = td/('branches'+str(bits))
        subprocess.run(cmd+includes+[str(target)]+objects+['-o',str(binary),'-lm'],check=True)
        subprocess.run([str(binary)],check=True)
