#!/usr/bin/env python3
"""Execute verified raw caller/leaf sections through actual native VM calls and a trusted LEA8 step."""
import argparse
import importlib.util
import os
from pathlib import Path
import resource
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('call_vectors', Path(__file__).with_name('dis64-bound-call.py'))
CALLS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CALLS)
I, EXIT, RET, words = CALLS.I, CALLS.EXIT, CALLS.RET, CALLS.words


# Preserve real VM/heap/decoder/JIT bodies; only the isolated control source is weakened.
def unit(control, instrument=False, instrument_source=False):
    runtime = runpy.run_path(str(ROOT/'libinterp/tests/dis64-runtime.py'))
    source = runtime['buildunit']().split('/* Set up an actual root extent, execute to IEXIT, then unwind actual owned storage. */')[0]
    extract = runtime['function']
    xec = (ROOT/'libinterp/xec.c').read_text()
    stack = (ROOT/'libinterp/stack.c').read_text()
    parts = [extract(stack, s) for s in ('void\nextend(', 'void\nunextend(')]
    parts += [extract(xec, s) for s in ('static Type*\nmoduletype(', 'static int\nframefits(', 'OP(frame)',
              'OP(call)', 'OP(ret)', 'OP(lea)', 'OP(cvtwf)', 'int\nnative64lea8step(')]
    if control == 'width':
        parts[-1] = parts[-1].replace('memmove(R.d, &address, sizeof(address));', 'memmove(R.d, &address, sizeof(WORD));')
    elif control == 'guard_mode':
        parts[-1] = parts[-1].replace('instruction.add != (SRC(AFP)|DST(AFP|AIND)) ||', '0 ||')
    elif control == 'priority' and not instrument:
        marker = 'if(step != nil && step())'
        if source.count(marker) != 1:
            raise RuntimeError('cstep marker drift')
        source = source.replace(marker, 'if(0 && step != nil && step())')
    elif control == 'ordinary':
        parts[-3] = parts[-3].replace('WORD address;', 'void *address;').replace('address = (WORD)(uintptr)R.s;', 'address = R.s;')
    if instrument:
        profile = extract((ROOT/'emu/port/devprof.c').read_text(), 'static void\ncpxec(')
        debug = extract((ROOT/'emu/port/devprog.c').read_text(), 'void\ndbgxec(')
        if 'vminstrument(&fetched, nil);' not in profile or 'vminstrument(&fetched, &loadslot);' not in debug:
            raise RuntimeError('instrumented executor wiring drift')
        if instrument_source:
            profile = profile.replace('vminstrument(&fetched, nil);', 'instrumentsource(&fetched, nil);')
        fixture = (ROOT/'utils/tests/native64-instrument.c').read_text().replace('INSTRUMENT_EXECUTOR', profile)
        marker = 'static int\nexecute(void *argument)'
        source = source.replace(marker, fixture+'\n\n'+marker, 1).replace('xec(argument);', 'cpxec(argument);', 1)
    tests = (ROOT/'utils/tests/native64-result-calls.c').read_text()
    if instrument:
        tests = tests.replace('widths = runs = refused = 0;', 'instrumentloadprobe();\n\twidths = runs = refused = 0;', 1)
    return source+'\n\n'+'\n\n'.join(parts)+'\n\n'+tests


# Actual wire sections carry no native addresses; expected bits are independent scalar values.
def fixtures():
    vectors = []
    for kind in (1, 2, 3, 4):
        letter = 'L' if kind == 2 else 'F' if kind == 3 else 'W'
        value = 1 if kind == 4 else 7 if kind == 3 else -31
        expected = struct.unpack('>Q', struct.pack('>d', 7.0))[0] if kind == 3 else value & ((1 << (64 if kind == 2 else 32)) - 1)
        for pointer in (40, 48, 56):
            for padding in (0, 8):
                for failure in (0, 1, 2):
                    rootbytes, leafbytes = 112 + padding, 88 + padding
                    producer = I('ICVTWF', 17, source=value, destination=64) if kind == 3 else CALLS.INIT.move(value, 64, kind=letter)
                    root = [producer, CALLS.frame(pointer=pointer), CALLS.indirect('IMOV'+letter, 64, pointer, 64),
                            CALLS.indirect('ILEA', rootbytes-8, pointer, 32), CALLS.call(7, pointer),
                            CALLS.INIT.move(rootbytes-8, 80, 1, letter), EXIT]
                    resultstore = CALLS.RETURNS.store(kind, 64, 1)
                    division = [CALLS.INIT.move(1, 72), I('IDIVW', 17, source=0, destination=72)]
                    leaf = [resultstore, RET] if not failure else division+[resultstore, RET] if failure == 1 else [resultstore]+division+[RET]
                    case = CALLS.fixture(root, leaves=[leaf], slots=[((64, kind), (80, kind)), ((64, kind), (72, 1))],
                                         signatures=[(), (kind,)], results=[0, kind], sizes=[rootbytes, leafbytes])
                    vectors.append((case, 1, kind, expected, failure, 0, expected))
                    if failure == 0:
                        bad = list(case)
                        raw = bytearray(bad[0])
                        raw[5] = 0xff
                        bad[0] = bytes(raw)
                        vectors.append((tuple(bad), 0, kind, expected, failure, 0, expected))
                        missing = root[:3] + root[4:]
                        missing[3] = CALLS.call(6, pointer)
                        wrong = root.copy()
                        wrong[3] = CALLS.indirect('ILEA', rootbytes-16, pointer, 32)
                        for badroot, badleaf in ((missing, leaf), (wrong, leaf), (root, [RET])):
                            rejected = CALLS.fixture(badroot, leaves=[badleaf],
                                slots=[((64, kind), (80, kind)), ((64, kind), (72, 1))],
                                signatures=[(), (kind,)], results=[0, kind], sizes=[rootbytes, leafbytes])
                            vectors.append((rejected, 0, kind, expected, failure, 0, expected))
                    value2 = 0 if kind == 4 else 8 if kind == 3 else 9
                    second = struct.unpack('>Q', struct.pack('>d', 8.0))[0] if kind == 3 else value2
                    producer2 = I('ICVTWF', 17, source=value2, destination=64) if kind == 3 else CALLS.INIT.move(value2, 64, kind=letter)
                    chainroot = [producer, CALLS.frame(pointer=pointer), CALLS.indirect('IMOV'+letter, 64, pointer, 64),
                                 CALLS.indirect('ILEA', rootbytes-8, pointer, 32), CALLS.call(13, pointer),
                                 CALLS.INIT.move(rootbytes-8, 80, 1, letter), producer2, CALLS.frame(pointer=pointer),
                                 CALLS.indirect('IMOV'+letter, 64, pointer, 64), CALLS.indirect('ILEA', rootbytes-8, pointer, 32),
                                 CALLS.call(15, pointer), CALLS.INIT.move(rootbytes-8, 88, 1, letter), EXIT]
                    chaincase = CALLS.fixture(chainroot, leaves=[[resultstore, RET], leaf],
                        slots=[((64, kind), (80, kind), (88, kind)), ((64, kind), (72, 1)), ((64, kind), (72, 1))],
                        signatures=[(), (kind,), (kind,)], results=[0, kind, kind], sizes=[rootbytes, leafbytes])
                    vectors.append((chaincase, 1, kind, expected, failure, 1, second))
    return vectors


# Reuse the port target compiler/runner; controls must fail before a bad pointer can reach CALL.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('width', 'priority', 'ordinary', 'guard_mode'))
    parser.add_argument('--instrument', action='store_true', help='execute the actual active coverage C loop')
    parser.add_argument('--instrument-source', action='store_true', help='probe debugger source-output convention in the coverage fixture')
    args = parser.parse_args()
    if args.instrument_source and not args.instrument:
        parser.error('--instrument-source requires --instrument')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    vectors = fixtures()
    data = bytearray(words(len(vectors)))
    for case, valid, kind, expected, failure, chain, second in vectors:
        if bool(CALLS.oracle(case)) != bool(valid):
            raise RuntimeError('independent fixture expectation mismatch')
        code, exports, pool, types, objlimit, entry, entryframe, locals_, workcap, capacity, limit = case
        data += words(len(code), len(exports), len(pool), len(types), objlimit, entry, entryframe, len(locals_), workcap, limit)
        data += code + exports + pool + types
        for symbol, pc, frame, slots in locals_:
            data += words(symbol, pc, frame, len(slots))
            for offset, scalar in slots:
                data += words(offset, scalar)
        data += words(valid, kind, expected >> 32, expected & 0xffffffff, failure, chain, second >> 32, second & 0xffffffff)
    with tempfile.TemporaryDirectory(prefix='native64-result-calls-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        source = work/'calls.c'
        source.write_text(unit(args.control, args.instrument, args.instrument_source))
        if args.instrument and args.control == 'priority':
            header = (ROOT/'include/vmfetch.h').read_text()
            marker = 'step == native64lea8step && step()'
            if header.count(marker) != 1:
                raise RuntimeError('instrumented priority marker drift')
            (work/'vmfetch.h').write_text(header.replace(marker, '0 && '+marker))
        fixturesfile = work/'cases.bin'
        fixturesfile.write_bytes(data)
        command = compiler + flags
        if arch == 'arm64':
            command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch == 'riscv64':
            command += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
        elif arch != 'amd64':
            raise RuntimeError('unsupported target')
        command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        for bits in ((32,) if args.control else (32, 64)):
            binary = work/('calls'+str(bits))
            subprocess.run(command+(['-DDIS_WORD32=1'] if bits == 32 else [])+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
            if args.control:
                for jit in ((0,) if args.control == 'ordinary' else (0, 1)):
                    result = subprocess.run(runner+[str(binary), str(fixturesfile), str(jit), args.control], env=env, capture_output=True, text=True, timeout=120)
                    marker = 'ordinary LEA changed width' if args.control == 'ordinary' else 'native64 LEA8 shape guard missed' if args.control == 'guard_mode' else 'native64 result address mismatch'
                    if result.returncode != 10 or marker not in result.stderr:
                        raise RuntimeError('result control missed semantic predicate, status='+str(result.returncode)+': '+result.stdout+result.stderr)
                    print('PASS result-address control:', args.control, arch, 'jit='+str(jit), flush=True)
            else:
                subprocess.run(runner+[str(binary), str(fixturesfile)], env=env, check=True, timeout=120)
        if args.instrument and not args.control:
            print('PASS instrumented result calls:', arch,
                  'source-output helper' if args.instrument_source else 'actual coverage loop',
                  '(C execution; cached JIT entries deliberately bypassed)', flush=True)


if __name__ == '__main__':
    main()
