#!/usr/bin/env python3
"""Run VM/compiler checkpoints with optional adjacent runtime and JIT drivers."""
import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import sys

CORE = [
    'libinterp/tests/mapped-copy-native.py',
    'libinterp/tests/srv-results.py',
    'libinterp/tests/owner-native.py',
    'libinterp/tests/jit-unknown-kind.py',
    'libinterp/tests/sys-pctl.py',
    'libinterp/tests/sys-packdir.py',
    'libinterp/tests/goto-native.py',
    'libinterp/tests/sys-stat-slots.py',
    'libinterp/tests/real-branch-native.py',
    'libinterp/tests/sys-status-slots.py',
    'libinterp/tests/real-power-native.py',
    'libinterp/tests/geometry-return-slots.py',
    'libinterp/tests/real-to-integer-native.py',
    'libinterp/tests/math-return-slots.py',
    'limbo/tests/fixed-zero-div.py',
    'libinterp/tests/heap-query.py',
    'libinterp/tests/prog-diagnostics.py',
    'libinterp/tests/exception-frames.py',
    'libinterp/tests/stack-lifecycle.py',
    'libinterp/tests/jit-coverage.py',
    'libinterp/tests/stack-unwind.py',
    'libinterp/tests/sys-tokenize.py',
    'libinterp/tests/sys-tokenize-matrix.py',
    'limbo/tests/fold-result-types.py',
    'emu/port/tests/startup-options.py',
    'emu/port/tests/startup-geometry.py',
    'libinterp/tests/heap-bounds.py',
    'libinterp/tests/array-indices.py',
    'libinterp/tests/descriptor-index.py',
    'libinterp/tests/channels.py',
    'libinterp/tests/strings.py',
    'libinterp/tests/string-codecs.py',
    'libinterp/tests/lists.py',
    'libinterp/tests/sys-print.py',
    'libinterp/tests/sys-utf.py',
    'libinterp/tests/heap-clones.py',
    'libinterp/tests/pointer-maps.py',
    'libinterp/tests/mapped-moves.py',
    'libinterp/tests/module-types.py',
    'libinterp/tests/module-imports.py',
    'libinterp/tests/call-slots.py',
    'libinterp/tests/return-slots.py',
    'libinterp/tests/gui-type-maps.py',
    'libinterp/tests/tk-owners.py',
    'libinterp/tests/channel-queues.py',
    'libinterp/tests/channel-transfers.py',
    'libinterp/tests/integer-ops.py',
    'libinterp/tests/real-branches.py',
    'libinterp/tests/integer-branches.py',
    'libinterp/tests/scalar-transfers.py',
    'libinterp/tests/integer-alu.py',
    'libinterp/tests/fixed-scale.py',
    'libinterp/tests/fixed-frame.py',
    'libinterp/tests/fixed-coeff.py',
    'libinterp/tests/shift-div.py',
    'libinterp/tests/integer-real.py',
    'libinterp/tests/real-arithmetic.py',
    'libinterp/tests/vm-addresses.py',
    'libinterp/tests/string-branches.py',
    'libinterp/tests/string-slots.py',
    'libinterp/tests/raise-slots.py',
    'libinterp/tests/riscv-features.py',
    'libinterp/tests/reference-slots.py',
    'libinterp/tests/geometry-arithmetic.py',
    'libinterp/tests/math-bessel.py',
    'libinterp/tests/runtime-protect.py',
    'limbo/tests/fold-traps.py',
    'limbo/tests/fold-short-circuit.py',
    'limbo/tests/fold-identities.py',
    'limbo/tests/fold-effects.py',
    'limbo/tests/fold-string-length.py',
    'limbo/tests/fold-strings.py',
    'limbo/tests/case-ranges.py',
    'limbo/tests/case-max.py',
]
JIT = [
    'libinterp/tests/mapped-copy-layouts.py',
    'libinterp/tests/mapped-copy-jit.py',
    'libinterp/tests/owner-jit.py',
    'libinterp/tests/386-byte-div.py',
    'libinterp/tests/386-types.py',
    'libinterp/tests/arm-types.py',
    'libinterp/tests/arm-vfp.py',
]


def main():
    """Fail at the first failed driver, without turning missing JIT packages into a pass."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', action='store_true', help='run VM/compiler checks without GUI/Tk or launcher-option tests')
    parser.add_argument('--jit', action='store_true', help='include instruction-emulation and emitter-bound checks')
    parser.add_argument('--list', action='store_true', help='list selected drivers without executing them')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    tests = CORE+(JIT if args.jit else [])
    if args.core:
        excluded = {'libinterp/tests/gui-type-maps.py', 'libinterp/tests/tk-owners.py',
                    'emu/port/tests/startup-options.py', 'emu/port/tests/startup-geometry.py'}
        tests = [test for test in tests if test not in excluded]
    for test in tests:
        if not (root/test).is_file():
            parser.error('missing driver: '+test)
    if args.list:
        print('\n'.join(tests))
        return
    if args.jit:
        missing = [name for name in ('capstone', 'unicorn', 'keystone') if importlib.util.find_spec(name) is None]
        if missing:
            parser.error('install libinterp/tests/requirements-jit.txt; missing '+', '.join(missing))
    env = dict(os.environ)
    env.setdefault('CFLAGS', '-O1 -g -fsanitize=undefined -fno-sanitize-recover=all')
    for test in tests:
        print('RUN '+test, flush=True)
        subprocess.run([sys.executable, str(root/test)], cwd=root, env=env, check=True)
    print('PASS checkpoint: %d host drivers; complete VM/native target acceptance remains separate' % len(tests))


if __name__ == '__main__':
    main()
