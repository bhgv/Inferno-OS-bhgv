#!/usr/bin/env python3
"""Compare real scalar-code initialization with synchronous greatest-fixed-point sets."""
import argparse
import importlib.util
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('operand_oracle', Path(__file__).with_name('dis64-local-operands.py'))
OPERANDS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OPERANDS)
I = OPERANDS.instruction
IMAGE = OPERANDS.image
LIMIT = 131 * 65536
EXIT = I('IEXIT', 27)


# Existing version1 signatures encode only incoming scalar kinds for this conditional analysis.
def signature(kinds=(), result=0):
    return struct.pack('>III', 1, result, len(kinds)) + b''.join(struct.pack('>I', k) for k in kinds)


# Canonical parameter prefix, without inventing initialized facts for other declarations.
def seed_for(slots, encoded):
    if len(encoded) < 12 or len(encoded) % 4:
        return None
    words = struct.unpack('>' + 'I' * (len(encoded) // 4), encoded)
    version, result, count = words[:3]
    if version != 1 or result not in range(5) or count > 64 or count > len(slots) or len(words) != 3 + count:
        return None
    at = 64
    for i, kind in enumerate(words[3:]):
        width = OPERANDS.WIDTH.get(kind)
        if width is None:
            return None
        at = (at + width - 1) // width * width
        if slots[i] != (at, kind):
            return None
        at += width
    return frozenset(range(1, count + 1))


# Solve with global synchronous intersections from TOP, not the C FIFO/first-edge states.
def oracle(frame, slots, sig, code, capacity, limit):
    if not 1 <= limit <= LIMIT:
        return None
    facts = OPERANDS.oracle(frame, slots, code, capacity)
    seed = seed_for(slots, sig)
    if facts is None or seed is None:
        return None
    successors = []
    for pc, fact in enumerate(facts):
        edge, target = fact[7], fact[6]
        if edge in (0, 2) and pc + 1 == len(facts):
            return None
        successors.append([] if edge == 3 else [target] if edge == 1 else
                          [pc + 1, target] if edge == 2 else [pc + 1])
    reached = {0}
    stack = [0]
    while stack:
        for target in successors[stack.pop()]:
            if target not in reached:
                reached.add(target)
                stack.append(target)
    predecessors = {pc: [] for pc in reached}
    for pc in reached:
        for target in successors[pc]:
            predecessors[target].append(pc)
    top = frozenset(range(1, len(slots) + 1))
    outgoing = {pc: top for pc in reached}
    for _ in range(len(slots) * len(reached) + 2):
        incoming = {}
        updated = {}
        for pc in reached:
            sources = [outgoing[p] for p in predecessors[pc]]
            if pc == 0:
                sources.append(seed)
            state = frozenset.intersection(*sources)
            incoming[pc] = state
            updated[pc] = state | ({facts[pc][4]} if facts[pc][4] else set())
        if updated == outgoing:
            break
        outgoing = updated
    else:
        raise RuntimeError('independent fixed-point convergence bound failed')
    for pc in reached:
        if any(read and read not in incoming[pc] for read in facts[pc][2:4]):
            return None
    edges = sum(len(successors[pc]) for pc in reached)
    missing = sum(len(slots) - len(incoming[pc]) for pc in reached)
    work = len(reached) + edges + missing
    if work > limit:
        return None
    summary = (len(facts), len(slots), len(seed), len(reached), edges, missing, work,
               sum(facts[pc][1] for pc in reached), sum(bool(facts[pc][4]) for pc in reached))
    return summary, incoming


# Raw instruction constructors keep source/LHS/destination roles visible in graph fixtures.
def move(value, destination, source_mode=2, kind='W'):
    return I('IMOV' + kind, (source_mode << 3) | 1, source=value, destination=destination)


# Existing branch encoding; both immediate operands avoid introducing a spurious slot read.
def branch(target):
    return I('IBEQW', 0x40 | (2 << 3) | 2, source=0, middle=0, destination=target)


# Existing absolute Dis-PC jump encoding, not a private semantic edge annotation.
def jump(target):
    return I('IJMP', (3 << 3) | 2, destination=target)


# Exercise delayed-path loss, aliases, both masks, canonical arguments and unreachable roles.
def cases():
    result = []
    slots = ((64, 1), (68, 1))

    # Optional limits/capacities are independently checked; expected sets never come from C.
    def add(body, declarations=slots, args=(), frame=80, cap=None, limit=LIMIT, sig=None):
        code = body if isinstance(body, bytes) else IMAGE(body)
        count = int.from_bytes(code[:4], 'big') if len(code) >= 4 else 0
        result.append((frame, declarations, signature(args) if sig is None else sig, code,
                       min(count, 65536) if cap is None else cap, limit))

    add([EXIT, I('UNKNOWN', 27)])
    add([move(0, 64)])
    add([move(0, 64), EXIT])
    add([move(0, 64), EXIT], cap=0)
    add([move(0, 64), EXIT], limit=1)
    add([move(64, 68, 1), EXIT])
    add([I('IADDW', (2 << 3) | 1, source=1, destination=64), EXIT])
    add([move(0, 64), move(68, 64, 1), EXIT])
    add([move(64, 68, 1), EXIT], declarations=((64, 4), (68, 4)), args=(1,))
    add([move(68, 72, 1), EXIT], declarations=((68, 1), (72, 1)), args=(1,), frame=88)
    # The initialized path reaches read3 first; a delayed path later removes that fact.
    delayed = [branch(4), move(1, 64), jump(3), move(64, 68, 1), jump(5), jump(6), jump(3), EXIT]
    add(delayed)
    add(delayed, args=(1,))
    # One path assigns an otherwise UNUSED local: intersection accepts unequal states.
    add([branch(3), move(1, 64), jump(4), jump(4), EXIT])
    add([branch(3), move(1, 64), jump(4), jump(4), move(64, 68, 1), EXIT])
    add([branch(3), move(1, 64), jump(4), move(2, 64), move(64, 68, 1), EXIT])
    add([EXIT, move(64, 68, 1), EXIT])
    add([jump(0)])
    add([move(1, 64), I('IADDW', (2 << 3) | 1, source=1, destination=64), jump(1)])
    add([branch(0), EXIT])
    add([branch(0)])
    add([jump(1)])
    # Cover signatures and local ID boundaries in all four32-bit words.
    for size in (0, 1, 2, 31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128):
        declarations = tuple((64 + 4 * i, 1) for i in range(size))
        frame = ((64 + size * 4 + 8 + 7) // 8) * 8
        for arguments in (0, min(1, size), min(32, size), min(64, size)):
            body = [move(1, offset) for offset, kind in declarations[arguments:]]
            body += [I('IADDW', (1 << 3) | 1, source=offset, destination=offset)
                     for offset, kind in declarations]
            body.append(EXIT)
            add(body, declarations, (1,) * arguments, frame)
            if size:
                add([move(declarations[-1][0], declarations[0][0], 1), EXIT],
                    declarations, (1,) * arguments, frame)
    for kinds in ((), (1,), (4,), (2,), (3,), (1, 2, 4, 3), (4, 4, 1, 2)):
        declarations = []
        at = 64
        for kind in kinds:
            width = OPERANDS.WIDTH[kind]
            at = (at + width - 1) // width * width
            declarations.append((at, kind))
            at += width
        declarations = tuple(declarations)
        frame = (at + 8 + 7) // 8 * 8
        add([EXIT], declarations, kinds, frame)
        for i, (offset, kind) in enumerate(declarations):
            letter = 'W' if kind in (1, 4) else 'L' if kind == 2 else 'F'
            add([move(offset, offset, 1, letter), EXIT], declarations, kinds, frame)
            changed = list(kinds)
            changed[i] = 4 if kind == 1 else 1 if kind == 4 else 3 if kind == 2 else 2
            add([EXIT], declarations, tuple(changed), frame)
    # Every prefix/trailer in signature and code, plus malformed declaration geometry.
    sig = signature((1,))
    for end in range(len(sig)):
        add([EXIT], args=(1,), sig=sig[:end])
    add([EXIT], sig=sig + b'\0')
    code = IMAGE([move(1, 64), move(64, 68, 1), EXIT])
    for end in range(len(code)):
        add(code[:end], cap=3)
    add(code + b'\0')
    for declarations in (((64, 0),), ((64, 5),), ((65, 1),), ((64, 1), (64, 1)), ((72, 2), (64, 1))):
        add([EXIT], declarations)
    rng = random.Random(78)
    for _ in range(1400):
        size = rng.randrange(1, 24)
        args = (1,) * rng.randrange(3)
        body = []
        for pc in range(size):
            action = rng.randrange(7)
            if pc + 1 == size or action == 6:
                body.append(EXIT)
            elif action == 0:
                body.append(move(rng.randrange(3), rng.choice((64, 68))))
            elif action == 1:
                body.append(move(rng.choice((64, 68)), rng.choice((64, 68)), 1))
            elif action == 2:
                body.append(I('IADDW', (2 << 3) | 1, source=1, destination=rng.choice((64, 68))))
            elif action == 3:
                body.append(branch(rng.randrange(size)))
            else:
                body.append(jump(rng.randrange(size)))
        add(body, args=args)
    for count in (0, 1, 65536, 65537):
        body = [jump(i + 1) for i in range(max(0, count - 1))] + ([EXIT] if count else [])
        add(body, declarations=(), frame=72)
    # Independent exact quotas are derived from the synchronous final sets, not C visit order.
    originals = result.copy()
    for case in originals[:350]:
        expected = oracle(*case)
        if expected is not None:
            add(case[3], case[1], frame=case[0], sig=case[2], limit=expected[0][6])
            add(case[3], case[1], frame=case[0], sig=case[2], limit=expected[0][6] - 1)
    for limit in (0, LIMIT + 1, 0xffffffff):
        add([EXIT], limit=limit)
    return result


# Compile the actual solver; controls must hit semantic status10 rather than UB/crashes.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('intersection', 'source', 'middle', 'self_read', 'seed',
                                              'parameter_kind', 'parameter_span', 'requeue', 'capacity',
                                              'budget', 'reset', 'unreachable', 'publication'))
    args = parser.parse_args()
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    words = lambda *values: struct.pack('>' + 'I' * len(values), *values)
    fixtures = cases()
    data = bytearray(words(len(fixtures)))
    positive = 0
    for frame, slots, sig, code, capacity, limit in fixtures:
        expected = oracle(frame, slots, sig, code, capacity, limit)
        positive += expected is not None
        data += words(frame, len(slots))
        for offset, kind in slots:
            data += words(offset, kind)
        data += words(len(sig), len(code), capacity, limit) + sig + code
        data += words(int(expected is not None), *(expected[0] if expected is not None else (0,) * 9))
        if expected is not None:
            for pc in range(expected[0][0]):
                mask = sum(1 << (i - 1) for i in expected[1].get(pc, ()))
                data += words(int(pc in expected[1]), *((mask >> (32 * i)) & 0xffffffff for i in range(4)))
    with tempfile.TemporaryDirectory(prefix='dis64-initialized-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = (ROOT / 'include/dis64-local-initialized.h').read_text()
        mutations = {
            'intersection': ('target->incoming[j]&outgoing[j]', 'target->incoming[j]|outgoing[j]'),
            'source': ('!dis64initread(node->incoming, node->fact.source)', '0'),
            'middle': ('!dis64initread(node->incoming, node->fact.middle)', '0'),
            'self_read': ('if(!dis64initread(node->incoming, node->fact.source) || !dis64initread(node->incoming, node->fact.middle))',
                          'if((node->fact.source != node->fact.write && !dis64initread(node->incoming, node->fact.source)) || (node->fact.middle != node->fact.write && !dis64initread(node->incoming, node->fact.middle)))'),
            'seed': ('*parameters = signature.parameters;', 'for(i=0; i<count; i++) bits[i/32] |= 1U<<(i%32);\n\t*parameters = signature.parameters;'),
            'parameter_kind': (' || slots[i].kind != kind', ''),
            'parameter_span': ('slots[i].offset != at || ', ''),
            'requeue': ('if(!target->queued)', 'if(!target->queued && !target->processed)'),
            'capacity': (' || count > capacity', ''),
            'budget': ('if(summary->work > limit || units > limit-summary->work)', 'if(0)'),
            'reset': ('scratch[pc].seen = scratch[pc].queued = scratch[pc].processed = 0;',
                      'scratch[pc].seen = scratch[pc].queued = scratch[pc].processed = 1;'),
            'unreachable': ('!dis64localoperandscheck(&instruction, count, 0, count, slots, slotcount, framebytes, &scratch[pc].fact)',
                            '(pc == 0 && !dis64localoperandscheck(&instruction, count, 0, count, slots, slotcount, framebytes, &scratch[pc].fact))'),
            'publication': ('if(first){\n\t\t\tsummary.reads', 'if(first){\n\t\t\t*output = summary;\n\t\t\tsummary.reads'),
        }
        if args.control:
            old, new = mutations[args.control]
            if header.count(old) != 1:
                raise RuntimeError('initialized mutation marker drift: ' + args.control)
            header = header.replace(old, new)
        (work / 'dis64-local-initialized.h').write_text(header)
        fixture = work / 'cases.bin'
        fixture.write_bytes(data)
        binary = work / 'initialized'
        subprocess.run(compiler + flags + ['-I' + str(work), '-I' + str(ROOT / 'include'),
                       str(ROOT / 'utils/tests/dis64-local-initialized.c'), '-o', str(binary)], env=env, check=True)
        run = subprocess.run(runner + [str(binary), str(fixture)], env=env,
                             capture_output=True, text=True, timeout=90)
        if args.control:
            if run.returncode != 10 or 'local initialized predicate failed' not in run.stderr:
                raise RuntimeError('initialized control missed predicate: ' + run.stdout + run.stderr)
            print('PASS numeric initialized removal control:', args.control)
        elif run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        else:
            print(run.stdout, end='')
            print('Independent synchronous oracle:', positive, 'accepted,', len(fixtures) - positive, 'refused')


if __name__ == '__main__':
    main()
