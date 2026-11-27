#!/usr/bin/env python3
"""Compare entry-seeded abstract CFG propagation against an independent DFS oracle."""
import argparse
import importlib.util
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('state_oracle', Path(__file__).with_name('dis64-local-state.py'))
STATE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = STATE
SPEC.loader.exec_module(STATE)
E = STATE.event
F = STATE.function
ZERO = STATE.ZERO


# Store explicit semantic events and graph edges without encoding Dis operands.
def node(action=None, edge=0, target=0, setup=0):
    return (action if action is not None else E(0), edge, target, setup)


# Check EVERY node first, independently of whether entry traversal can reach it.
def static(plan, nodes, base):
    for pc, (event, edge, target, setup) in enumerate(nodes):
        action, *values = event
        facts = dict(zip(STATE.FIELDS, values))
        if edge not in range(4) or (edge in (0, 2) and pc + 1 == len(nodes)):
            return False
        if edge in (0, 3) and target:
            return False
        if edge in (1, 2):
            if target >= len(nodes) or nodes[target][3] or setup or action:
                return False
        function = None
        prepare = None
        if setup:
            if not base < setup <= base + pc:
                return False
            prepare = nodes[setup - base - 1][0]
            if prepare[0] != 1 or prepare[1] != setup or prepare[2] >= len(plan[4]):
                return False
            function = plan[4][prepare[2]]
        if action == 0:
            if any(values) or edge == 3:
                return False
            continue
        if action not in STATE.ALLOWED or any(v for k, v in facts.items() if k not in STATE.ALLOWED[action]):
            return False
        if (action in (8, 9)) != (edge == 3):
            return False
        kind, boolean = facts['kind'], facts['boolean']
        if action == 1:
            if setup or facts['site'] != base + pc + 1 or facts['callee'] >= len(plan[4]):
                return False
            if facts['frame'] != plan[4][facts['callee']][0]:
                return False
        elif action == 2:
            if function is None or facts['index'] >= function[2] or kind != function[3][facts['index']]:
                return False
            if (kind == 4 and boolean > 1) or (kind != 4 and boolean):
                return False
        elif action == 3:
            if function is None or not function[1] or kind != function[1]:
                return False
            if (facts['source'], facts['destination'], facts['width']) != (plan[2], 32, 8):
                return False
        elif action == 4:
            if function is None or facts['callee'] != prepare[2] or facts['frame'] != function[0]:
                return False
        elif action == 7:
            if function is None:
                return False
        elif action == 5:
            if setup or kind not in STATE.KINDS:
                return False
        elif action == 6:
            if setup or not plan[3] or kind != plan[3]:
                return False
            if (kind == 4 and boolean > 1) or (kind != 4 and boolean):
                return False
        elif action == 8 and setup:
            return False
    return True


# DFS uses immutable incoming states, unlike the C FIFO and poisoned mutable scratch.
def oracle(plan, nodes, base, identity, capacity, limit):
    count = len(nodes)
    if not STATE.valid_plan(plan) or not 1 <= count <= 65536 or base > 65536 - count:
        return None
    if identity >= 64 or capacity < count or not 1 <= limit <= 196608:
        return None
    if not static(plan, nodes, base):
        return None
    incoming = {0: ZERO}
    stack = [0]
    edges = returns = failures = work = reached = 0
    while stack:
        if work == limit:
            return None
        work += 1
        pc = stack.pop()
        event, edge, target, setup = nodes[pc]
        state = incoming[pc]
        if state[1] != setup:
            return None
        after = STATE.transition(plan, state, event) if event[0] else state
        if after is None:
            return None
        reached += 1
        if edge == 3:
            returns += event[0] == 8
            failures += event[0] == 9
            continue
        successors = [target] if edge == 1 else [pc + 1]
        if edge == 2:
            successors.append(target)
        for successor in successors:
            if work == limit:
                return None
            work += 1
            edges += 1
            if successor in incoming:
                if STATE.join(plan, incoming[successor], after) is None:
                    return None
            else:
                incoming[successor] = after
                stack.append(successor)
    return (identity, count, reached, edges, returns, failures, work)


# Generate malformed unreachable code, setup boundaries, joins, loops and exact caps.
def cases():
    result = []
    void = (128, 64, 120, 0, (F(7, 0, ()),))
    scalar = (128, 64, 120, 1, (F(7, 1, ()), F(7, 4, (4,))))

    # First records let structural controls fail semantically before any unsafe traversal.
    def add(plan, nodes, base=0, identity=0, capacity=None, limit=196608):
        result.append((plan, nodes, base, identity, len(nodes) if capacity is None else capacity, limit))

    add(void, [node(edge=1), node(E(99), 3), node(E(8), 3)])
    add(void, [node(edge=1), node(E(1, site=2, frame=7)),
               node(E(4, frame=7), setup=2), node(E(8), 3), node(edge=1, target=2)])
    add(void, [node(edge=1)], limit=1)
    add(void, [node(edge=1)], capacity=0)
    add(scalar, [node(E(8), 3)])
    add(void, [node(E(1, site=1, frame=7)), node(),
               node(E(4, frame=7), setup=1), node(E(8), 3)])
    ready = (128, 64, 120, 0, (F(7, 1, ()),))
    add(ready, [node(edge=2, target=5), node(E(1, site=2, frame=7)),
                node(E(3, kind=1, source=120, destination=32, width=8), setup=2),
                node(E(4, frame=7), setup=2), node(edge=1, target=6), node(), node(E(8), 3)])
    add(void, [node(), node(E(8), 3)])
    for own in range(5):
        for returned in range(5):
            for count in (0, 1, 2, 31, 32, 33, 63, 64):
                kinds = tuple(STATE.KINDS[i % 4] for i in range(count))
                plan = (1024, 64, 1016, own, (F(7, returned, kinds),))
                base = 17
                nodes = [node(E(1, site=18, frame=7))]
                nodes += [node(E(2, index=i, kind=k, boolean=int(k == 4)), setup=18)
                          for i, k in enumerate(kinds)]
                if returned:
                    nodes.append(node(E(3, kind=returned, source=1016, destination=32, width=8), setup=18))
                nodes.append(node(E(4, frame=7), setup=18))
                if returned:
                    nodes.append(node(E(5, kind=returned)))
                if own:
                    nodes.append(node(E(6, kind=own)))
                nodes.append(node(E(8), 3))
                add(plan, nodes, base)
                if own:
                    # A nested call result does not establish this function's incoming result.
                    add(plan, nodes[:-2] + nodes[-1:], base)
                if returned:
                    callpc = next(i for i, n in enumerate(nodes) if n[0][0] == 4)
                    abandoned = nodes.copy()
                    abandoned[callpc] = node(E(7), setup=18)
                    add(plan, abandoned, base)
                    failed = nodes.copy()
                    failed[callpc] = node(E(9), 3, setup=18)
                    add(plan, failed, base)
                expected = oracle(plan, nodes, base, 0, len(nodes), 196608)
                add(plan, nodes, base, limit=expected[-1])
                add(plan, nodes, base, limit=expected[-1] - 1)
                # A correct-looking setup label must name THIS preparation, not a sibling.
                changed = nodes.copy()
                changed[1] = changed[1][:3] + (19,)
                add(plan, changed, base)
                # Branching into even otherwise well-formed argument/binding/call setup refuses.
                changed = [node(edge=1, target=2)] + nodes
                changed[1] = node(E(1, site=19, frame=7))
                changed = [n[:3] + (19 if n[3] else 0,) for n in changed]
                add(plan, changed, base)
    # Closed local loops are accepted; changing initialization across a back edge refuses.
    add(void, [node(edge=1)])
    add(scalar, [node(E(6, kind=1)), node(edge=1)])
    add(void, [node(edge=2, target=0), node(E(8), 3)])
    # Every static field and node position is checked, even after a terminal entry.
    baseline = [node(E(9), 3), node(E(1, site=2, frame=7)),
                node(E(3, kind=1, source=120, destination=32, width=8), setup=2),
                node(E(4, frame=7), setup=2), node(E(8), 3)]
    for pc in range(len(baseline)):
        flat = list(baseline[pc][0]) + list(baseline[pc][1:])
        for field in range(13):
            for value in (0, 1, 2, 4, 7, 8, 32, 64, 65, 120, 65536, 65537, 0xffffffff):
                changed = flat.copy()
                changed[field] = value
                nodes = baseline.copy()
                nodes[pc] = (tuple(changed[:10]), *changed[10:])
                add(scalar, nodes)
    rng = random.Random(76)
    for _ in range(1600):
        size = rng.randrange(1, 24)
        own = rng.randrange(2)
        plan = scalar[:3] + (own, scalar[4])
        nodes = []
        for pc in range(size):
            kind = rng.randrange(5)
            if pc + 1 == size or kind == 4:
                nodes.append(node(E(rng.choice((8, 9))), 3))
            elif kind in (1, 2):
                nodes.append(node(edge=kind, target=rng.randrange(size)))
            elif kind == 3 and own:
                nodes.append(node(E(6, kind=own)))
            else:
                nodes.append(node())
        add(plan, nodes, identity=rng.randrange(64), limit=rng.choice((1, 2, 10, 196608)))
    for size in (0, 1, 65536, 65537):
        nodes = [node()] * max(0, size - 1) + ([node(E(8), 3)] if size else [])
        add(void, nodes)
    add(void[:3] + (65, void[4]), [node(edge=1)])
    for base in (0, 65535, 65536, 0xffffffff):
        for identity in (0, 63, 64, 0xffffffff):
            for limit in (0, 1, 196608, 196609):
                add(void, [node(E(8), 3)], base, identity, limit=limit)
    return result


# Execute target C and distinguish semantic removal failures from crashes/compiler errors.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('entry', 'unreachable', 'setup', 'site',
                                              'join', 'capacity', 'limit', 'reset'))
    args = parser.parse_args()
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    words = lambda *values: struct.pack('>' + 'I' * len(values), *values)
    fixtures = cases()
    data = bytearray(words(len(fixtures)))
    positive = 0
    for plan, nodes, base, identity, capacity, limit in fixtures:
        data += words(*plan[:4], len(plan[4]))
        for frame, result, count, kinds in plan[4]:
            data += words(frame, result, count, *kinds)
        data += words(len(nodes), base, identity, capacity, limit)
        for event, edge, target, setup in nodes:
            data += words(*event, edge, target, setup)
        expected = oracle(plan, nodes, base, identity, capacity, limit)
        positive += expected is not None
        data += words(int(expected is not None), *(expected if expected is not None else (0,) * 7))
    with tempfile.TemporaryDirectory(prefix='dis64-local-cfg-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        header = (ROOT / 'include/dis64-local-cfg.h').read_text()
        mutations = {
            'entry': ('next.assigned = 0;', 'next.assigned = plan->result != 0;'),
            'unreachable': ('if(!dis64localcfgnode(plan, nodes, count, base, pc))',
                            'if(pc == 0 && !dis64localcfgnode(plan, nodes, count, base, pc))'),
            'setup': ('nodes[node->target].setup != 0 ||', ''),
            'site': ('if(scratch[pc].state.site != node->setup)', 'if(0)'),
            'join': ('if(!dis64localstatemerge(plan, &scratch[targets[i]].state, &next, &merged))', 'if(0)'),
            'capacity': ('capacity < count ||', ''),
            'limit': ('if(result.work == limit)', 'if(0)'),
            'reset': ('scratch[pc].seen = 0;', 'scratch[pc].seen = 0xa5;'),
        }
        if args.control:
            old, new = mutations[args.control]
            if header.count(old) != (2 if args.control == 'limit' else 1):
                raise RuntimeError('CFG mutation marker drift')
            header = header.replace(old, new)
        (work / 'dis64-local-cfg.h').write_text(header)
        fixture = work / 'graphs.bin'
        fixture.write_bytes(data)
        binary = work / 'cfg'
        subprocess.run(compiler + flags + ['-I' + str(work), '-I' + str(ROOT / 'include'),
                       str(ROOT / 'utils/tests/dis64-local-cfg.c'), '-o', str(binary)], env=env, check=True)
        run = subprocess.run(runner + [str(binary), str(fixture)], env=env,
                             capture_output=True, text=True, timeout=90)
        if args.control:
            if run.returncode != 10 or 'local CFG predicate failed' not in run.stderr:
                raise RuntimeError('CFG removal missed semantic predicate: ' + run.stdout + run.stderr)
            print('PASS experimental CFG removal control:', args.control)
        elif run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        else:
            print(run.stdout, end='')
            print('Independent DFS oracle:', positive, 'accepted,', len(fixtures) - positive, 'refused')


if __name__ == '__main__':
    main()
