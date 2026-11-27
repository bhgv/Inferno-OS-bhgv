#!/usr/bin/env python3
"""Check private scalar-call transitions and strict joins, without admitting file calls."""
import argparse
from dataclasses import dataclass, replace
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
KINDS = (1, 2, 3, 4)
FIELDS = ('site', 'callee', 'frame', 'index', 'kind', 'boolean',
          'source', 'destination', 'width')
ALLOWED = {
    1: {'site', 'callee', 'frame'},
    2: {'index', 'kind', 'boolean'},
    3: {'kind', 'source', 'destination', 'width'},
    4: {'callee', 'frame'},
    5: {'kind'},
    6: {'kind', 'boolean'},
    7: set(),
    8: set(),
    9: set(),
}
ZERO = (0,) * 8


# Represent preparation independently as an immutable owner and an argument SET.
@dataclass(frozen=True)
class Owner:
    site: int
    callee: int
    arguments: frozenset
    bound: bool


# Keep incoming assignment separate from the outgoing result and preparation.
@dataclass(frozen=True)
class Model:
    phase: int
    owner: object
    ready: int
    assigned: bool


# Encode complete descriptor storage, including canonical zero unused kind slots.
def function(frame, result, kinds):
    return (frame, result, len(kinds), tuple(kinds) + (0,) * (64 - len(kinds)))


# Build readable semantic events; unused fields remain explicit zero in the fixture.
def event(action, **values):
    return (action,) + tuple(values.get(name, 0) for name in FIELDS)


# Validate scalar facts and caller geometry independently of production codecs.
def valid_plan(plan):
    size, argumentend, cell, result, functions = plan
    if size < 72 or size % 8 or cell != size - 8 or not 64 <= argumentend <= cell:
        return False
    if result not in (0,) + KINDS or len(functions) > 64:
        return False
    for frame, kind, count, kinds in functions:
        if kind not in (0,) + KINDS or count > 64:
            return False
        if any(k not in KINDS for k in kinds[:count]) or any(kinds[count:]):
            return False
    return True


# Translate raw state into a set-based model, refusing noncanonical representation.
def decode(plan, raw):
    phase, site, ordinal, bound, ready, assigned, low, high = raw
    if phase not in (0, 1, 2) or assigned not in (0, 1) or ready not in (0,) + KINDS:
        return None
    if plan[3] == 0 and assigned:
        return None
    if phase and any(raw[1:]):
        return None
    arguments = frozenset(i for i in range(64) if ((low | (high << 32)) >> i) & 1)
    if not site:
        if ordinal or bound or arguments:
            return None
        owner = None
    else:
        if site > 65536 or not 1 <= ordinal <= len(plan[4]) or bound not in (0, 1) or ready:
            return None
        frame, kind, count, kinds = plan[4][ordinal - 1]
        if (not kind and bound) or any(i >= count for i in arguments):
            return None
        owner = Owner(site, ordinal - 1, arguments, bool(bound))
    return Model(phase, owner, ready, bool(assigned))


# Serialize semantic models without depending on C structure layout or padding.
def encode(model):
    owner = model.owner
    mask = sum(1 << i for i in owner.arguments) if owner else 0
    return (model.phase, owner.site if owner else 0,
            owner.callee + 1 if owner else 0, int(owner.bound) if owner else 0,
            model.ready, int(model.assigned), mask & 0xffffffff, mask >> 32)


# Reduce a trusted abstract event; this is not a wire instruction decoder.
def transition(plan, raw, operation):
    if not valid_plan(plan):
        return None
    model = decode(plan, raw)
    action, *values = operation
    if model is None or model.phase or action not in ALLOWED:
        return None
    facts = dict(zip(FIELDS, values))
    if any(value for name, value in facts.items() if name not in ALLOWED[action]):
        return None
    owner = model.owner
    callee = plan[4][owner.callee] if owner else None
    kind = facts['kind']
    boolean = facts['boolean']
    if action == 1:
        ordinal = facts['callee']
        if owner or not 1 <= facts['site'] <= 65536 or ordinal >= len(plan[4]):
            return None
        if facts['frame'] != plan[4][ordinal][0]:
            return None
        model = replace(model, owner=Owner(facts['site'], ordinal, frozenset(), False), ready=0)
    elif action == 2:
        index = facts['index']
        if not owner or index >= callee[2] or kind != callee[3][index]:
            return None
        if (kind == 4 and boolean > 1) or (kind != 4 and boolean):
            return None
        model = replace(model, owner=replace(owner, arguments=owner.arguments | {index}))
    elif action == 3:
        if not owner or not callee[1] or owner.bound or kind != callee[1]:
            return None
        if (facts['source'], facts['destination'], facts['width']) != (plan[2], 32, 8):
            return None
        model = replace(model, owner=replace(owner, bound=True))
    elif action == 4:
        if not owner or facts['callee'] != owner.callee or facts['frame'] != callee[0]:
            return None
        if owner.arguments != frozenset(range(callee[2])) or (callee[1] and not owner.bound):
            return None
        model = replace(model, owner=None, ready=callee[1])
    elif action == 5:
        if owner or not model.ready or kind != model.ready:
            return None
    elif action == 6:
        if owner or not plan[3] or kind != plan[3]:
            return None
        if (kind == 4 and boolean > 1) or (kind != 4 and boolean):
            return None
        model = replace(model, assigned=True)
    elif action == 7:
        if not owner:
            return None
        model = replace(model, owner=None, ready=0)
    elif action == 8:
        if owner or (plan[3] and not model.assigned):
            return None
        model = Model(1, None, 0, False)
    else:
        model = Model(2, None, 0, False)
    return encode(model)


# Equality of semantic live models implements strict prepared/argument/result joins.
def join(plan, left, right):
    if not valid_plan(plan):
        return None
    first, second = decode(plan, left), decode(plan, right)
    if first is None or second is None or first.phase or first != second:
        return None
    return encode(first)


# Record expected results and preserve the input after every refusal in a trace.
def trace(plan, events, records, initial=ZERO):
    state = initial
    for operation in events:
        expected = transition(plan, state, operation)
        records.append((0, state, ZERO, operation, expected))
        if expected is not None:
            state = expected
    return state


# Cover all scalar signature combinations, bit boundaries, reuse and terminal ownership.
def groups():
    output = []
    for own in range(5):
        for result in range(5):
            for count in (0, 1, 2, 31, 32, 33, 63, 64):
                kinds = tuple(KINDS[i % 4] for i in range(count))
                functions = (function(7, result, kinds), function(8, (result + 1) % 5, (4,)))
                plan = (1024, 80, 1016, own, functions)
                records = []
                prepare = event(1, site=23, callee=0, frame=7)
                bind = event(3, kind=result, source=1016, destination=32, width=8)
                call = event(4, callee=0, frame=7)
                # A successful prefix, plus repeated preparation/call/abandon/return probes.
                events = [event(5, kind=result), event(8), event(6, kind=own),
                          prepare, prepare, event(5, kind=result), call]
                for i, kind in enumerate(kinds):
                    events += [event(2, index=i, kind=kind, boolean=int(kind == 4))]
                events += [bind, bind, call, call, event(5, kind=result),
                           event(5, kind=(result + 1) % 5), prepare,
                           event(5, kind=result), event(8), event(7), event(7),
                           event(5, kind=result), event(6, kind=own), event(8), event(9)]
                trace(plan, events, records)
                # Success-to-failure and success-to-abandon never preserve the old result.
                setup = [prepare]
                setup += [event(2, index=i, kind=k, boolean=int(k == 4)) for i, k in enumerate(kinds)]
                if result:
                    setup.append(bind)
                setup.append(call)
                ready = trace(plan, setup, records)
                trace(plan, [prepare, event(9), event(9), event(5, kind=result)], records, ready)
                trace(plan, [prepare, event(7), event(5, kind=result)], records, ready)
                # Join each reachable prefix with itself and with each changed state field.
                states = {ZERO, ready}
                state = ZERO
                for operation in setup:
                    next_state = transition(plan, state, operation)
                    if next_state is not None:
                        state = next_state
                        states.add(state)
                for state in sorted(states):
                    records.append((1, state, state, event(0), join(plan, state, state)))
                    for index in range(8):
                        changed = list(state)
                        changed[index] ^= 1
                        changed = tuple(changed)
                        records.append((1, state, changed, event(0), join(plan, state, changed)))
                output.append((plan, records))
    # Targeted malformed events and plan/state boundaries independent of random traces.
    plan = (128, 64, 120, 4, (function(9, 4, (1, 4, 2, 3)), function(10, 1, ())))
    records = []
    pending = trace(plan, [event(1, site=65536, frame=9)], records)
    complete = trace(plan, [event(2, index=i, kind=k, boolean=int(k == 4))
                           for i, k in enumerate((1, 4, 2, 3))], records, pending)
    bound = trace(plan, [event(3, kind=4, source=120, destination=32, width=8)], records, complete)
    ready = trace(plan, [event(4, frame=9)], records, bound)
    for state in (ZERO, pending, complete, bound, ready):
        for action in range(12):
            base = event(action)
            probes = [base]
            for index in range(1, 10):
                for value in (1, 2, 4, 8, 32, 64, 65, 65536, 65537, 0xffffffff):
                    changed = list(base)
                    changed[index] = value
                    probes.append(tuple(changed))
            for operation in probes:
                trace(plan, [operation], records, state)
        for index in range(8):
            for value in (0, 1, 2, 4, 64, 65, 65536, 65537, 0xffffffff):
                changed = list(state)
                changed[index] = value
                trace(plan, [event(9)], records, tuple(changed))
    # Exact cell provenance/address width and bool kind/range checks need valid prefixes.
    for kind in range(6):
        for source in (0, 64, 112, 120, 124, 128, 0xffffffff):
            for destination in (0, 4, 31, 32, 36, 40):
                for width in (0, 4, 8, 16):
                    trace(plan, [event(3, kind=kind, source=source,
                                       destination=destination, width=width)], records, complete)
        for value in (0, 1, 2, 0xffffffff):
            trace(plan, [event(2, index=1, kind=kind, boolean=value)], records, pending)
            trace(plan, [event(6, kind=kind, boolean=value)], records)
    output.append((plan, records))
    # Malformed geometry, unused signature fields and maximum descriptor counts.
    variants = []
    for field in range(4):
        for value in (0, 1, 63, 64, 65, 71, 72, 120, 128, 0xffffffff):
            changed = list(plan)
            changed[field] = value
            variants.append(tuple(changed))
    for count in (0, 64, 65):
        variants.append(plan[:4] + ((function(9, 4, ()),) * count,))
    for field in (1, 2, 3):
        changed = list(plan[4][0])
        changed[field] = (0, 1, 2, 3, 4, 5, 65, 0xffffffff) if field == 3 else 65
        if field == 3:
            for value in changed[field]:
                kinds = list(plan[4][0][3])
                kinds[63] = value
                variants.append(plan[:4] + ((plan[4][0][:3] + (tuple(kinds),),),))
        else:
            variants.append(plan[:4] + ((tuple(changed),),))
    for changed in variants:
        records = []
        trace(changed, [event(1, site=1, frame=9), event(9), event(8)], records)
        output.append((changed, records))
    # Deterministic mixed traces explore continuations after refusals, not just first errors.
    rng = random.Random(75)
    for own in range(5):
        plan = (128, 64, 120, own, (function(9, 4, (1, 4)), function(10, 1, ())))
        records = []
        for _ in range(80):
            events = []
            for _ in range(40):
                action = rng.randrange(1, 10)
                values = {name: 0 for name in FIELDS}
                if action == 1:
                    values.update(site=rng.choice((1, 17, 65536)), callee=rng.randrange(2))
                    values['frame'] = 9 + values['callee']
                elif action == 2:
                    values.update(index=rng.randrange(3), kind=rng.randrange(1, 5), boolean=rng.randrange(3))
                elif action == 3:
                    values.update(kind=rng.randrange(1, 5), source=120,
                                  destination=rng.choice((32, 40)), width=rng.choice((4, 8)))
                elif action == 4:
                    values.update(callee=rng.randrange(2))
                    values['frame'] = 9 + values['callee']
                elif action in (5, 6):
                    values.update(kind=rng.randrange(1, 5))
                    if values['kind'] == 4:
                        values['boolean'] = rng.randrange(3)
                events.append(event(action, **values))
            trace(plan, events, records)
        output.append((plan, records))
    return output


# Compile target code and require a semantic predicate for every removal control.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('arguments', 'binding', 'reuse', 'kind', 'boolean',
                                              'return', 'retire', 'join', 'canonical',
                                              'preparation', 'address', 'identity', 'result'))
    args = parser.parse_args()
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS',
                               '-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    words = lambda *values: struct.pack('>' + 'I' * len(values), *values)
    fixtures = groups()
    data = bytearray(words(len(fixtures)))
    accepted = refused = merges = 0
    for plan, records in fixtures:
        data += words(*plan[:4], len(plan[4]))
        for frame, result, count, kinds in plan[4]:
            data += words(frame, result, count, *kinds)
        data += words(len(records))
        for operation, left, right, action, expected in records:
            data += words(operation, *left, *right, *action, int(expected is not None),
                          *(expected if expected is not None else ZERO))
            accepted += expected is not None
            refused += expected is None
            merges += operation == 1
    with tempfile.TemporaryDirectory(prefix='dis64-local-state-',
                                     dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = (ROOT / 'include/dis64-local-state.h').read_text()
        mutations = {
            'arguments': ('if((next.arguments[i/32]&(1U<<(i%32))) == 0)', 'if(0)'),
            'binding': ('(function->result != Dis64Void && next.bound == 0)', '0'),
            'reuse': ('next.site = event->site;', 'next.ready = input->ready;\n\t\tnext.site = event->site;'),
            'kind': ('event->kind != function->kinds[event->index]', '0'),
            'boolean': ('(event->kind == Dis64Bool ? event->boolean > 1 : event->boolean != 0)', '0'),
            'return': ('(plan->result != Dis64Void && next.assigned == 0)', '0'),
            'retire': ('input->phase != 0)', '0)'),
            'join': ('left->arguments[0] != right->arguments[0]', '0'),
            'canonical': ('if((allowed&(1U<<i)) == 0 && fields[i] != 0)', 'if(0)'),
            'preparation': ('next.site != 0 || event->site == 0', 'event->site == 0'),
            'address': ('event->destination != 32 || event->width != 8', '0'),
            'identity': ('event->frame != function->frame', '0'),
            'result': ('event->kind != function->result', '0'),
        }
        if args.control:
            old, new = mutations[args.control]
            occurrences = 2 if args.control == 'boolean' else 1
            if header.count(old) != occurrences:
                raise RuntimeError('state mutation marker drift')
            header = header.replace(old, new)
        (work / 'dis64-local-state.h').write_text(header)
        fixture = work / 'cases.bin'
        fixture.write_bytes(data)
        binary = work / 'state'
        subprocess.run(compiler + flags + ['-I' + str(work), '-I' + str(ROOT / 'include'),
                       str(ROOT / 'utils/tests/dis64-local-state.c'), '-o', str(binary)],
                       env=env, check=True)
        run = subprocess.run(runner + [str(binary), str(fixture)], env=env,
                             capture_output=True, text=True, timeout=90)
        if args.control:
            if run.returncode != 10 or 'local state predicate failed' not in run.stderr:
                raise RuntimeError('state control missed semantic predicate: ' + run.stdout + run.stderr)
            print('PASS experimental state removal control:', args.control)
        elif run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        else:
            print(run.stdout, end='')
            print('Independent set oracle:', accepted, 'accepted,', refused, 'refused;', merges, 'joins')


if __name__ == '__main__':
    main()
