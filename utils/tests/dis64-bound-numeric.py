#!/usr/bin/env python3
"""Bind actual code/metadata to per-function numeric proofs using independent parsers."""
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
SPEC = importlib.util.spec_from_file_location('initialized_oracle', Path(__file__).with_name('dis64-local-initialized.py'))
INIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(INIT)
OP = INIT.OPERANDS
I, IMAGE = OP.instruction, OP.image
EXIT = I('IEXIT', 27)
RET = I('IRET', 27)
LIMIT = INIT.LIMIT


# Encode already-agreed metadata records without using any production codec.
def words(*values):
    return struct.pack('>' + 'I' * len(values), *values)


# Fixtures keep raw metadata and declaration identities separately mutable.
def fixture(bodies, slots, signatures=None, frames=None, sizes=None, order=None, declarations=None):
    signatures = signatures or [()] * len(bodies)
    frames = frames or [0] * len(bodies)
    sizes = sizes or [80] * (max(frames) + 1)
    entries = []
    at = 0
    for body in bodies:
        entries.append(at)
        at += len(body)
    order = list(range(len(bodies))) if order is None else order
    exports = words(len(bodies))
    for i in order:
        sig = INIT.signature(signatures[i])
        exports += words(i, entries[i], frames[i], len(sig)) + sig
    pool = words(len(bodies))
    for i in range(len(bodies)):
        name = 'function' + str(i)
        pool += words(len(name), *(ord(c) for c in name))
    types = words(len(sizes)) + b''.join(words(size, 8) + bytes((size + 63) // 64) for size in sizes)
    locals_ = declarations if declarations is not None else [
        (i, entries[i], frames[i], tuple(slots[i])) for i in range(len(bodies))]
    code = IMAGE([instruction for body in bodies for instruction in body])
    return (code, exports, pool, types, 1048576, 0, frames[0], tuple(locals_), at, 64, LIMIT)


# Bounds-checked word reader independent of the C metadata views.
class Reader:

    # Keep a private cursor over the independent immutable payload.
    def __init__(self, data):
        self.data, self.at = data, 0

    # Every length must describe available immutable bytes before advancing.
    def take(self, length):
        if length > len(self.data) - self.at:
            raise ValueError('extent')
        result = self.data[self.at:self.at + length]
        self.at += length
        return result

    # Big-endian words match only the agreed wire framing, not native structure layout.
    def word(self):
        return int.from_bytes(self.take(4), 'big')

    # Trailing metadata bytes never become borrowed view slack.
    def end(self):
        if self.at != len(self.data):
            raise ValueError('trailer')


# Parse catalog/type/name metadata and actual code; rebase only in the independent oracle.
def oracle(case):
    code, exports, strings, typedata, objectlimit, entry, entryframe, locals_, workcap, capacity, limit = case
    if any(len(blob) > 1048576 for blob in (code, exports, strings, typedata)):
        return None
    if not 1 <= len(locals_) <= 64 or not 1 <= limit <= LIMIT or entry:
        return None
    try:
        r = Reader(strings)
        names = []
        for _ in range(r.word()):
            name = tuple(r.word() for _ in range(r.word()))
            # The agreed lossless Rune32 pool preserves every payload bit.
            names.append(name)
        r.end()
        r = Reader(typedata)
        types = []
        for _ in range(r.word()):
            size, alignment = r.word(), r.word()
            mapping = r.take((size + 63) // 64)
            if alignment not in (1, 2, 4, 8) or size % alignment or size > objectlimit:
                raise ValueError('type')
            if size == 0 and alignment != 1:
                raise ValueError('zero type')
            if any(mapping):
                raise ValueError('fixture NP0 type')
            types.append((size, alignment))
        r.end()
        count = int.from_bytes(code[:4], 'big') if len(code) >= 4 else 0
        if not 1 <= count <= 65536 or count > workcap or count > (len(code) - 4) // 2:
            return None
        r = Reader(exports)
        functions = []
        for _ in range(r.word()):
            symbol, pc, frame, sigbytes = (r.word() for _ in range(4))
            sig = r.take(sigbytes)
            if symbol >= len(names) or pc >= count or frame >= len(types) or len(sig) < 12 or len(sig) % 4:
                raise ValueError('binding')
            sw = struct.unpack('>' + 'I' * (len(sig) // 4), sig)
            if sw[0] != 1 or sw[1] not in range(5) or sw[2] > 64 or len(sw) != 3 + sw[2]:
                raise ValueError('signature')
            if any(kind not in OP.WIDTH for kind in sw[3:]):
                raise ValueError('parameter')
            size, alignment = types[frame]
            if size < 72 or alignment != 8:
                raise ValueError('frame shape')
            at = 64
            for kind in sw[3:]:
                width = OP.WIDTH[kind]
                at = (at + width - 1) // width * width + width
            if at > size - 8:
                raise ValueError('tail')
            functions.append((symbol, pc, frame, sig))
        r.end()
        if not 1 <= len(functions) <= 64 or len(functions) != len(locals_) or len(functions) > capacity:
            return None
        if len({names[f[0]] for f in functions}) != len(functions) or len({f[1] for f in functions}) != len(functions):
            return None
        functions.sort(key=lambda f: f[1])
        root = struct.unpack('>' + 'I' * (len(functions[0][3]) // 4), functions[0][3])
        if functions[0][1] != 0 or functions[0][2] != entryframe or root[1:3] != (0, 0):
            return None
        at = 4
        decoded = []
        for _ in range(count):
            if at + 2 > len(code):
                raise ValueError('short instruction')
            opcode, mode = code[at:at + 2]
            at += 2
            sm, dm, mm = (mode >> 3) & 7, mode & 7, mode & 0xc0
            if sm > 3 or dm > 3:
                raise ValueError('indirect')
            middle = source = destination = 0
            if mm:
                middle, at = OP.decode(code, at)
            if sm != 3:
                source, at = OP.decode(code, at)
            if dm != 3:
                destination, at = OP.decode(code, at)
            name = OP.NAMES[opcode] if opcode < len(OP.NAMES) else 'UNKNOWN'
            decoded.append((name, mode, source, middle, destination))
        if at != len(code):
            return None
        results = []
        spent = 0
        for i, (symbol, first, frame, sig) in enumerate(functions):
            end = functions[i + 1][1] if i + 1 < len(functions) else count
            matches = [record for record in locals_ if record[:3] == (symbol, first, frame)]
            if len(matches) != 1:
                return None
            slots = matches[0][3]
            body = []
            for name, mode, source, middle, destination in decoded[first:end]:
                if name == 'IRET':
                    if i == 0 or mode != 27:
                        return None
                    body.append(EXIT)
                    continue
                if name == 'IEXIT' and i != 0:
                    return None
                if name in OP.BRANCH or name == 'IJMP':
                    if not first <= destination < end:
                        return None
                    destination -= first
                body.append(I(name, mode, source=source, middle=middle, destination=destination))
            localcode = IMAGE(body)
            numeric = INIT.oracle(types[frame][0], slots, sig, localcode, end - first, LIMIT)
            if numeric is None:
                return None
            spent += numeric[0][6]
            if spent > limit:
                return None
            results.append((symbol, first, end, frame, *numeric[0]))
        return results
    except (ValueError, IndexError, KeyError, struct.error, OverflowError):
        return None


# Build actual function bodies with equal frame types but distinct identities and arguments.
def cases():
    output = []
    small = ((64, 1), (68, 1))
    base = fixture([[INIT.move(1, 64), EXIT], [INIT.move(64, 68, 1), RET]], [small, small], [(), (1,)])
    wrong = list(base)
    wrong[7] = ((99, 0, 0, small), base[7][1])
    output.append(tuple(wrong))
    wrong = list(base)
    wrong[7] = ((0, 99, 0, small), base[7][1])
    output.append(tuple(wrong))
    wrong = list(base)
    wrong[3] = words(2) + (words(80, 8) + bytes(2)) * 2
    wrong[7] = ((0, 0, 1, small), base[7][1])
    output.append(tuple(wrong))
    output.append(fixture([[EXIT], [RET]], [[(64, 1)], ()], sizes=[72]))
    output.append(fixture([[INIT.jump(1)], [RET]], [(), ()], sizes=[72]))
    output.append(fixture([[EXIT], [INIT.jump(1)]], [(), ()], sizes=[72]))
    output.append(fixture([[RET]], [()], sizes=[72]))
    output.append(fixture([[EXIT], [EXIT]], [(), ()], sizes=[72]))
    output.append(fixture([[INIT.move(1, 64)], [RET]], [small, small]))
    for index in (8, 9):
        changed = list(base)
        changed[index] = 0
        output.append(tuple(changed))
    quota = list(fixture([[EXIT], [RET]], [[(64, 1)], [(64, 1)]]))
    quota[10] = 3
    output.append(tuple(quota))
    output.append(fixture([[EXIT], [INIT.move(64, 68, 1), RET]], [small, small]))
    output.append(base)
    rng = random.Random(79)
    for count in range(1, 17):
        bodies = [[INIT.move(1, 64), EXIT]]
        declarations = [small]
        signatures = [()]
        for i in range(1, count):
            body = [INIT.move(64, 68, 1), RET] if i % 2 else [INIT.move(i, 68), RET]
            bodies.append(body)
            declarations.append(small)
            signatures.append((1,) if i % 2 else ())
        for _ in range(5):
            order = list(range(count))
            rng.shuffle(order)
            good = fixture(bodies, declarations, signatures, order=order)
            locals_ = list(good[7])
            rng.shuffle(locals_)
            output.append(good[:7] + (tuple(locals_),) + good[8:])
    # Mixed argument kinds and per-function frames must come from the exported signature/type.
    for kind in (1, 2, 3, 4):
        letter = 'W' if kind in (1, 4) else 'L' if kind == 2 else 'F'
        width = OP.WIDTH[kind]
        slots = ((64, kind), (64 + width, kind))
        frame = (64 + 2 * width + 8 + 7) // 8 * 8
        output.append(fixture([[EXIT], [INIT.move(64, 64 + width, 1, letter), RET]],
                              [(), slots], [(), (kind,)], frames=[0, 1], sizes=[72, frame]))
    # Local branch PC is rebased only after global ownership checks; no cross-entry fallthrough.
    for target in range(8):
        output.append(fixture([[EXIT], [INIT.branch(target), INIT.move(1, 64), INIT.jump(target), RET]],
                              [(), small]))
    for section in range(4):
        for length in range(len(base[section])):
            changed = list(base)
            changed[section] = base[section][:length]
            output.append(tuple(changed))
        changed = list(base)
        changed[section] += b'\0'
        output.append(tuple(changed))
    for localindex in range(2):
        for field in range(3):
            for value in (0, 1, 2, 99, 0xffffffff):
                locals_ = [list(record) for record in base[7]]
                locals_[localindex][field] = value
                output.append(base[:7] + (tuple(tuple(row) for row in locals_),) + base[8:])
    for index in (4, 5, 6, 8, 9, 10):
        for value in (0, 1, 2, 63, 64, 65536, LIMIT, LIMIT + 1, 0xffffffff):
            if index == 8 and value > 65536:
                continue
            changed = list(base)
            changed[index] = value
            output.append(tuple(changed))
    # Header corruption independently challenges canonical signature, entry, frame and pool IDs.
    for section in (1, 2, 3):
        for at in range(0, len(base[section]) - 3, 4):
            changed = list(base)
            raw = bytearray(base[section])
            raw[at:at + 4] = words(0xffffffff)
            changed[section] = bytes(raw)
            output.append(tuple(changed))
    for count in (64, 65):
        bodies = [[EXIT]] + [[RET] for _ in range(count - 1)]
        output.append(fixture(bodies, [()] * count, sizes=[72], order=list(reversed(range(count)))))
    # A nonvoid signature does not promise that RET writes a caller result: numeric-only proof.
    changed = list(base)
    raw = bytearray(changed[1])
    raw[52:56] = words(1)
    changed[1] = bytes(raw)
    output.append(tuple(changed))
    # Equal-shaped frames cannot hide duplicated or missing function declarations.
    output.append(base[:7] + ((base[7][0], base[7][0]),) + base[8:])
    # A root write must never seed an unrelated zero-argument callable entry.
    output.append(fixture([[INIT.move(1, 64), EXIT], [INIT.move(64, 68, 1), RET]], [small, small]))
    # Preflight includes unreachable bytes, empty RET role encoding and rejected call setup.
    for opcode in ('IFRAME', 'ICALL', 'ILEA', 'IRET'):
        for mode in (0, 27, 255):
            output.append(fixture([[EXIT, I(opcode, mode)], [RET]], [small, small]))
    # Full global backing bound; unreachable root exits avoid a quadratic oracle chain.
    for count in (65536, 65537):
        changed = list(fixture([[INIT.jump(0)] + [EXIT] * (count - 2), [RET]], [(), ()], sizes=[72]))
        changed[8] = 65536
        output.append(tuple(changed))
    # Aggregate exact/below quota, output capacity and code-work backing limits.
    for good in output[:120].copy():
        expected = oracle(good)
        if expected is not None:
            budget = sum(row[10] for row in expected)
            for limit in (budget, budget - 1):
                changed = list(good)
                changed[10] = limit
                output.append(tuple(changed))
    return output


# Compile the actual metadata-bound constructor; controls must reach exact semantic failures.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('symbol', 'entry', 'frame', 'signature', 'framebytes',
                                              'interval', 'rebase', 'root_return', 'callee_exit', 'fence',
                                              'output_capacity', 'work_capacity', 'budget', 'atomic'))
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
    with tempfile.TemporaryDirectory(prefix='dis64-bound-numeric-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = (ROOT / 'include/dis64-bound-numeric.h').read_text()
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
            'root_return': ('owner == 0 || ', ''),
            'callee_exit': ('if(instruction.op == IEXIT && owner != 0)', 'if(0)'),
            'fence': ('pc+1 == functions[owner].end)', '0)'),
            'output_capacity': (' || functionsused > capacity', ''),
            'work_capacity': (' || count > workcapacity', ''),
            'budget': ('limit-spent, &results[i].numeric', 'limit, &results[i].numeric'),
            'atomic': ('results[i].frame = functions[i].frame;',
                       'results[i].frame = functions[i].frame;\n\t\toutput[i] = results[i];'),
        }
        if args.control:
            old, new = mutations[args.control]
            if header.count(old) != 1:
                raise RuntimeError('bound mutation marker drift: ' + args.control)
            header = header.replace(old, new)
        (work / 'dis64-bound-numeric.h').write_text(header)
        fixturepath = work / 'cases.bin'
        fixturepath.write_bytes(data)
        binary = work / 'bound'
        subprocess.run(compiler + flags + ['-I' + str(work), '-I' + str(ROOT / 'include'),
                       str(ROOT / 'utils/tests/dis64-bound-numeric.c'), '-o', str(binary)], env=env, check=True)
        result = subprocess.run(runner + [str(binary), str(fixturepath)], env=env,
                                capture_output=True, text=True, timeout=90)
        if args.control:
            if result.returncode != 10 or 'bound numeric predicate failed' not in result.stderr:
                raise RuntimeError('bound control missed semantic predicate: ' + result.stdout + result.stderr)
            print('PASS bound numeric removal control:', args.control)
        elif result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        else:
            print(result.stdout, end='')
            print('Independent catalog/numeric oracle:', positive, 'accepted,', len(fixtures) - positive, 'refused')


if __name__ == '__main__':
    main()
