#!/usr/bin/env python3
"""Compare raw linear caller/leaf composition with independent setup and value-state models."""
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
SPEC = importlib.util.spec_from_file_location('return_oracle', Path(__file__).with_name('dis64-bound-return.py'))
RETURNS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RETURNS)
BASE, INIT, OP = RETURNS.BASE, RETURNS.INIT, RETURNS.OP
I, IMAGE, words = BASE.I, BASE.IMAGE, BASE.words
EXIT, RET = BASE.EXIT, BASE.RET
LIMIT = 135 * 65536


# Encode existing indirect destination bytes, not a semantic setup event supplied to C.
def indirect(name, source, pointer, offset, sm=1):
    return bytes((OP.CODES[name], sm * 8 + 5)) + OP.operand(source) + OP.operand(pointer) + OP.operand(offset)


# FRAME and CALL retain ordinary opcode/mode numbers
# no new operation IDs are assigned.
def frame(typeid=1, pointer=40):
    return I('IFRAME', 17, source=typeid, destination=pointer)


# Exact canonical entry PC is separately mutable from the selected frame ordinal.
def call(entry, pointer=40):
    return I('ICALL', 10, source=pointer, destination=entry)


# Canonical exports/signatures/types are encoded independently of setup bytes.
def fixture(root, leaves=None, slots=None, signatures=None, results=None, sizes=None, frames=None, order=None):
    leaves = leaves if leaves is not None else [[RETURNS.store(), RET]]
    slots = slots if slots is not None else [((64, 1),), ()]
    signatures = signatures if signatures is not None else [()] * (len(leaves) + 1)
    results = results if results is not None else [0] + [1] * len(leaves)
    frames = frames if frames is not None else [0] + [1] * len(leaves)
    case = list(RETURNS.fixture([root] + leaves, slots, signatures, results, frames=frames,
                               sizes=sizes or [80, 72], order=order))
    case[10] = LIMIT
    return tuple(case)


# Own framing parser preserves all helper bytes for a reference-only root projection.
def decode(code):
    at, output = 4, []
    for _ in range(int.from_bytes(code[:4], 'big')):
        start = at
        if at + 2 > len(code):
            raise ValueError('short instruction')
        opcode, mode = code[at:at + 2]
        at += 2
        name = OP.NAMES[opcode] if opcode < len(OP.NAMES) else 'UNKNOWN'
        sm, dm, mm = mode >> 3 & 7, mode & 7, mode & 192
        middle = source = destination = secondary = 0
        if dm == 5:
            if mm or sm not in (1, 2):
                raise ValueError('indirect role')
            source, at = OP.decode(code, at)
            destination, at = OP.decode(code, at)
            secondary, at = OP.decode(code, at)
        else:
            if sm > 3 or dm > 3:
                raise ValueError('indirect source')
            if mm:
                middle, at = OP.decode(code, at)
            if sm != 3:
                source, at = OP.decode(code, at)
            if dm != 3:
                destination, at = OP.decode(code, at)
        output.append((name, mode, source, middle, destination, secondary, code[start:at]))
    if at != len(code):
        raise ValueError('trailing code')
    return output


# Validate canonical metadata/leaf bodies, then independently classify complete root islands.
def oracle(case):
    code, exports, pool, types, objlimit, entry, entryframe, locals_, workcap, capacity, limit = case
    if len(code) < 4 or len(code) > 1048576 or not 1 <= int.from_bytes(code[:4], 'big') <= min(workcap, 65536) or not 1 <= limit <= LIMIT:
        return None
    try:
        r = BASE.Reader(exports)
        bindings = []
        for _ in range(r.word()):
            symbol, pc, typeid, length = [r.word() for _ in range(4)]
            sig = r.take(length)
            bindings.append((symbol, pc, typeid, sig))
        if not bindings:
            return None
        bindings.sort(key=lambda row: row[1])
        end = bindings[1][1] if len(bindings) > 1 else int.from_bytes(code[:4], 'big')
        decoded = decode(code)
        if not 1 <= end <= len(decoded):
            return None
        # Reference-only erasure: new root parser checks EVERY original root instruction below.
        proxy = IMAGE([EXIT] * end + [row[6] for row in decoded[end:]])
        leafproof = RETURNS.oracle((proxy, *case[1:10], RETURNS.LIMIT))
        if leafproof is None:
            return None
        symbol, first, end, typeid = leafproof[0][:4]
        slots = next(row[3] for row in locals_ if row[:3] == (symbol, first, typeid))
        r = BASE.Reader(types)
        frames = []
        for _ in range(r.word()):
            size, alignment = r.word(), r.word()
            r.take((size + 63) // 64)
            frames.append(size)
        framebytes, tail = frames[typeid], frames[typeid] - 8
        entries = {row[1]: i for i, row in enumerate(bindings)}
        facts, counters = [None] * end, [0, 0, 0, 0, end]
        ready, pc = 0, 0

        # Resolve a full typed numeric source
        # Initialized sets are checked after classification.
        def read(mode, value, kind):
            if mode == 2:
                if kind == 3 or kind == 4 and value not in (0, 1):
                    raise ValueError('literal type')
                return 0
            if mode != 1 or (value, kind) not in slots:
                raise ValueError('source type')
            return slots.index((value, kind)) + 1

        while pc < end:
            name, mode, source, middle, destination, secondary, raw = decoded[pc]
            if name == 'IFRAME':
                if mode != 17 or destination not in (40, 48, 56):
                    return None
                start, pointer, typeid = pc, destination, source
                pc += 1
                while pc < end and decoded[pc][0] not in ('IFRAME', 'ICALL', 'IEXIT'):
                    pc += 1
                if pc == end or decoded[pc][0] != 'ICALL':
                    return None
                _, callmode, callpointer, _, target, _, _ = decoded[pc]
                chosen = entries.get(target, 0)
                if not chosen or callmode != 10 or callpointer != pointer or bindings[chosen][2] != typeid:
                    return None
                sig = bindings[chosen][3]
                sw = struct.unpack('>' + 'I' * (len(sig) // 4), sig)
                result, kinds = sw[1], sw[3:]
                offsets, at = {}, 64
                for index, kind in enumerate(kinds):
                    width = OP.WIDTH[kind]
                    at = (at + width - 1) // width * width
                    offsets[at] = (index, kind)
                    at += width
                ready, filled, bound = 0, set(), False
                facts[start] = ((), 0)
                for inside in range(start + 1, pc):
                    n, m, s, h, d, extra, _ = decoded[inside]
                    if d != pointer or m & 7 != 5 or m & 192:
                        return None
                    if n == 'ILEA':
                        if m != 13 or s != tail or extra != 32 or not result or bound:
                            return None
                        bound = True
                        counters[2] += 1
                        facts[inside] = ((), 0)
                    else:
                        if extra not in offsets:
                            return None
                        index, kind = offsets[extra]
                        expected = 'IMOV' + ('L' if kind == 2 else 'F' if kind == 3 else 'W')
                        if n != expected:
                            return None
                        sourceid = read(m >> 3 & 7, s, kind)
                        facts[inside] = ((sourceid,), 0)
                        filled.add(index)
                        counters[1] += 1
                if len(filled) != len(kinds) or result and not bound:
                    return None
                facts[pc] = ((), 0)
                counters[0] += 1
                ready = result
            elif name == 'ICALL':
                return None
            elif name in OP.MOVES and mode >> 3 & 7 == 1 and source == tail:
                kind = ready
                expected = 'IMOV' + ('L' if kind == 2 else 'F' if kind == 3 else 'W')
                if not kind or name != expected or mode != 9 or (destination, kind) not in slots:
                    return None
                facts[pc] = ((), slots.index((destination, kind)) + 1)
                counters[3] += 1
            elif name in ('ICVTWF', 'ICVTWL'):
                converted = 2 if name == 'ICVTWL' else 3
                if mode not in (9, 17) or (destination, converted) not in slots:
                    return None
                facts[pc] = ((read(mode >> 3 & 7, source, 1),), slots.index((destination, converted)) + 1)
            elif name == 'IEXIT':
                if mode != 27 or pc + 1 != end:
                    return None
                facts[pc] = ((), 0)
            else:
                normal = OP.oracle(framebytes, slots, IMAGE([raw, EXIT]), 2)
                if normal is None or normal[0][7] != 0:
                    return None
                facts[pc] = (normal[0][2:4], normal[0][4])
            pc += 1
        if decoded[end - 1][0] != 'IEXIT':
            return None
        assigned, missing, reads, writes = set(), 0, 0, 0
        for sources, destination in facts:
            missing += len(slots) - len(assigned)
            if any(source and source not in assigned for source in sources):
                return None
            reads += sum(bool(source) for source in sources)
            if destination:
                assigned.add(destination)
                writes += 1
        numeric = (end, len(slots), 0, end, end - 1, missing, 2 * end - 1 + missing, reads, writes)
        root = (*leafproof[0][:4], *numeric, 0, 0, 0, 0, *counters)
        output = [root] + [(*row, 0, 0, 0, 0, 0) for row in leafproof[1:]]
        cost = sum(row[10] + row[16] + row[21] for row in output)
        return output if cost <= limit else None
    except (ValueError, IndexError, KeyError, struct.error, OverflowError, TypeError):
        return None


# Whole raw modules cover ownership, leaf postconditions, argument typing and result invalidation.
def cases():
    output = []

    # Build a root with a placeholder CALL and then bind its actual global callee entry.
    def add(root, **kwargs):
        root = list(root)
        for i, data in enumerate(root):
            if data is None:
                root[i] = call(len(root))
        output.append(fixture(root, **kwargs))

    setup = [frame(), indirect('ILEA', 72, 40, 32), None, INIT.move(72, 64, 1), EXIT]
    add(setup)
    for pointer in (40, 48, 56):
        root = [frame(pointer=pointer), indirect('ILEA', 72, pointer, 32), call(5, pointer), INIT.move(72, 64, 1), EXIT]
        output.append(fixture(root))
    add([frame(), None, EXIT])
    add([call(3), EXIT])
    add([frame(pointer=64), indirect('ILEA', 72, 64, 32), None, EXIT])
    add([frame(typeid=0), indirect('ILEA', 72, 40, 32), None, EXIT])
    add([frame(), indirect('ILEA', 64, 40, 32), None, EXIT])
    add([frame(), indirect('ILEA', 72, 48, 32), None, EXIT])
    add([frame(), indirect('ILEA', 72, 40, 40), None, EXIT])
    add([frame(), indirect('ILEA', 72, 40, 32), indirect('ILEA', 72, 40, 32), None, EXIT])
    add([frame(), INIT.move(1, 64), indirect('ILEA', 72, 40, 32), None, EXIT])
    add([frame(), indirect('ILEA', 72, 40, 32), call(0), EXIT])
    add([frame(), indirect('ILEA', 72, 40, 32), call(5, 48), EXIT])
    add([frame(), indirect('ILEA', 72, 40, 32), call(6), EXIT])
    add([frame(), frame(), None, EXIT])
    add([frame(), EXIT])
    add([INIT.move(72, 64, 1), EXIT])
    add([frame(), indirect('ILEA', 72, 40, 32), None, INIT.move(72, 64, 1), EXIT], leaves=[[RET]])
    add([frame(), None, EXIT], leaves=[[RET]], results=[0, 0])
    add([frame(), indirect('ILEA', 72, 40, 32), None, EXIT], leaves=[[RET]], results=[0, 0])
    # Same frame shape does not select the callee: target identity controls its signature.
    for kind in (1, 2, 3, 4):
        letter = 'L' if kind == 2 else 'F' if kind == 3 else 'W'
        width = OP.WIDTH[kind]
        slots = ((64, kind), (64 + width, kind))
        rootframe = (64 + 2 * width + 8 + 7) // 8 * 8
        leafframe = (64 + width + 8 + 7) // 8 * 8
        producer = I('ICVTWF', 17, source=7, destination=64) if kind == 3 else INIT.move(1, 64, kind=letter)
        root = [producer, frame(), indirect('IMOV' + letter, 64, 40, 64),
                indirect('ILEA', rootframe - 8, 40, 32), None,
                INIT.move(rootframe - 8, 64 + width, 1, letter), EXIT]
        leaf = [RETURNS.store(kind, 64, 1), RET]
        args = dict(leaves=[leaf], slots=[slots, ((64, kind),)], signatures=[(), (kind,)],
                    results=[0, kind], sizes=[rootframe, leafframe])
        add(root, **args)
        for value in (0, 1, 2, -1):
            changed = root.copy()
            changed[2] = indirect('IMOV' + letter, value, 40, 64, 2)
            add(changed, **args)
        changed = root.copy()
        changed.pop(0)
        add(changed, **args)
        for displacement in (0, 32, 63, 64, 65, 68, 72, -1):
            changed = root.copy()
            changed[2] = indirect('IMOV' + letter, 64, 40, displacement)
            add(changed, **args)
        for op in ('IMOVW', 'IMOVL', 'IMOVF'):
            changed = root.copy()
            changed[2] = indirect(op, 64, 40, 64)
            add(changed, **args)
    # Reuse invalidates old cell even for a void call
    # copied numeric locals retain their values.
    for copy in (False, True):
        root = [frame(), indirect('ILEA', 72, 40, 32), call(8 if copy else 7)]
        if copy:
            root.append(INIT.move(72, 64, 1))
        root += [frame(), call(10 if copy else 9), INIT.move(72, 64, 1), EXIT]
        output.append(fixture(root, leaves=[[RETURNS.store(), RET], [RET]], slots=[((64, 1),), (), ()], results=[0, 1, 0]))
    # Repeated same-kind argument stores are legal
    # duplicate binding is not.
    for count in (1, 2, 3, 8, 31, 64):
        root = [frame()] + [indirect('IMOVW', 1, 40, 64, 2)] * count + [indirect('ILEA', 72, 40, 32), None, EXIT]
        add(root, leaves=[[RETURNS.store(1, 64, 1), RET]], slots=[(), ((64, 1),)], signatures=[(), (1,)], sizes=[80, 80])
    # At most128 numeric facts remain independent of pointer/result proof fields.
    for count in (31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128):
        slots = tuple((64 + 4 * i, 1) for i in range(count))
        size = (64 + 4 * count + 8 + 7) // 8 * 8
        add([frame(), indirect('ILEA', size - 8, 40, 32), None, INIT.move(size - 8, slots[-1][0], 1), EXIT], slots=[slots, ()], sizes=[size, 72])
    # New leaf normal-return guarantee is mandatory, including missing store on just one arm.
    for body in ([RET], [INIT.branch(3), RETURNS.store(), RET], [EXIT], [INIT.jump(5), RET]):
        add(setup, leaves=[body])
    # Ordinary root branches and nested caller bodies remain explicitly closed.
    for instruction in (INIT.branch(1), INIT.jump(0), RET, I('UNKNOWN', 27)):
        add([instruction, *setup])
    add([frame(), indirect('ILEA', 72, 40, 32), call(4, 48), EXIT])
    add([frame(), indirect('ILEA', 72, 40, 32), None, EXIT], leaves=[[RETURNS.store(1, 64, 1), RET]],
        slots=[(), ((64, 1),)], signatures=[(), (1,)], sizes=[80, 80])
    add(setup, leaves=[[RETURNS.store(4, 1), RET]], results=[0, 4])
    good = fixture([frame(), indirect('ILEA', 72, 40, 32), call(5), INIT.move(72, 64, 1), EXIT])
    for section in range(4):
        for length in range(len(good[section])):
            changed=list(good)
            changed[section]=good[section][:length]
            output.append(tuple(changed))
        changed=list(good)
        changed[section]+=b'\0'
        output.append(tuple(changed))
    for section in (1, 2, 3):
        for at in range(0, len(good[section])-3, 4):
            changed=list(good)
            raw=bytearray(good[section])
            raw[at:at+4]=words(0xffffffff)
            changed[section]=bytes(raw)
            output.append(tuple(changed))
    for pc in range(5):
        for mode in range(256):
            rows=decode(good[0])
            raw=bytearray(rows[pc][6])
            raw[1]=mode
            changed=list(good)
            changed[0]=IMAGE([bytes(raw) if i==pc else row[6] for i,row in enumerate(rows)])
            output.append(tuple(changed))
    for case in output[:120].copy():
        expected=oracle(case)
        if expected is not None:
            cost=sum(row[10]+row[16]+row[21] for row in expected)
            for quota in (cost,cost-1,1):
                changed=list(case)
                changed[10]=quota
                output.append(tuple(changed))
    # Two equal-shaped leaf frames have different signatures
    # copied results survive reuse.
    root = [frame(), indirect('ILEA', 80, 40, 32), call(11), INIT.move(80, 64, 1),
            frame(pointer=48), indirect('IMOVW', 64, 48, 64), indirect('ILEA', 80, 48, 32),
            call(13, 48), INIT.move(80, 68, 1), I('IADDW', 9, source=64, destination=68), EXIT]
    output.append(fixture(root, leaves=[[RETURNS.store(), RET], [RETURNS.store(1, 64, 1), RET]],
                          slots=[((64, 1), (68, 1)), (), ((64, 1),)],
                          signatures=[(), (), (1,)], sizes=[88, 80]))
    # Both argument bitmap words and canonical mixed alignment are used by a real call.
    for kinds in ((1,) * 64, (4,) * 64, (1, 4, 2, 3) * 16):
        root = [I('ICVTWF', 17, source=7, destination=64), frame()]
        declarations, at = [], 64
        for kind in kinds:
            width = OP.WIDTH[kind]
            at = (at + width - 1) // width * width
            declarations.append((at, kind))
            letter = 'L' if kind == 2 else 'F' if kind == 3 else 'W'
            root.append(indirect('IMOV' + letter, 64 if kind == 3 else 1, 40, at, 1 if kind == 3 else 2))
            at += width
        root += [indirect('ILEA', 80, 40, 32), None, INIT.move(80, 72, 1), EXIT]
        add(root, leaves=[[RETURNS.store(1, declarations[0][0], 1), RET]] if kinds[0] == 1 else [[RETURNS.store(1), RET]],
            slots=[((64, 3), (72, 1)), tuple(declarations)], signatures=[(), kinds],
            sizes=[88, (at + 8 + 7) // 8 * 8])
    for count in (64, 65):
        size = 2 * (count - 1) + 1
        root = []
        for callee in range(count - 1):
            root += [frame(), call(size + callee)]
        root.append(EXIT)
        output.append(fixture(root, leaves=[[RET]] * (count - 1), slots=[()] * count,
                              results=[0] * count, order=list(reversed(range(count)))))
    for count in (65536, 65537):
        changed = list(fixture([INIT.move(1, 64)] * (count - 2) + [EXIT],
                               leaves=[[RET]], results=[0, 0]))
        changed[8] = 65536
        output.append(tuple(changed))
    # Reuse the old raw body corpus under the stricter linear-entry call contract.
    output.append(fixture([frame(pointer=64), indirect('ILEA', 72, 64, 32), call(4, 64), EXIT]))
    output.append(fixture([INIT.move(1, 64), I('ICVTWF', 9, source=64, destination=72), EXIT],
                          leaves=[[RET]], results=[0, 0], slots=[((64, 4), (72, 3)), ()], sizes=[88, 72]))
    output.append(fixture([I('ICVTWF', 17, source=1, destination=64), EXIT],
                          leaves=[[RET]], results=[0, 0], slots=[((64, 2),), ()]))
    output.append(fixture([I('ICVTWL',17,source=-7,destination=64),EXIT],leaves=[[RET]],results=[0,0],slots=[((64,2),),()]))
    for sourcekind,destkind,initialized in ((1,2,True),(3,2,True),(1,3,True),(1,2,False)):
        root=([I('ICVTWF',17,source=3,destination=64)if sourcekind==3 else INIT.move(3,64)]if initialized else [])+[I('ICVTWL',9,source=64,destination=72),EXIT]
        output.append(fixture(root,leaves=[[RET]],results=[0,0],slots=[((64,sourcekind),(72,destkind)),()],sizes=[88,72]))
    output.extend(RETURNS.cases())
    return output


# Compile the actual metadata-bound constructor; controls must reach exact semantic failures.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('symbol', 'entry', 'frame', 'signature', 'callee_exit', 'work_capacity', 'output_capacity', 'budget', 'steps', 'atomic', 'pointer_temp', 'frame_type', 'call_pointer', 'bind_owner', 'bind_source', 'bind_destination', 'bind_width', 'argument_offset', 'argument_source', 'argument_complete', 'pending', 'linear', 'ready', 'result_kind', 'result_write', 'leaf_return', 'leaf_address', 'conversion_source', 'conversion_destination'))
    args = parser.parse_args()
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    fixtures = cases()
    data = bytearray(words(len(fixtures)))
    positive = 0
    for case in fixtures:
        code, exports, pool, types, objlimit, entry, frame, locals_, workcap, capacity, limit = case
        expected = oracle(case)
        positive += expected is not None
        data += words(len(code), len(exports), len(pool), len(types), objlimit, entry, frame,
                      len(locals_), workcap, capacity, limit) + code + exports + pool + types
        for symbol, pc, kind, slots in locals_:
            data += words(symbol, pc, kind, len(slots))
            for offset, scalar in slots:
                data += words(offset, scalar)
        data += words(int(expected is not None), len(expected or ()))
        if expected is not None:
            for row in expected:
                data += words(*row)
    with tempfile.TemporaryDirectory(prefix='dis64-bound-call-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = (ROOT / 'include/dis64-bound-call.h').read_text()
        mutations = {'symbol': ('locals[j].symbol == functions[i].symbol && ', ''),
         'entry': ('locals[j].entry == functions[i].entry &&', '1 &&'),
         'frame': ('locals[j].frame == functions[i].frame', '1'),
         'signature': ('functions[i].signature.data, functions[i].signature.bytes',
                       'functions[0].signature.data, functions[0].signature.bytes'),
         'callee_exit': ('if(instruction.op == IEXIT && owner != 0)', 'if(0)'),
         'work_capacity': (' || count > workcapacity', ''),
         'output_capacity': (' || functionsused > capacity', ''),
         'budget': ('limit-spent, &results[i].body.function.numeric',
                    'limit, &results[i].body.function.numeric'),
         'steps': ('spent = results[0].steps;', 'spent = 0;'),
         'atomic': ('results[i].body.function.frame = functions[i].frame;',
                    'results[i].body.function.frame = functions[i].frame;\n\t\toutput[i] = results[i];'),
         'pointer_temp': ('(instruction->destination != 40 && instruction->destination != 48 && '
                          'instruction->destination != 56)',
                          'instruction->destination < 0'),
         'frame_type': ('(unsigned long)instruction->source != functions[chosen].frame', '0'),
         'call_pointer': ('(unsigned long)instruction->source != pointer', '0'),
         'bind_owner': ('if((unsigned long)instruction->destination != pointer)', 'if(0)'),
         'bind_source': ('(unsigned long)instruction->source != plan.resultcell', '0'),
         'bind_destination': ('decoded[pc].secondary != 32', '0'),
         'bind_width': ('event.width = 8;', 'event.width = 4;'),
         'argument_offset': ('dis64callargumentindex(callee, decoded[pc].secondary, &index)',
                             'dis64callargumentindex(callee, 64, &index)'),
         'argument_source': ('fact.reads = fact.source != 0;\n\t\t\t\tevent.action',
                             'fact.source = 0; fact.reads = 0;\n\t\t\t\tevent.action'),
         'argument_complete': ('event.action = Dis64StateCall;',
                               'for(j=0; j<callees[state.callee-1].parameters; j++) state.arguments[j/32] '
                               '|= 1U<<(j%32);\n'
                               '\t\t\tevent.action = Dis64StateCall;'),
         'pending': ('}else if(state.site != 0){', '}else if(0){'),
         'linear': (' || fact.edge != Dis64CFGNext)', ' )'),
         'ready': ('result->calls++;',
                   'if(state.ready == 0) state.ready = Dis64Int;\n\t\t\tresult->calls++;'),
         'result_kind': ('instruction->destination, kind, &id)',
                         'instruction->destination, Dis64Int, &id)'),
         'result_write': ('fact.write = id;\n\t\t\tfact.kind = kind;',
                          'fact.write = 0;\n\t\t\tfact.kind = kind;'),
         'leaf_return': ('if(i != 0 && !dis64returncover(', 'if(0 && !dis64returncover('),
         'leaf_address': ('instruction.destination != 32 || ', ''),
         'conversion_source': ('instruction->source, Dis64Int, &fact.source)',
                               'instruction->source, Dis64Bool, &fact.source)'),
         'conversion_destination': ('kind = instruction->op == ICVTWL ? Dis64Big : Dis64Real;',
                                    'kind = instruction->op == ICVTWL ? Dis64Real : Dis64Big;')}
        if args.control:
            old, new = mutations[args.control]
            if header.count(old) != 1:
                raise RuntimeError('bound mutation marker drift: ' + args.control)
            header = header.replace(old, new)
        (work / 'dis64-bound-call.h').write_text(header)
        fixturepath = work / 'cases.bin'
        fixturepath.write_bytes(data)
        binary = work / 'bound'
        subprocess.run(compiler + flags + ['-I' + str(work), '-I' + str(ROOT / 'include'),
                       str(ROOT / 'utils/tests/dis64-bound-call.c'), '-o', str(binary)], env=env, check=True)
        result = subprocess.run(runner + [str(binary), str(fixturepath)], env=env,
                                capture_output=True, text=True, timeout=90)
        if args.control:
            if result.returncode != 10 or 'bound call predicate failed' not in result.stderr:
                raise RuntimeError('bound control missed semantic predicate: ' + result.stdout + result.stderr)
            print('PASS bound call removal control:', args.control)
        elif result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        else:
            print(result.stdout, end='')
            print('Independent capsule/leaf oracle:', positive, 'accepted,', len(fixtures) - positive, 'refused')


if __name__ == '__main__':
    main()
