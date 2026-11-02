#!/usr/bin/env python3
"""Derive typed numeric facts from real Dis64 bytes against independent Python parsing."""
import argparse
import os
from pathlib import Path
import random
import re
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NAMES = re.findall(r'^\s*(I[A-Z0-9]+),', (ROOT / 'include/isa.h').read_text().split('};', 1)[0], re.M)
CODES = {name: i for i, name in enumerate(NAMES)}
WIDTH = {1: 4, 2: 8, 3: 8, 4: 4}
MOVES = {'IMOVW': 1, 'IMOVL': 2, 'IMOVF': 3}
ARITH = {'I' + op + suffix: kind for suffix, kind in (('W', 1), ('L', 2))
         for op in ('ADD', 'SUB', 'MUL', 'DIV', 'MOD', 'AND', 'OR', 'XOR', 'SHL', 'SHR')}
BRANCH = {'I' + op + suffix: kind for suffix, kind in (('W', 1), ('L', 2), ('F', 3))
          for op in ('BEQ', 'BNE', 'BLT', 'BLE', 'BGT', 'BGE')}
SLOTS = ((64, 1), (68, 4), (72, 2), (80, 3), (88, 1), (92, 4), (96, 2), (104, 3))


# Encode compact signed operands mathematically; no production emitter is called.
def operand(value, size=None):
    size = size or (1 if -64 <= value <= 63 else 2 if -8192 <= value <= 8191 else 4)
    bits = value & ((1 << (7 if size == 1 else 14 if size == 2 else 30)) - 1)
    data = bytearray(bits.to_bytes(size, 'big'))
    data[0] |= 0 if size == 1 else 0x80 if size == 2 else 0xc0
    return bytes(data)


# Serialize existing opcode/mode roles, including malformed direct/indirect combinations.
def instruction(name, mode, source=0, middle=0, destination=0, size=None):
    data = bytes((CODES.get(name, 255), mode))
    if mode & 0xc0:
        data += operand(middle, size)
    if (mode >> 3) & 7 != 3:
        data += operand(source, size)
    if mode & 7 != 3:
        data += operand(destination, size)
    return data


# Existing code framing remains a count followed by unchanged compact instructions.
def image(instructions):
    return struct.pack('>I', len(instructions)) + b''.join(instructions)


# Independently parse signed7/14/30 using Python arbitrary-precision integers.
def decode(data, at):
    if at >= len(data):
        raise ValueError('short operand')
    first = data[at]
    size, bits = (1, 7) if first < 128 else (2, 14) if first < 192 else (4, 30)
    if at + size > len(data):
        raise ValueError('truncated operand')
    value = int.from_bytes(data[at:at + size], 'big') & ((1 << bits) - 1)
    if value >= 1 << (bits - 1):
        value -= 1 << bits
    return value, at + size


# Derive facts from exact typed intervals, not C width or opcode-role helpers.
def oracle(frame, slots, data, capacity):
    if frame < 72 or frame % 8 or len(slots) > 128:
        return None
    end = 64
    for offset, kind in slots:
        width = WIDTH.get(kind, 0)
        if not width or offset < end or offset % width or offset + width > frame - 8:
            return None
        end = offset + width
    if len(data) < 4:
        return None
    count = int.from_bytes(data[:4], 'big')
    if not 1 <= count <= 65536 or count > capacity or count > (len(data) - 4) // 2:
        return None
    declarations = {offset: (i + 1, kind) for i, (offset, kind) in enumerate(slots)}
    facts = []
    at = 4

    # An immediate has no slot ID; declared scalar kind is stronger than width equality.
    def read(mode, value, kind):
        if mode == 2:
            if kind == 3 or (kind == 4 and value not in (0, 1)):
                raise ValueError('literal kind')
            return 0
        if mode != 1 or value not in declarations or declarations[value][1] != kind:
            raise ValueError('typed span')
        return declarations[value][0]

    try:
        for _ in range(count):
            if at + 2 > len(data):
                return None
            code, mode = data[at:at + 2]
            at += 2
            sm, dm, mm = (mode >> 3) & 7, mode & 7, mode & 0xc0
            if sm > 3 or dm > 3:
                return None
            middle = source = destination = 0
            if mm:
                middle, at = decode(data, at)
            if sm != 3:
                source, at = decode(data, at)
            if dm != 3:
                destination, at = decode(data, at)
            name = NAMES[code] if code < len(NAMES) else 'UNKNOWN'
            if name == 'IEXIT':
                if (sm, dm, mm) != (3, 3, 0):
                    return None
                facts.append((code, 0, 0, 0, 0, 0, 0, 3))
                continue
            if name == 'IJMP':
                if (sm, dm, mm) != (3, 2, 0) or not 0 <= destination < count:
                    return None
                facts.append((code, 0, 0, 0, 0, 0, destination, 1))
                continue
            if name not in MOVES and name not in ARITH and name not in BRANCH:
                return None
            move, branch = name in MOVES, name in BRANCH
            kind = MOVES.get(name, ARITH.get(name, BRANCH.get(name)))
            lm = dm if mm == 0 else 2 if mm == 0x40 else 1 if mm == 0x80 else 0
            lhs = destination if mm == 0 else middle
            if move and mm:
                return None
            if branch and (dm != 2 or not mm or not 0 <= destination < count):
                return None
            if name == 'IMOVW':
                kind = declarations.get(destination, (0, 1))[1] if dm == 1 else 1
                if kind not in (1, 4):
                    return None
            elif branch and kind == 1:
                kinds = [declarations.get(value, (0, 1))[1] if role == 1 else 1
                         for role, value in ((sm, source), (lm, lhs))]
                if 4 in kinds:
                    kind = 4
            sourcekind = 1 if name in ('ISHLL', 'ISHRL') else kind
            src = read(sm, source, sourcekind)
            dst = 0 if branch else read(dm, destination, kind) if dm == 1 else None
            if dst is None:
                return None
            mid = 0 if move else read(lm, lhs, kind)
            facts.append((code, int(bool(src)) + int(bool(mid)), src, mid, dst, kind,
                          destination if branch else 0, 2 if branch else 0))
    except ValueError:
        return None
    return facts if at == len(data) else None


# Cover exact kinds, old-destination reads, mixed-width shifts and every mode byte.
def cases():
    result = []

    # Keep fixture metadata and raw code independent so corruption cannot be hidden by re-emission.
    def add(code, slots=SLOTS, frame=120, capacity=None):
        count = int.from_bytes(code[:4], 'big') if len(code) >= 4 else 0
        result.append((frame, slots, code, min(count, 65536) if capacity is None else capacity))

    exitcode = instruction('IEXIT', (3 << 3) | 3)
    mov = instruction('IMOVW', (2 << 3) | 1, source=1, destination=64)
    add(image([mov, exitcode]))
    add(image([mov, instruction('UNKNOWN', (3 << 3) | 3)]))
    add(image([exitcode, instruction('UNKNOWN', (3 << 3) | 3)]))
    add(image([mov, exitcode]) + b'\0')
    add(image([instruction('IMOVW', (2 << 3) | 1, source=0, destination=112), exitcode]),
        slots=((112, 1),))
    add(image([mov, exitcode]), slots=((64, 1), (64, 4)))
    add(image([mov, exitcode]), capacity=0)
    for name in sorted(set(MOVES) | set(ARITH) | set(BRANCH) | {'IEXIT', 'IJMP', 'IFRAME', 'ICALL', 'IRET', 'ILEA', 'INOP'}):
        kind = MOVES.get(name, ARITH.get(name, BRANCH.get(name, 1)))
        source = {1: 64, 2: 72, 3: 80}[kind]
        destination = {1: 88, 2: 96, 3: 104}[kind]
        if name in ('ISHLL', 'ISHRL'):
            source = 64
        for mode in range(256):
            add(image([instruction(name, mode, source=source, middle=destination,
                                   destination=0 if name in BRANCH or name == 'IJMP' else destination), exitcode]))
    for name in sorted(MOVES | ARITH | BRANCH):
        for source in (-1, 0, 32, 60, 64, 65, 68, 72, 76, 80, 88, 92, 96, 104, 112, 116, 120, 536870911):
            for mode in ((1 << 3) | 1, (2 << 3) | 1, 0x80 | (1 << 3) | 1,
                         0x40 | (2 << 3) | 1, 0x80 | (1 << 3) | 2):
                add(image([instruction(name, mode, source=source, middle=96,
                                       destination=0 if name in BRANCH else 96), exitcode]))
    # Same-width kind mismatches, bool constants and a valid old-destination Big shift.
    for source in (64, 68, 72, 80, 88, 92, 96, 104):
        for destination in (64, 68, 72, 80, 88, 92, 96, 104):
            for name in MOVES:
                add(image([instruction(name, (1 << 3) | 1, source=source, destination=destination), exitcode]))
    for value in (-536870912, -8193, -8192, -65, -64, -1, 0, 1, 2, 63, 64, 8191, 8192, 536870911):
        for name in MOVES:
            for destination in (64, 68, 72, 80):
                for size in (None, 4):
                    add(image([instruction(name, (2 << 3) | 1, source=value, destination=destination, size=size), exitcode]))
    add(image([instruction('ISHLL', (1 << 3) | 1, source=64, destination=96), exitcode]))
    add(image([instruction('IADDW', (1 << 3) | 1, source=64, destination=88), exitcode]))
    # Bool equality can compare a bool slot with literal0/1, not an int slot of equal width.
    for source in (0, 1, 2, -1):
        for middle in (64, 68, 92):
            add(image([instruction('IBEQW', 0x80 | (2 << 3) | 2,
                                   source=source, middle=middle, destination=0), exitcode]))
    representative = image([instruction('IMOVW', (2 << 3) | 1, source=-8193, destination=64),
                            instruction('ISHLL', 0x80 | (2 << 3) | 1, source=3, middle=72, destination=96),
                            exitcode])
    for end in range(len(representative)):
        add(representative[:end], capacity=3)
    for frame in (0, 64, 71, 72, 119, 120, 128, 0xfffffff8, 0xffffffff):
        for slots in ((), ((0, 1),), ((60, 1),), ((64, 0),), ((64, 5),),
                      ((65, 1),), ((64, 2), (68, 1)), ((68, 1), (64, 1)),
                      ((64, 1), (64, 1)), ((0xfffffff0, 2),)):
            add(image([exitcode]), slots, frame)
    for count in (128, 129):
        slots = tuple((64 + 8 * i, 2) for i in range(count))
        add(image([instruction('IMOVL', (2 << 3) | 1, destination=slots[-1][0])]),
            slots, 72 + count * 8)
    rng = random.Random(77)
    for _ in range(900):
        code = []
        for _ in range(rng.randrange(1, 6)):
            name = rng.choice(tuple(sorted(MOVES | ARITH | BRANCH)))
            code.append(instruction(name, rng.randrange(256),
                                    source=rng.choice((0, 1, 2, 64, 68, 72, 80, 88, 96, 104, 112)),
                                    middle=rng.choice((0, 1, 64, 68, 72, 80, 88, 96, 104)),
                                    destination=rng.choice((0, 1, 64, 68, 72, 80, 88, 96, 104))))
        add(image(code))
    for count in (0, 1, 65536, 65537):
        add(image([exitcode] * count), slots=(), frame=72)
    return result


# Compile actual code/header changes and require exact semantic status10 for controls.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('narrow', 'width', 'tail', 'overlap', 'kind', 'bool',
                                              'implicit', 'shift', 'unreachable', 'atomic', 'capacity', 'publication'))
    args = parser.parse_args()
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    words = lambda *values: struct.pack('>' + 'I' * len(values), *values)
    fixtures = cases()
    data = bytearray(words(len(fixtures)))
    positive = 0
    for frame, slots, code, capacity in fixtures:
        expected = oracle(frame, slots, code, capacity)
        positive += expected is not None
        data += words(frame, len(slots))
        for offset, kind in slots:
            data += words(offset, kind)
        data += words(len(code), capacity) + code + words(int(expected is not None), len(expected or ()))
        if expected is not None:
            for fact in expected:
                data += words(*fact)
    with tempfile.TemporaryDirectory(prefix='dis64-local-operands-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = (ROOT / 'include/dis64-local-operands.h').read_text()
        code = (ROOT / 'include/dis64-code.h').read_text()
        mutations = {
            'tail': (' || width > tail-at', ''),
            'overlap': ('at < end', 'at < 64'),
            'kind': ('slots[i].kind == kind', 'dis64scalarwidth(slots[i].kind) == dis64scalarwidth(kind)'),
            'bool': ('(kind == Dis64Bool && offset != 0 && offset != 1)', '0'),
            'implicit': ('if(!move && !dis64localreadslot', 'if(!move && mm != AXNON && !dis64localreadslot'),
            'shift': ('? Dis64Int : kind;', '? kind : kind;'),
            'unreachable': ('!dis64localoperandscheck(&instruction, count, 0, count, slots, slotcount, framebytes, scratch+pc)',
                            '(pc == 0 && !dis64localoperandscheck(&instruction, count, 0, count, slots, slotcount, framebytes, scratch+pc))'),
            'atomic': ('at += used;', 'at += used;\n\t\tif(pc == 0) output[0] = scratch[0];'),
            'capacity': (' || count > capacity', ''),
            'publication': ('for(pc=0; pc<count; pc++)\n\t\toutput[pc] = scratch[pc];',
                            'for(pc=0; pc<=count; pc++)\n\t\toutput[pc] = scratch[pc];'),
        }
        if args.control in ('narrow', 'width'):
            old, new = (' || (unsigned long)offset > 0xffffffffUL', '') if args.control == 'narrow' else (
                'if(width != 4 && width != 8)', 'if(0)')
            if code.count(old) != 1:
                raise RuntimeError('slot mutation marker drift')
            code = code.replace(old, new)
        elif args.control:
            old, new = mutations[args.control]
            if header.count(old) != 1:
                raise RuntimeError('operand mutation marker drift')
            header = header.replace(old, new)
        (work / 'dis64-local-operands.h').write_text(header)
        (work / 'dis64-code.h').write_text(code)
        fixture = work / 'cases.bin'
        fixture.write_bytes(data)
        binary = work / 'operands'
        subprocess.run(compiler + flags + ['-I' + str(work), '-I' + str(ROOT / 'include'),
                       str(ROOT / 'utils/tests/dis64-local-operands.c'), '-o', str(binary)], env=env, check=True)
        run = subprocess.run(runner + [str(binary), str(fixture)], env=env,
                             capture_output=True, text=True, timeout=90)
        if args.control:
            if run.returncode != 10 or 'local operand predicate failed' not in run.stderr:
                raise RuntimeError('operand removal missed predicate: ' + run.stdout + run.stderr)
            print('PASS experimental operand removal control:', args.control)
        elif run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        else:
            print(run.stdout, end='')
            print('Independent raw-byte oracle:', positive, 'accepted,', len(fixtures) - positive, 'refused')


if __name__ == '__main__':
    main()
