#!/usr/bin/env python3
"""Build the real Dis64 constructor with production heap, decoder, loop and unwind."""
import os
from pathlib import Path
import platform
import re
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


# Extract unchanged production bodies while ignoring braces inside literals/comments.
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


# Encode the agreed signed7/14/30 range independently of the C operand reader.
def operand(value):
    if -64 <= value <= 63:
        return bytes([value & 127])
    if -8192 <= value <= 8191:
        return (0x8000 | (value & 0x3fff)).to_bytes(2, 'big')
    if not -(1 << 29) <= value < (1 << 29):
        raise ValueError(value)
    return (0xc0000000 | (value & 0x3fffffff)).to_bytes(4, 'big')


# Build whole images independently; scalar sections use no native pointer values.
def image(instructions, data=4, frame=64, name='Pilot', initialized=40, datatype=0, kind=1, reverse_types=False, include_data=True):
    word = lambda *values: struct.pack('>'+'I'*len(values), *values)
    module = word(0, 1, data) + (word(1 if reverse_types else datatype) if data else b'') + word(0, 0 if reverse_types else 1)
    data_type = word(data, 4 if data else 1) + bytes((data+63)//64)
    frame_type = word(frame, 8) + bytes((frame+63)//64)
    descriptors = word(2) + (frame_type+data_type if reverse_types else data_type+frame_type)
    pool = word(1, len(name)) + b''.join(word(ord(rune)) for rune in name)
    records = word(0) if initialized is None else word(1, 0, kind) + (initialized & ((1 << (32 if kind in (1, 4) else 64))-1)).to_bytes(4 if kind in (1, 4) else 8, "big")
    code = word(len(instructions)) + b''.join(instructions)
    payloads = [(1, module), (2, code), (3, descriptors)] + ([(4, records)] if include_data else []) + [(7, pool)]
    cursor = 32+12*len(payloads)
    directory = b''
    for kind, payload in payloads:
        directory += struct.pack('>HHII', kind, 1, cursor, len(payload))
        cursor += len(payload)
    return b'Dis64\r\n\x1a'+struct.pack('>HHIIIII', 1, 0, 32+12*len(payloads), 1, 0, 0, len(payloads))+directory+b''.join(p for _, p in payloads)


# Compile real source closures, then check independent inputs and result bits.
def main():
    if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        print('SKIP Dis64 runtime RX: requires Linux/AMD64 host')
        return
    heap = (ROOT/'libinterp/heap.c').read_text()
    xec = (ROOT/'libinterp/xec.c').read_text()
    load = (ROOT/'libinterp/load.c').read_text()
    stack = (ROOT/'libinterp/stack.c').read_text()
    emulator = (ROOT/'emu/port/dis.c').read_text()
    parts = [function(emulator, 'int\nkprotect(')]
    parts += [function(heap, signature) for signature in (
        'void\nfreeptrs(', 'void\nfreeheap(', 'void\ndestroy(', 'Type*\ndtype(',
        'void\nfreetype(', 'void\ninitmem(', 'Heap*\nheapz(', 'Heap*\nnheap(', 'void\nfreemodlink(')]
    parts += [function(load, signature) for signature in ('void\nfreemod(', 'void\nunload(')]
    parts += [function((ROOT/'libinterp/link.c').read_text(), 'Modlink*\nmklinkmod(')]
    parts += [function(stack, signature) for signature in
              ('static int\nstacksize(', 'static ulong\nstackhint(', 'static Stkext*\nstackextent(',
               'static Type*\nstackframe(', 'void\nnewstack(')]
    parts += [function(xec, signature) for signature in
              ('static WORD\nshiftcount(', 'static void\nbranchpc(', 'static void\nbranchreal(', 'static void\nunwindstate(', 'void\ndestroystack(',
               'int\ncjitexec(', 'void\nxec(')]
    names = ('movw', 'movl', 'movf', 'addw', 'addl', 'subw', 'subl', 'mulw', 'mull',
             'andw', 'andl', 'orw', 'orl', 'xorw', 'xorl', 'divw', 'divl', 'modw', 'modl',
             'shlw', 'shrw', 'shll', 'shrl', 'beqw', 'bnew', 'bltw', 'blew', 'bgtw', 'bgew',
             'beql', 'bnel', 'bltl', 'blel', 'bgtl', 'bgel',
             'beqf', 'bnef', 'bltf', 'blef', 'bgtf', 'bgef', 'jmp', 'iexit')
    parts += [function(xec, 'OP('+name+')') for name in names]
    loader = (ROOT/'libinterp/load64.c').read_text()
    parts.append(re.sub(r'^#include[^\n]*\n', '', loader, flags=re.M))
    unit = (ROOT/'libinterp/tests/dis64-runtime.c').read_text()
    unit = unit.replace('IMPLEMENTATION', '\n\n'.join(parts))
    # Explicit bindings are compiled by C; the independent image opcodes below are fixed ISA bytes.
    unit = unit.replace('HANDLERS', '\n'.join('optab[I'+('EXIT' if name == 'iexit' else name.upper())+'] = '+name+';' for name in names))
    exit_ = bytes.fromhex('0f1b')
    movw = lambda value, offset=0: bytes.fromhex('2d10')+operand(value)+operand(offset)
    addw = lambda value: bytes.fromhex('3a10')+operand(value)+operand(0)
    cases = [
        (image([movw(42), exit_], initialized=None), 4, 42),
        (image([addw(2), exit_]), 4, 42),
        (image([movw(-1), exit_]), 4, 0xffffffff),
        (image([movw(-(1 << 29)), exit_]), 4, 0xe0000000),
        (image([movw((1 << 29)-1), exit_]), 4, (1 << 29)-1),
        # MOVL source immediates must occupy all eight bytes of native Adr storage.
        (image([bytes.fromhex('7610')+operand(-1)+operand(0), exit_], data=8), 8, 0xffffffffffffffff),
        (image([bytes.fromhex('7610')+operand(-(1 << 29))+operand(0), exit_], data=8), 8, 0xffffffffe0000000),
        # Valid wire signed30 middle immediates that cannot fit native Inst.reg.
        (image([bytes.fromhex('3a50')+operand(32768)+operand(2)+operand(0), exit_]), 0, 0),
        (image([bytes.fromhex('3a50')+operand(-32769)+operand(2)+operand(0), exit_]), 0, 0),
        # Lossless pool names are broader than the explicit runtime display-name subset.
        (image([movw(42), exit_], name='\u03bb'), 0, 0),
        (image([movw(42), exit_], name=''), 0, 0),
        (image([movw(42), exit_], name='A\0B'), 0, 0),
    ]
    # Enumerate only the established first ISA enum, independently of C code generation.
    isa = (ROOT/'include/isa.h').read_text().split('};', 1)[0]
    opcodes = {name: number for number, name in enumerate(re.findall(r'^\s*(I[A-Z0-9]+),', isa, re.M))}
    instruction = lambda name, mode, *args: bytes([opcodes['I'+name.upper()], mode])+b''.join(operand(value) for value in args)
    for suffix, width, kind in (('w', 32, 1), ('l', 64, 2)):
        mask = (1 << width)-1
        for left in (0, 1, -1, -(1 << (width-1)), (1 << (width-1))-1):
            for right in (-32768, -1, 0, 1, 32767):
                operations = [('add', left+right), ('sub', left-right), ('mul', left*right),
                              ('and', left & right), ('or', left | right), ('xor', left ^ right)]
                for name, result in operations:
                    cases.append((image([instruction(name+suffix, 0x10, right, 0), exit_],
                                        data=width//8, initialized=left, kind=kind), width//8, result & mask))
            for count in (-1, 0, 1, width-1, width, width+1):
                for operation in ('shl', 'shr'):
                    invalid = count < 0 or count >= width
                    result = left if invalid else left << count if operation == 'shl' else left >> count
                    cases.append((image([instruction(operation+suffix, 0x10, count, 0), exit_],
                                        data=width//8, initialized=left, kind=kind),
                                  width//8+(200 if invalid else 0), result & mask))
            for right in (-1, 0, 1, 3):
                quotient = 0 if right == 0 else abs(left)//abs(right)*(1 if (left < 0) == (right < 0) else -1)
                for operation, result in (('div', quotient), ('mod', left-quotient*right)):
                    cases.append((image([instruction(operation+suffix, 0x10, right, 0), exit_],
                                        data=width//8, initialized=left, kind=kind),
                                  width//8+(100 if right == 0 else 0), (left if right == 0 else result) & mask))
        # Signed narrow middle immediate, explicit MP/FP middle and protected local storage.
        for middle in (-32768, 32767):
            cases.append((image([instruction('add'+suffix, 0x50, middle, 2, 0), exit_],
                                data=width//8, kind=kind), 4 if suffix == 'w' else 0,
                          (middle+2) & 0xffffffff if suffix == 'w' else 0))
    # Branches use a relocated pointer to the original Inst; taken paths skip MOVW99.
    for suffix, width, kind in (('w', 4, 1), ('l', 8, 2), ('f', 8, 3)):
        for operation in ('beq', 'bne', 'blt', 'ble', 'bgt', 'bge'):
            taken = operation in ('beq', 'ble', 'bge')
            program = [instruction(operation+suffix, 0xc2, 0, 0, 2), movw(99), exit_]
            cases.append((image(program, data=width, kind=kind, initialized=0), width, 0 if taken else 99))
    cases += [
        (image([instruction('jmp', 0x1a, 2), movw(99), exit_]), 4, 40),
        (image([exit_], data=0, initialized=None), 1, 0),
        (image([exit_], data=0, initialized=None, include_data=False), 1, 0),
        (image([addw(2), exit_], reverse_types=True), 4, 42),
        (image([instruction('addw', 0xd0, 65532, 2, 0), exit_], data=65536), 4, 2),
        (image([instruction('addw', 0xd0, 65536, 2, 0), exit_], data=65540), 0, 0),
        (image([instruction('addw', 0x90, 64, 2, 0), exit_], frame=72), 4, 2),
        (image([instruction('addw', 0x90, 65536, 2, 0), exit_], frame=65544), 0, 0),
        (image([movw(42), exit_], datatype=2), 0, 0),
    ]
    for bits in (0, 0x8000000000000000, 0x7ff0000000000000, 0x7ff8000000001234):
        cases.append((image([instruction('movf', 0x00, 0, 0), exit_], data=8, kind=3, initialized=bits), 8, bits))
    cases += [
        (image([movw(42), exit_], data=1024*1024+4), 0, 0),
        (image([movw(42), exit_], frame=1024*1024+8), 0, 0),
        (image([exit_]*65537, data=0, initialized=None), 0, 0),
        (image([instruction('nop', 0x1b), exit_]), 0, 0),
        (image([instruction('ret', 0x1b), exit_]), 0, 0),
    ]
    golden = image([addw(2), exit_])
    cases += [(golden[:extent], 0, 0) for extent in range(len(golden))]
    with tempfile.TemporaryDirectory(prefix='dis64-runtime-') as temporary:
        work = Path(temporary)
        source = work/'runtime.c'
        source.write_text(unit)
        command = shlex.split(os.environ.get('CC', 'cc'))
        command += shlex.split(os.environ.get('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all'))
        # Select the actual production header; no typedef rewriting is used.
        command += ['-I'+str(ROOT/path) for path in ('Linux/386/include', 'emu/port', 'include', 'libinterp')]
        binary = work/'runtime'
        sources = [str(source), str(ROOT/'libinterp/dec.c')]
        subprocess.run(command+sources+['-o', str(binary), '-lm', '-Wl,--wrap=free'], check=True)
        legacy_image = work/'legacy-rejection.dis64'
        legacy_image.write_bytes(cases[0][0])
        subprocess.run([str(binary), '--unsupported', str(legacy_image)], check=True)
        subprocess.run(command+['-DDIS_WORD32=1']+sources+['-o', str(binary), '-lm', '-Wl,--wrap=free'], check=True)
        predicates = 0
        for index, (payload, width, expected) in enumerate(cases):
            fixture = work/(str(index)+'.dis64')
            fixture.write_bytes(payload)
            result = subprocess.run([str(binary), str(fixture), str(width), format(expected, 'x')], capture_output=True, text=True)
            if result.returncode != 0:
                raise RuntimeError(f'case {index}, width {width}, expected {expected:x}: {result.stdout}{result.stderr}')
            predicates += int(re.search(r': (\d+) predicates', result.stdout).group(1))
        print(f'PASS Dis64 runtime: {len(cases)} whole images, {predicates} active predicates; actual heap/Inst/decoder/xec/errors/unwind C/RX; process scheduler lifecycle not run')


if __name__ == '__main__':
    main()
