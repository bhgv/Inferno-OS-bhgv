#!/usr/bin/env python3
"""Check canonical typed return stores against numeric and independent boolean fixed points."""
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
SPEC = importlib.util.spec_from_file_location('catalog_oracle', Path(__file__).with_name('dis64-bound-numeric.py'))
BASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BASE)
INIT, OP = BASE.INIT, BASE.OP
I, IMAGE, words = BASE.I, BASE.IMAGE, BASE.words
EXIT, RET = BASE.EXIT, BASE.RET
LIMIT = 134 * 65536


# Serialize existing indirect framing, retaining separately mutable base and displacement.
def store(kind=1, source=7, sm=2, base=32, secondary=0, size=None):
    name = 'IMOV' + ('L' if kind == 2 else 'F' if kind == 3 else 'W')
    return bytes((OP.CODES[name], sm * 8 + 5)) + OP.operand(source, size) + OP.operand(base, size) + OP.operand(secondary, size)


# Keep canonical result kinds separate from the actual return-store opcodes and declarations.
def fixture(bodies, slots, parameters=None, results=None, **kwargs):
    case = list(BASE.fixture(bodies, slots, parameters, **kwargs))
    case[10] = LIMIT
    results = results if results is not None else [0] + [1] * (len(bodies) - 1)
    raw = bytearray(case[1])
    at = 4
    for _ in range(int.from_bytes(raw[:4], 'big')):
        symbol, pc, frame, size = struct.unpack('>IIII', raw[at:at + 16])
        raw[at + 20:at + 24] = words(results[symbol])
        at += 16 + size
    case[1] = bytes(raw)
    return tuple(case)


# Parse actual bytes independently; only exact MOV indirect destinations gain a private role.
def decode(code):
    count = int.from_bytes(code[:4], 'big')
    at, decoded, stores = 4, [], {}
    for pc in range(count):
        if at + 2 > len(code):
            raise ValueError('short instruction')
        opcode, mode = code[at:at + 2]
        at += 2
        name = OP.NAMES[opcode] if opcode < len(OP.NAMES) else 'UNKNOWN'
        sm, dm, mm = mode >> 3 & 7, mode & 7, mode & 192
        if dm == 5:
            if name not in OP.MOVES or sm not in (1, 2) or mm:
                raise ValueError('indirect role')
            source, at = OP.decode(code, at)
            base, at = OP.decode(code, at)
            secondary, at = OP.decode(code, at)
            if base != 32 or secondary:
                raise ValueError('result address')
            stores[pc] = (name, sm, source)
            decoded.append(('IJMP', 26, 0, 0, pc + 1))
            continue
        if sm > 3 or dm > 3:
            raise ValueError('other indirect')
        middle = source = destination = 0
        if mm:
            middle, at = OP.decode(code, at)
        if sm != 3:
            source, at = OP.decode(code, at)
        if dm != 3:
            destination, at = OP.decode(code, at)
        decoded.append((name, mode, source, middle, destination))
    if at != len(code):
        raise ValueError('trailing code')
    return decoded, stores


# Metadata/code validation is independent of C; numeric states use synchronous TOP intersection.
def oracle(case):
    code, exports, pool, types, objlimit, entry, frame, locals_, workcap, capacity, limit = case
    if len(code) < 4 or len(code) > 1048576 or not 1 <= int.from_bytes(code[:4], 'big') <= min(workcap, 65536) or not 1 <= limit <= LIMIT:
        return None
    try:
        decoded, stores = decode(code)
        proxy = IMAGE([I(n, m, source=s, middle=h, destination=d) for n, m, s, h, d in decoded])
        canonical = BASE.oracle((proxy, *case[1:10], INIT.LIMIT))
        if canonical is None:
            return None
        reader = BASE.Reader(exports)
        signatures = {}
        for _ in range(reader.word()):
            symbol, pc, typeid, length = [reader.word() for _ in range(4)]
            signatures[symbol, pc, typeid] = reader.take(length)
        reader = BASE.Reader(types)
        frames = []
        for _ in range(reader.word()):
            size, align = reader.word(), reader.word()
            reader.take((size + 63) // 64)
            frames.append(size)
        results, spent = [], 0
        for symbol, first, end, typeid, *_ in canonical:
            sig = signatures[symbol, first, typeid]
            kind = int.from_bytes(sig[4:8], 'big')
            slots = next(record[3] for record in locals_ if record[:3] == (symbol, first, typeid))
            body = []
            for n, m, s, h, d in decoded[first:end]:
                if n == 'IRET':
                    body.append(EXIT)
                    continue
                if n in OP.BRANCH or n == 'IJMP':
                    d -= first
                body.append(I(n, m, source=s, middle=h, destination=d))
            localcode = IMAGE(body)
            numeric = INIT.oracle(frames[typeid], slots, sig, localcode, end - first, INIT.LIMIT)
            if numeric is None:
                return None
            summary, incoming = numeric
            extra_reads, store_pcs = 0, set()
            for pc in range(first, end):
                if pc not in stores:
                    continue
                name, sm, source = stores[pc]
                expected = 'IMOV' + ('L' if kind == 2 else 'F' if kind == 3 else 'W')
                if not kind or name != expected:
                    return None
                read = 0
                if sm == 2:
                    if kind == 3 or kind == 4 and source not in (0, 1):
                        return None
                else:
                    matches = [i + 1 for i, declaration in enumerate(slots) if declaration == (source, kind)]
                    if len(matches) != 1:
                        return None
                    read = matches[0]
                local = pc - first
                store_pcs.add(local)
                if local in incoming:
                    if read and read not in incoming[local]:
                        return None
                    extra_reads += bool(read)
            facts = OP.oracle(frames[typeid], slots, localcode, end - first)
            successors = [[] if f[7] == 3 else [f[6]] if f[7] == 1 else
                          [i + 1, f[6]] if f[7] == 2 else [i + 1] for i, f in enumerate(facts)]
            reached = set(incoming)
            returns = {pc for pc in reached if decoded[first + pc][0] == 'IRET'}
            resultwork = 0
            if kind:
                if not returns:
                    return None
                predecessors = {pc: [] for pc in reached}
                for pc in reached:
                    for target in successors[pc]:
                        predecessors[target].append(pc)
                outgoing = dict.fromkeys(reached, True)
                for _ in range(len(reached) + 2):
                    states, updated = {}, {}
                    for pc in reached:
                        sources = [outgoing[p] for p in predecessors[pc]]
                        if pc == 0:
                            sources.append(False)
                        states[pc] = all(sources)
                        updated[pc] = states[pc] or pc in store_pcs
                    if outgoing == updated:
                        break
                    outgoing = updated
                else:
                    raise RuntimeError('boolean reference convergence')
                if any(not states[pc] for pc in returns):
                    return None
                # Independent DFS computes deterministic work after the boolean proof succeeds.
                prefix, pending = set(), [0]
                while pending:
                    pc = pending.pop()
                    if pc in prefix:
                        continue
                    prefix.add(pc)
                    nexts = [] if pc in store_pcs else successors[pc]
                    resultwork += 1 + len(nexts)
                    pending.extend(nexts)
            summary = (*summary[:7], summary[7] + extra_reads, summary[8])
            spent += summary[6] + resultwork
            if spent > limit:
                return None
            results.append((symbol, first, end, typeid, *summary, kind, len(returns), len(store_pcs & reached), resultwork))
        return results
    except (ValueError, IndexError, KeyError, struct.error, OverflowError):
        return None


# Helpers retain global Dis-PC targets even when fixture bodies are written as local graphs.
def branch(target):
    return INIT.branch(target + 1)


# Helpers start at global PC1 in the two-function fixtures below.
def jump(target):
    return INIT.jump(target + 1)


# Exercise typed stores, initialization, every return path, loops, bounds and late atomic refusal.
def cases():
    cases = []
    small = ((64, 1), (68, 1))

    # A canonical root and helper allow each raw body to remain separately mutable.
    def add(body, slots=small, args=(), kind=1, frame=80):
        cases.append(fixture([[EXIT], body], [(), slots], [(), args], [0, kind], sizes=[frame]))

    add([RET])
    add([INIT.move(1, 64), RET])
    add([store(base=40), RET])
    add([store(secondary=4), RET])
    add([store(2), RET])
    add([store(source=2), RET], kind=4)
    add([store(), RET], kind=0)
    add([store(source=64, sm=1), RET])
    add([branch(2), store(), RET])
    add([jump(0), RET])
    add([store(), RET])
    add([branch(3), store(), jump(4), store(source=9), RET])
    add([branch(4), INIT.move(1, 64), store(source=64, sm=1), jump(5), store(), RET])
    add([branch(2), jump(0), store(), RET])
    add([store(), branch(3), jump(1), RET])
    add([store(), RET, store(source=64, sm=1)])
    add([RET], kind=0)
    add([jump(0)], kind=0)
    # Terminal malformed store still refuses; valid unreachable uninitialized store may remain.
    add([store(), RET, store(source=64, sm=1), RET])
    add([store(), RET, store(secondary=1), RET])
    for kind in (1, 2, 3, 4):
        width = OP.WIDTH[kind]
        slots = ((64, kind), (64 + width, kind))
        frame = ((64 + 2 * width + 8 + 7) // 8) * 8
        add([store(kind, 64, 1), RET], slots, (kind,), kind, frame)
        add([store(kind, 64, 1), RET], slots, (), kind, frame)
        for other in (1, 2, 3, 4):
            other_slots = ((64, other),)
            add([store(kind, 64, 1), RET], other_slots, (other,), kind, frame)
        for literal in (-536870912, -1, 0, 1, 2, 536870911):
            add([store(kind, literal), RET], (), (), kind, 72)
        for base in (0, 24, 28, 32, 36, 40, 48, 56, 64, -1, 536870911):
            add([store(kind, 0, base=base), RET], (), (), kind, 72)
        for secondary in (-536870912, -8, -1, 0, 1, 4, 8, 536870911):
            add([store(kind, 0, secondary=secondary), RET], (), (), kind, 72)
    # Independent predecessor intersections: either arm may omit the required store.
    for left in (False, True):
        for right in (False, True):
            body = [branch(3), store() if left else INIT.move(1, 64), jump(4),
                    store() if right else INIT.move(2, 64), RET]
            add(body)
    rng = random.Random(80)
    for size in range(2, 15):
        for _ in range(18):
            body = []
            for pc in range(size - 1):
                choice = rng.randrange(5)
                body.append(store(source=rng.randrange(3)) if choice == 0 else
                            INIT.move(1, 64) if choice == 1 else
                            branch(rng.randrange(size)) if choice == 2 else
                            jump(rng.randrange(size)) if choice == 3 else RET)
            body.append(RET)
            add(body)
    # Long delayed predecessor and branch cycles must not hide a missing return write.
    for length in (2, 8, 31, 128):
        body = [branch(2), store()] + [jump(pc + 1) for pc in range(2, length + 2)] + [RET]
        add(body)
        body[0] = store()
        add(body)
    # Real local bit128 remains available; return state never steals a numeric bitmap bit.
    for count in (31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128):
        slots = tuple((64 + i * 4, 1) for i in range(count))
        frame = ((64 + count * 4 + 8 + 7) // 8) * 8
        add([INIT.move(5, slots[-1][0]), store(source=slots[-1][0], sm=1), RET], slots, (), 1, frame)
    # Wrong middle/source/destination modes, compact operand lengths, all prefixes and trailers.
    valid = fixture([[EXIT], [store(), RET]], [(), ()], sizes=[72])
    for mode in range(256):
        instruction = bytes((OP.CODES['IMOVW'], mode))
        if mode & 192:
            instruction += OP.operand(0)
        if mode >> 3 & 7 != 3:
            instruction += OP.operand(0)
        if mode & 7 != 3:
            instruction += OP.operand(32)
            if mode & 7 > 3:
                instruction += OP.operand(0)
        changed = list(valid)
        changed[0] = IMAGE([EXIT, instruction, RET])
        cases.append(tuple(changed))
    for size in (1, 2, 4):
        add([store(source=1, size=size), RET], (), (), 1, 72)
    for section in range(4):
        for end in range(len(valid[section])):
            changed = list(valid)
            changed[section] = valid[section][:end]
            cases.append(tuple(changed))
        changed = list(valid)
        changed[section] += b'\0'
        cases.append(tuple(changed))
    for kind in (0, 1, 2, 3, 4):
        cases.append(fixture([[store(kind), EXIT]], [()], results=[0], sizes=[72]))
    # Nonvoid helper failure AFTER an earlier function succeeds must preserve all final bytes.
    cases.append(fixture([[EXIT], [RET], [RET]], [(), (), ()], results=[0, 0, 1], sizes=[72]))
    for count in (64, 65):
        cases.append(fixture([[EXIT]] + [[store(), RET] for _ in range(count - 1)], [()] * count,
                             order=list(reversed(range(count))), sizes=[72]))
    for count in (65536, 65537):
        changed = list(fixture([[EXIT] + [EXIT] * (count - 4), [store(), RET, RET]], [(), ()], sizes=[72]))
        changed[8] = 65536
        cases.append(tuple(changed))
    for case in cases[:80].copy():
        expected = oracle(case)
        if expected is not None:
            cost = sum(row[10] + row[16] for row in expected)
            for quota in (cost, cost - 1, 1):
                changed = list(case)
                changed[10] = quota
                cases.append(tuple(changed))
    for field, values in ((8, (0, 1)), (9, (0, 1)), (10, (0, LIMIT, LIMIT + 1, 0xffffffff))):
        for value in values:
            changed = list(valid)
            changed[field] = value
            cases.append(tuple(changed))
    cases.extend(BASE.cases())
    return cases


# Compile the actual metadata-bound constructor; controls must reach exact semantic failures.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('symbol', 'entry', 'frame', 'signature', 'framebytes', 'interval', 'rebase', 'root_return', 'callee_exit', 'fence', 'output_capacity', 'work_capacity', 'budget', 'atomic', 'base', 'secondary', 'store_kind', 'bool', 'void', 'source', 'cut', 'branch', 'terminal', 'exists', 'reachable', 'return_budget'))
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
    with tempfile.TemporaryDirectory(prefix='dis64-bound-return-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = (ROOT / 'include/dis64-bound-return.h').read_text()
        mutations = {
            'symbol': ('locals[j].symbol == functions[i].symbol && ', ''),
            'entry': ('locals[j].entry == functions[i].entry &&', '1 &&'),
            'frame': ('locals[j].frame == functions[i].frame', '1'),
            'signature': ('functions[i].signature.data, functions[i].signature.bytes',
                          'functions[0].signature.data, functions[0].signature.bytes'),
            'framebytes': ('frames[i] = type.bytes;', 'frames[i] = 80;'),
            'interval': ('instruction, count, functions[owner].entry, functions[owner].end,',
                         'instruction, count, 0, count,'),
            'rebase': ('scratch[pc].fact.target -= functions[owner].entry;', 'scratch[pc].fact.target -= 0;'),
            'root_return': ('if(owner == 0 || instruction.mode !=', 'if(instruction.mode !='),
            'callee_exit': ('if(instruction.op == IEXIT && owner != 0)', 'if(0)'),
            'fence': ('pc+1 == functions[owner].end)', '0)'),
            'output_capacity': (' || functionsused > capacity', ''),
            'work_capacity': (' || count > workcapacity', ''),
            'budget': ('limit-spent, &results[i].function.numeric', 'limit, &results[i].function.numeric'),
            'atomic': ('results[i].function.frame = functions[i].frame;',
                       'results[i].function.frame = functions[i].frame;\n\t\toutput[i] = results[i];'),
            'base': ('base != 32', 'base < 0'),
            'secondary': ('secondary != 0', 'secondary < 0'),
            'store_kind': ('instruction->op != op', '0'),
            'bool': ('instruction->source, kind, &fact.source', 'instruction->source, kind == Dis64Bool ? Dis64Int : kind, &fact.source'),
            'void': ('kind == 0 || kind > Dis64Bool', 'kind > Dis64Bool'),
            'source': ('fact.reads = fact.source != 0;', 'fact.source = 0; fact.reads = 0;'),
            'cut': ('return fact->write == 0 &&', 'return fact->write != 0 ||'),
            'branch': ('edges = fact->edge == Dis64CFGBranch ? 2 : 1;', 'edges = 1;'),
            'terminal': ('if(fact->op == IRET)', 'if(0)'),
            'exists': ('if(result->returns == 0)', 'if(0)'),
            'reachable': ('if(scratch[pc].seen){', 'if(1){'),
            'return_budget': ('units = 1+edges;', 'units = 0;'),
        }
        if args.control:
            old, new = mutations[args.control]
            if header.count(old) != 1:
                raise RuntimeError('bound mutation marker drift: ' + args.control)
            header = header.replace(old, new)
        (work / 'dis64-bound-return.h').write_text(header)
        fixturepath = work / 'cases.bin'
        fixturepath.write_bytes(data)
        binary = work / 'bound'
        subprocess.run(compiler + flags + ['-I' + str(work), '-I' + str(ROOT / 'include'),
                       str(ROOT / 'utils/tests/dis64-bound-return.c'), '-o', str(binary)], env=env, check=True)
        result = subprocess.run(runner + [str(binary), str(fixturepath)], env=env,
                                capture_output=True, text=True, timeout=90)
        if args.control:
            if result.returncode != 10 or 'bound return predicate failed' not in result.stderr:
                raise RuntimeError('bound control missed semantic predicate: ' + result.stdout + result.stderr)
            print('PASS bound return removal control:', args.control)
        elif result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        else:
            print(result.stdout, end='')
            print('Independent numeric/boolean oracle:', positive, 'accepted,', len(fixtures) - positive, 'refused')


if __name__ == '__main__':
    main()
