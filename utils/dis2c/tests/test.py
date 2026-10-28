#!/usr/bin/env python3
"""Compile generated C and compare it with the actual VM loader/decoder/executor."""
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile

# Keep integration predicates active under Python -O and C NDEBUG.
def require(condition, detail):
    if not condition:
        raise RuntimeError(detail)


root = Path(__file__).resolve().parents[3]
isa = re.sub(r'/\*.*?\*/', '', (root/'include/isa.h').read_text(), flags=re.S)
ops = [s.strip() for s in isa[isa.index('INOP'):isa.index('};')].split(',') if s.strip()]
opcode = {name: i for i, name in enumerate(ops)}
table = re.sub(r'/\*.*?\*/', '', (root/'libinterp/optab.h').read_text(), flags=re.S)
handlers = [s.strip() for s in table[table.index('{')+1:table.index('};')].split(',') if s.strip()]
require(len(ops) == len(handlers), 'integration invariant')


# Encode one signed canonical Dis operand.
def operand(n):
    require(-536870912 <= n <= 536870911, 'integration invariant')
    if -64 <= n <= 63:
        return bytes([n & 127])
    if -8192 <= n <= 8191:
        return bytes([0x80 | ((n >> 8) & 63), n & 255])
    return bytes([0xc0 | ((n >> 24) & 63), (n >> 16) & 255, (n >> 8) & 255, n & 255])


# Encode exactly the fields selected by the addressing modes.
def instruction(op, src=3, dst=3, mid=0, s=0, d=0, m=0):
    data = bytes([opcode[op], (src << 3) | dst | mid])
    if mid:
        data += operand(m)
    for mode, value in ((src, s), (dst, d)):
        if mode == 3:
            continue
        if mode in (4, 5):
            data += operand(0)+operand(value)
        else:
            data += operand(value)
    return data


# Build a bounded image with optional imports and exception metadata.
def module(code, rich=False):
    flags = 97 if rich else 0  # MUSTCOMPILE, HASLDT, HASEXCEPT
    data = b''.join(operand(v) for v in [819248, flags, 1024, len(code), 128, 1, int(rich), 0, 0])
    data += b''.join(code)+operand(0)+operand(128)+operand(0)
    data += bytes([0x13])+operand(0)+b'ABC'+b'\0Fixture\0'
    if rich:
        data += operand(-1)+operand(-1)+struct.pack('>I', 42)+b'.mp\0'
        data += operand(1)+operand(1)+struct.pack('>I', 42)+b'foreign\0\0'
        data += operand(1)+operand(64)+operand(0)+operand(len(code))+operand(0)+operand(0x10002)
        data += b'first\0'+operand(2)+b'second\0'+operand(6)+operand(-1)+b'\0'
    return data+b'sum.b\0'


# Extract production C definitions without counting comment or literal braces.
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
    raise ValueError(signature)


with tempfile.TemporaryDirectory(prefix='dis2c-test-') as directory:
    td = Path(directory)
    cc = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(os.environ.get('CFLAGS', '-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    tool = td/'dis2c'
    subprocess.run(cc+flags+['-std=c99', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'include'), '-I'+str(root/'utils/dis2c'), str(Path(os.environ.get('DIS2C_NATIVE_SOURCE', root/'utils/dis2c/dis2c.c'))), '-o', str(tool)], check=True)
    loop = [instruction('IMOVW', 2, 1, s=0, d=64), instruction('IMOVW', 2, 1, s=1, d=72),
            instruction('IBGTW', 1, 2, 64, s=72, d=6, m=10),
            instruction('IADDW', 1, 1, 128, s=72, d=64, m=64),
            instruction('IADDW', 2, 1, 128, s=1, d=72, m=72),
            instruction('IJMP', 3, 2, d=2), instruction('IRUNT')]
    matrix = [instruction('IRUNT', s, d, m, s=8, d=16, m=-1 if m == 64 else 24)
              for s in range(6) for d in range(6) for m in (0, 64, 128, 192)]
    matrix += [instruction('IRUNT', 2, 1, 64, s=n, d=16, m=m)
               for n in (-536870912,-8193,-8192,-65,-64,-1,0,63,64,8191,8192,536870911)
               for m in (-32768,-1,0,32767,65535)]
    matrix += [instruction(op) for op in ops]
    images = {'loop': module(loop, True), 'matrix': module(matrix)}
    images['signed_module'] = operand(923426)+operand(4)+b'sig!'+images['loop'][len(operand(819248)):]
    images['empty'] = b''.join(operand(v) for v in (819248,0,1024,0,0,0,0,-1,-1))+b'\0Empty\0empty.b\0'
    for symbol, data in images.items():
        path = td/(symbol+'.dis'); path.write_bytes(data)
        subprocess.run([str(tool), '-n', symbol, '-o', str(td/(symbol+'.c')), str(path)], check=True)
        subprocess.run([str(tool), '--check', str(path)], check=True)
    # Reject every truncation; a malformed input must leave an existing output untouched.
    source = images['loop']
    for length in range(len(source)):
        (td/'bad.dis').write_bytes(source[:length])
        result = subprocess.run([str(tool), '--check', str(td/'bad.dis')], capture_output=True)
        # A source path is optional to the loader; its complete omission is legal.
        if source[:length].endswith(b'\0') and source[length:] == b'sum.b\0':
            require(result.returncode == 0, 'integration invariant')
        else:
            require(result.returncode != 0, length)
    (td/'bad.dis').write_bytes(b'bad')
    (td/'protected.c').write_text('keep')
    require(subprocess.run([str(tool), '-o', str(td/'protected.c'), str(td/'bad.dis')], capture_output=True).returncode != 0, 'integration invariant')
    require((td/'protected.c').read_text() == 'keep', 'integration invariant')
    require(subprocess.run([str(tool), '-n', 'bad-name', str(td/'loop.dis')], capture_output=True).returncode != 0, 'integration invariant')
    require(subprocess.run([str(tool), '-o', str(td/'loop.dis'), str(td/'loop.dis')], capture_output=True).returncode != 0, 'integration invariant')
    malformed = [module([instruction('IJMP', 3, 2, d=1)]),
                 module([instruction('IRUNT', 2, 1, 64, m=65536)]),
                 module([bytes([opcode['IRUNT'], (6<<3)|3])]),
                 module([instruction('IRUNT', 4, 3, s=-1)]),
                 images['loop']+b'junk']
    for raw in malformed:
        (td/'bad.dis').write_bytes(raw)
        require(subprocess.run([str(tool), '--check', str(td/'bad.dis')], capture_output=True).returncode != 0, 'integration invariant')
    # Compile the emitted files against the actual VM headers as separate units.
    includes = ['-I'+str(root/p) for p in ('Linux/arm/include', 'emu/port', 'include', 'libinterp')]
    objects = []
    for name in images:
        obj = td/(name+'.o'); objects.append(str(obj))
        subprocess.run(cc+flags+includes+['-c', str(td/(name+'.c')), '-o', str(obj)], check=True)
    base = (root/'libinterp/tests/dis-loader.c').read_text().split('static uchar buffer[2048]')[0]
    load = (root/'libinterp/load.c').read_text()
    parser = load[load.index('/* Check the complete encoded operand'):load.index('Module*\nnewmod(')]
    heap = (root/'libinterp/heap.c').read_text()
    link = (root/'libinterp/link.c').read_text()
    parts = [function((root/'emu/port/dis.c').read_text(), 'int\nkprotect(')]
    parts += [function(heap, sig) for sig in ('Type*\ndtype(', 'void\nfreetype(', 'void\ninitmem(', 'void\ninitarray(')]
    parts += [function(link, sig) for sig in ('static void\nnewlink(', 'void\nmlink(', 'void\ndestroylinks(')]
    parts += [function(load, sig) for sig in ('static int\noperand(', 'static ulong\ndisw(', 'static WORD\ndissignedword(', 'double\ncanontod(', 'int\nbrpatch(', 'void\nfreemod(')]
    base = base.replace('IMPLEMENTATION', '\n'.join(parts)+'\n'+parser)
    actual = {'runt', 'movw', 'movb', 'movl', 'movf', 'addw', 'subw', 'mulw', 'andw', 'orw', 'xorw',
              'addl', 'subl', 'mull', 'andl', 'orl', 'xorl', 'jmp', 'bgtw', 'beqw'}
    xec = (root/'libinterp/xec.c').read_text()
    bodies = []
    for op, handler in zip(ops, handlers):
        if handler in actual:
            bodies.append(function(xec, 'OP('+handler+')'))
        else:
            bodies.append('void '+handler+'(void) { called = '+op+'; }')
    fixture = (root/'utils/dis2c/tests/runtime.c').read_text()
    fixture = fixture.replace('OPERATIONS', '\n'.join(bodies)).replace('DISPATCH', (root/'libinterp/optab.h').read_text())
    fixture = fixture.replace('EXECUTOR', function(xec, 'void\nxec('))
    (td/'runtime.c').write_text('static void runtimepredicate(int);\n#define runtimecheck(c) runtimepredicate(!!(c)) // Preserve inherited fixture predicates.\n'+base.replace('assert(', 'runtimecheck(')+'\n'+fixture)
    binary = td/'runtime'
    subprocess.run(cc+flags+includes+[str(td/'runtime.c'), str(root/'libinterp/disc.c'), str(root/'libinterp/dec.c')]+objects+['-o', str(binary)], check=True)
    subprocess.run([str(binary), str(td/'loop.dis')], check=True)
    if os.environ.get('LIMBO'):
        compiler = shlex.split(os.environ['LIMBO'])
        real = td/'sum-real.dis'
        subprocess.run(compiler+['-I'+str(root/'module'), '-I'+str(root/'utils/dis2c/examples'),
                                 '-o', str(real), str(root/'utils/dis2c/examples/sum.b')], check=True)
        generated = td/'sum-real.c'
        subprocess.run([str(tool), '-n', 'sum_real', '-o', str(generated), str(real)], check=True)
        subprocess.run(cc+flags+includes+['-c', str(generated), '-o', str(td/'sum-real.o')], check=True)
    # The checked-in mapping is a reproducible view of authoritative VM metadata.
    before = (root/'utils/dis2c/ops.h').read_bytes()
    subprocess.run(['python3', str(root/'utils/dis2c/genops.py')], check=True)
    require(before == (root/'utils/dis2c/ops.h').read_bytes(), 'integration invariant')
    print('dis2c: C99 build, generated C compilation, malformed inputs, metadata and runtime differential checks passed')
