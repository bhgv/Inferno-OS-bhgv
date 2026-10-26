#!/usr/bin/env python3
"""Compare short decimal native paths with C and the FP environment."""
import os
import ast
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


production_big = bool(os.environ.get('DECIMAL_VM_BIG_PARSER'))
bigbody = function('OP(cvtcl)')
if production_big:
    bigbody = bigbody.replace('strtoll(', 'fixture_vm_strtoll(')
parts = [bigbody]
source=Path(os.environ.get('DECIMAL_STRING_SOURCE',root/'libinterp/string.c')).read_text()
realbody = function('OP(cvtcf)')
production_parser = bool(os.environ.get('DECIMAL_VM_PARSER'))
if production_parser:
    realbody = realbody.replace('strtod(', 'fixture_vm_strtod(')
parts += [function('char*\nstring2c('), function('OP(cvtcw)'), realbody]
unit=(root/'libinterp/tests/decimal-native.c').read_text().replace('IMPLEMENTATION','\n'.join(parts))
if production_parser:
    unit = 'extern double fixture_vm_strtod(const char*, char**);\n'+unit
if production_big:
    # Python integers provide an independent modulo-2^64 oracle for the real lib9 parser.
    texts = unit.split('static char *texts[] = {', 1)[1].split('};', 1)[0]
    values = []
    for literal in re.findall(r'"(?:\\.|[^"\\])*"', texts):
        text = ast.literal_eval(literal).encode('latin1')
        prefix = re.match(rb'^[ \t\n\r\f\v]*([+-]?[0-9]+)', text)
        values.append((int(prefix[1]) if prefix else 0) & ((1 << 64)-1))
    table = ','.join(hex(value)+'ULL' for value in values)
    unit = ('extern long long fixture_vm_strtoll(const char*, char**, int);\n'
            'static const unsigned long long fixture_big_values[] = {'+table+'};\n'+unit)

with tempfile.TemporaryDirectory(prefix='decimal-native-') as directory:
    td = Path(directory)
    target = td/'branches.c'
    target.write_text(unit)
    cmd = shlex.split(os.environ.get('CC','cc'))
    cmd += shlex.split(os.environ.get('CFLAGS','-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
    if production_big:
        cmd += ['-DDECIMAL_VM_BIG=1']
    objects = []
    if production_parser or production_big:
        minimal = td/'parser-headers'
        minimal.mkdir()
        header = '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <math.h>\n'
        header += 'typedef long long vlong;typedef unsigned long long uvlong;\n'
        header += '#define nil NULL // Production parser null pointer.\n'
        if sys.byteorder == 'little':
            header += '#ifndef __LITTLE_ENDIAN\n#define __LITTLE_ENDIAN 1 // Native binary64 word order.\n#endif\n'
        (minimal/'lib9.h').write_text(header)
    if production_parser:
        wrapper = td/'vm-parser.c'
        wrapper.write_text('#include "lib9.h"\n#define strtod fixture_vm_strtod // Keep the libc oracle independent.\n'
                           '#include '+json.dumps(str(root/'libmath/dtoa.c'))+'\n')
        parser = td/'vm-parser.o'
        subprocess.run(cmd+['-fno-strict-aliasing', '-I'+str(minimal), '-c', str(wrapper), '-o', str(parser)], check=True)
        objects.append(str(parser))
    if production_big:
        wrapper = td/'vm-big-parser.c'
        wrapper.write_text('#include "lib9.h"\n#define strtoll fixture_vm_strtoll // Preserve the libc comparison parser.\n'
                           '#include '+json.dumps(str(root/'lib9/strtoll.c'))+'\n')
        parser = td/'vm-big-parser.o'
        subprocess.run(cmd+['-I'+str(minimal), '-c', str(wrapper), '-o', str(parser)], check=True)
        objects.append(str(parser))
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
