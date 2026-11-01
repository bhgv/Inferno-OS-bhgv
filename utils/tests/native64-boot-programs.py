#!/usr/bin/env python3
"""Independent scalar Dis64 programs checking results inside a complete emulator."""
from pathlib import Path
import re
import runpy
import struct

ROOT = Path(__file__).resolve().parents[2]
OPERAND = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))['operand']
ISA = (ROOT/'include/isa.h').read_text().split('};', 1)[0]
OPCODES = {name: number for number, name in enumerate(re.findall(r'^\s*(I[A-Z0-9]+),', ISA, re.M))}


# Serialize only the established np0 scalar sections; introduce no wire IDs or calls.
def image(name, instructions, records, extent):


    # Encode section words in canonical big-endian order.
    def words(*values):
        return struct.pack('>'+'I'*len(values), *values)
    module = words(0, 1, extent, 0, 0, 1)
    types = words(2, extent, 4)+bytes((extent+63)//64)+words(64, 8)+bytes(1)
    data = words(len(records))
    for offset, kind, value in records:
        width = 4 if kind == 1 else 8
        data += words(offset, kind)+(value & ((1 << (8*width))-1)).to_bytes(width, 'big')
    pool = words(1, len(name))+b''.join(words(ord(c)) for c in name)
    sections = [(1, module), (2, words(len(instructions))+b''.join(instructions)), (3, types), (4, data), (7, pool)]
    cursor = 32+12*len(sections)
    directory = b''
    for kind, payload in sections:
        directory += struct.pack('>HHII', kind, 1, cursor, len(payload))
        cursor += len(payload)
    return b'Dis64\r\n\x1a'+struct.pack('>HHIIIII', 1, 0, 92, 1, 0, 0, 5)+directory+b''.join(p for _, p in sections)


# Independent wire instructions never inspect emitted target code or C handler results.
def instruction(name, mode, *values):
    return bytes([OPCODES['I'+name.upper()], mode])+b''.join(OPERAND(value) for value in values)


# Unbounded Python integers define modular results, including signed MIN divided by minus one.
def arithmetic():
    for suffix, bits, kind in (('w', 32, 1), ('l', 64, 2)):
        cases = []
        mask = (1 << bits)-1
        for left in (0, 1, -1, -(1 << (bits-1)), (1 << (bits-1))-1):
            for right in (-32768, -1, 0, 1, 32767):
                for op, result in (('add', left+right), ('sub', left-right), ('mul', left*right),
                                   ('and', left & right), ('or', left | right), ('xor', left ^ right)):
                    cases.append((op+suffix, right, left, result & mask))
            for count in (0, 1, bits-2, bits-1):
                cases += [('shl'+suffix, count, left, (left << count) & mask),
                          ('shr'+suffix, count, left, (left >> count) & mask)]
            for right in (-1, 1, 3):
                quotient = abs(left)//abs(right)*(1 if (left < 0) == (right < 0) else -1)
                cases += [('div'+suffix, right, left, quotient & mask),
                          ('mod'+suffix, right, left, (left-quotient*right) & mask)]
        # Respect the existing 1024-initializer limit.
        for start in range(0, len(cases), 256):
            yield suffix, kind, cases[start:start+256]


# A result mismatch branches to a deliberate language error rather than a successful exit.
def programs():
    for group, (suffix, kind, cases) in enumerate(arithmetic()):
        name = 'BootArithmetic'+str(group)
        records, code = [], []
        failure = 2*len(cases)+1
        for index, (op, right, left, expected) in enumerate(cases):
            offset = index*16
            records += [(offset, kind, left), (offset+8, kind, expected)]
            code += [instruction(op, 0x10, right, offset),
                     instruction('bne'+suffix, 0xc2, offset+8, offset, failure)]
        code += [instruction('exit', 0x1b), instruction('divw', 0x10, 0, 0)]
        yield name, image(name, code, records, 16*len(cases)), '', len(cases)
        if group == 0:
            wrong = list(records)
            offset, fieldkind, expected = wrong[1]
            wrong[1] = (offset, fieldkind, expected ^ 1)
            yield name, image(name, code, wrong, 16*len(cases)), 'zero divide', 1
    for suffix, kind in (('w', 1), ('l', 2)):
        name = 'BootQuantum'+suffix
        code = [instruction('add'+suffix, 0x10, 1, 0),
                instruction('blt'+suffix, 0xc2, 8, 0, 0),
                instruction('bne'+suffix, 0xc2, 8, 0, 4),
                instruction('exit', 0x1b), instruction('divw', 0x10, 0, 0)]
        yield name, image(name, code, [(0, kind, 0), (8, kind, 4097)], 16), '', 1
        for operation, value in (('div', 0), ('mod', 0), ('shl', -1), ('shr', 32 if suffix == 'w' else 64)):
            name = 'BootError'+operation+suffix
            code = [instruction(operation+suffix, 0x10, value, 0), instruction('exit', 0x1b)]
            error = 'zero divide' if value == 0 else 'invalid argument'
            yield name, image(name, code, [(0, kind, -1)], 8), error, 1
    yield branches()
    yield moves()
    # LONG comparisons check exact Real bits, including NaN payloads and signed zero.
    for index, bits in enumerate((0, 0x8000000000000000, 0x7ff0000000000000, 0x7ff8000000001234)):
        name = 'BootRealBits'+str(index)
        code = [instruction('movf', 0, 0, 8), instruction('bnel', 0xc2, 0, 8, 3),
                instruction('exit', 0x1b), instruction('divw', 0x10, 0, 0)]
        yield name, image(name, code, [(0, 3, bits)], 16), '', 1


# Exercise all six scalar predicates, both outcomes and finite/infinite Real operands.
def branches():
    records, code = [], []
    predicates = 0
    for suffix, kind in (('w', 1), ('l', 2), ('f', 3)):
        pairs = [(-1, 0), (0, 0), (1, 0)]
        if suffix == 'f':
            pairs += [(float('inf'), 1), (-float('inf'), 1)]
        else:
            bits = 32 if suffix == 'w' else 64
            pairs += [(-(1 << (bits-1)), (1 << (bits-1))-1), ((1 << (bits-1))-1, -(1 << (bits-1)))]
        for left, right in pairs:
            offset = len(records)*8
            values = (left, right)
            if suffix == 'f':
                values = tuple(int.from_bytes(struct.pack('>d', value), 'big') for value in values)
            records += [(offset, kind, values[0]), (offset+8, kind, values[1])]
            for op, taken in (('beq', left == right), ('bne', left != right), ('blt', left < right),
                              ('ble', left <= right), ('bgt', left > right), ('bge', left >= right)):
                at = len(code)
                code.append(instruction(op+suffix, 0xc2, offset+8, offset, at+2))
                if taken:
                    code.append(instruction('divw', 0x10, 0, 0))
                else:
                    code += [instruction('jmp', 0x1a, at+3), instruction('divw', 0x10, 0, 0)]
                predicates += 1
    code.append(instruction('exit', 0x1b))
    return 'BootBranches', image('BootBranches', code, records, len(records)*8), '', predicates


# Include memory and sign-extended immediate LONG moves in the whole-process profile.
def moves():
    code = [instruction('movl', 0x10, -1, 0), instruction('bnel', 0xc2, 8, 0, 7),
            instruction('movl', 0, 0, 16), instruction('bnel', 0xc2, 8, 16, 7),
            instruction('movw', 0x10, -1, 16), instruction('bnew', 0xc2, 24, 16, 7),
            instruction('exit', 0x1b), instruction('divw', 0x10, 0, 0)]
    return 'BootLongMoves', image('BootLongMoves', code, [(8, 2, -1), (24, 1, -1)], 32), '', 3
