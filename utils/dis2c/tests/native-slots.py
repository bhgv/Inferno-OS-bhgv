#!/usr/bin/env python3
"""Compile emitted AOT code and compare packed slots/addresses with the real VM."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]  # Owning translator and VM source tree.
SOURCE = Path(os.environ.get('DIS2C_NATIVE_SOURCE', ROOT/'utils/dis2c/dis2c.c'))
isa = re.sub(r'/\*.*?\*/', '', (ROOT/'include/isa.h').read_text(), flags=re.S)
ops = [name.strip() for name in isa[isa.index('INOP'):isa.index('};')].split(',') if name.strip()]
table = re.sub(r'/\*.*?\*/', '', (ROOT/'libinterp/optab.h').read_text(), flags=re.S)
handlers = [name.strip() for name in table[table.index('{')+1:table.index('};')].split(',') if name.strip()]
if len(ops) != len(handlers):
    raise RuntimeError('opcode/handler mismatch')
inline = ['IMOVB','IMOVW','IMOVL','IMOVF','IADDW','ISUBW','IMULW','IANDW','IORW','IXORW',
          'IADDL','ISUBL','IMULL','IANDL','IORL','IXORL']
records = [(op, 3, 3, 0, 0, 0, 0) for op in inline]
for src in range(6):
    for dst in range(6):
        for mid in (0, 64, 128, 192):
            records.append(('IRUNT', src, dst, mid, 8 if src != 2 else -17, 8 if dst != 2 else 31, -32768 if mid == 64 else 25))
# Include every active complex-command dispatch with borrowed decoded registers.
records += [(op, 3, 3, 0, 0, 0, 0) for op in ops if op not in ('INOP','IECLR')]


# Encode a signed canonical Dis operand with no host-width dependence.
def operand(value):
    if not -536870912 <= value <= 536870911:
        raise ValueError(value)
    if -64 <= value <= 63:
        return bytes([value & 127])
    if -8192 <= value <= 8191:
        return bytes([128 | ((value >> 8) & 63), value & 255])
    return bytes([192 | ((value >> 24) & 63), (value >> 16) & 255, (value >> 8) & 255, value & 255])


# Extract complete real C handlers while ignoring braces in comments and literals.
def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*|[{}]', source[brace:]):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                return source[start:brace+token.end()]
    raise RuntimeError(signature)


wire = b''.join(operand(v) for v in (819248, 0, 1024, len(records), 0, 0, 0, -1, -1))
initializers = []
for op, src, dst, mid, source, dest, middle in records:
    wire += bytes([ops.index(op), src*8+dst+mid])
    if mid:
        wire += operand(middle)
    fields = []
    for name, mode, value in (('s', src, source), ('d', dst, dest)):
        if mode != 3:
            wire += operand(1)+operand(11) if mode >= 4 else operand(value)
        fields.append('.'+name+'={.i={.f=1,.s=11}}' if mode >= 4 else '.'+name+'={.imm='+str(value)+'}')
    initializers.append('{.op='+op+',.add='+str(src*8+dst+mid)+',.reg=(ushort)'+str(middle)+','+','.join(fields)+'}')
wire += b'\0Native\0'
xec = (ROOT/'libinterp/xec.c').read_text()
actual = {handlers[ops.index(op)] for op in inline} | {'runt'}
bodies = []
for i, handler in enumerate(dict.fromkeys(handlers)):
    if handler in actual:
        bodies.append(function(xec, 'OP('+handler+')'))
    else:
        bodies.append('/* Observe the selected complex VM handler without simulating its effects. */\nvoid\n'+handler+'(void)\n{\n\tcalled = '+str(i)+';\n}')
unit = (ROOT/'utils/dis2c/tests/native-slots.c').read_text()
unit = unit.replace('PROGRAM_COUNT', str(len(records))).replace('PROGRAM_INITIALIZERS', ',\n'.join(initializers))
unit = unit.replace('HANDLERS', ','.join(handlers)).replace('OPERATIONS', '\n\n\n'.join(bodies))
with tempfile.TemporaryDirectory(prefix='dis2c-native-') as temporary:
    work = Path(temporary)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all'))
    tool = work/'dis2c'
    sanitizer = work/'sanitizer.c'
    sanitizer.write_text('/* Retain ASan/UBSan without unavailable process leak inspection. */\nint __lsan_is_turned_off(void) { return 1; }\n')
    subprocess.run(cc+flags+['-std=c99','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-I'+str(ROOT/'utils/dis2c'),str(SOURCE),str(sanitizer),'-o',str(tool)], check=True)
    image = work/'native.dis'
    image.write_bytes(wire)
    generated = work/'native.c'
    subprocess.run([str(tool),'-n','native','-o',str(generated),str(image)], check=True)
    fixture = work/'fixture.c'
    fixture.write_text(unit)
    widths = tuple(map(int, os.environ.get("DIS2C_WORD_BITS", "32 64").split()))
    if not widths or any(bits not in (32, 64) for bits in widths):
        raise ValueError("DIS2C_WORD_BITS must select 32 and/or 64")
    for bits in widths:
        model = work/str(bits)
        model.mkdir()
        header = (ROOT/'include/interp.h').read_text()
        if bits == 32:
            header = re.sub(r'typedef long\s+WORD;', 'typedef int WORD;', header, count=1)
            header = re.sub(r'typedef unsigned long\s+UWORD;', 'typedef unsigned int UWORD;', header, count=1)
        (model/'interp.h').write_text(header)
        includes = ['-I'+str(model)]+['-I'+str(ROOT/p) for p in ('Linux/arm/include','include','libinterp')]
        binary = model/'native'
        subprocess.run(cc+flags+includes+[str(fixture),str(generated),str(ROOT/'libinterp/dec.c'),'-o',str(binary)], check=True)
        families = [os.environ['DIS2C_NATIVE_CASE']] if os.environ.get('DIS2C_NATIVE_CASE') else ['slots','addresses','fallback','invalid','decode','headers']
        for family in families:
            subprocess.run([str(binary),family], check=True, timeout=60)
