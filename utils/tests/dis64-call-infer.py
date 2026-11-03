#!/usr/bin/env python3
"""Derive scalar call locals from complete raw images, then execute actual C/RX without trusted declarations."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import runpy
import shlex
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE = runpy.run_path(str(ROOT/'utils/tests/dis64-call-goldens.py'))
CALLS = BASE['CALLS']
words = CALLS.words


# Independent fixture source supplies expected behavior, never runtime local-kind declarations.
def extra_cases():
    cases = []
    for kind, letter in ((1, 'W'), (2, 'L'), (3, 'F')):
        producer = CALLS.I('ICVTWF', 17, source=42, destination=64) if kind == 3 else CALLS.INIT.move(42, 64, kind=letter)
        root = [producer, CALLS.frame(), CALLS.indirect('IMOV'+letter, 64, 40, 64),
                CALLS.indirect('ILEA', 104, 40, 32), CALLS.call(7), CALLS.INIT.move(104, 80, 1, letter), CALLS.EXIT]
        leaf = [CALLS.RETURNS.store(kind, 64, 1), CALLS.RET]
        for change in ('normal', 'unused', 'overlap', 'reinterpret', 'uninitialized'):
            changed = root.copy()
            body = leaf.copy()
            if change == 'unused': body[0] = CALLS.RETURNS.store(kind, 42)
            if kind == 3 and change == 'unused': continue  # Real immediate is deliberately not supported.
            if change == 'overlap': changed[5] = CALLS.INIT.move(104, 68, 1, 'L')
            if change == 'reinterpret': changed[5] = CALLS.INIT.move(104, 64, 1, 'L' if kind == 1 else 'W')
            if change == 'uninitialized': changed[0] = CALLS.INIT.move(80, 64, 1, letter)
            case = CALLS.fixture(changed, leaves=[body], slots=[(), ()], signatures=[(), (kind,)], results=[0, kind], sizes=[112, 88])
            accepted = change in ('normal', 'unused')
            bits = struct.unpack('>Q', struct.pack('>d', 42.0))[0] if kind == 3 else 42
            cases.append((kind, change, BASE['container'](case), accepted, bits))
    return cases


# Commit exact additional wire bytes without adding any caller-local type authority.
def goldens(write=False):
    directory = ROOT/'utils/tests/goldens/dis64-infer'
    rows = extra_cases()
    manifest = {'purpose': 'private raw-image scalar inference; public call admission closed',
                'declarations': 'none; numeric kinds derive from raw opcodes and parsed signatures',
                'cases': [{'name': str(k)+'-'+c, 'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest(),
                           'accept': a, 'kind': k, 'result': v} for k, c, b, a, v in rows]}
    contents = {'manifest.json': json.dumps(manifest, indent=2)+'\n'}
    for kind, change, data, _, _ in rows:
        contents[str(kind)+'-'+change+'.hex'] = '\n'.join(data.hex()[i:i+64] for i in range(0, len(data)*2, 64))+'\n'
    if write: directory.mkdir(parents=True, exist_ok=True)
    for name, content in contents.items():
        path = directory/name
        if write: path.write_text(content)
        elif path.read_text() != content: raise RuntimeError('Inferred golden drift: '+name)
    return rows


# Replace only the trusted fixture boundary; keep actual owning runtime bodies and result checks.
def unit(control):
    source = BASE['unit']()
    header = (ROOT/'include/dis64-call-infer.h').read_text()
    if control == 'seed':
        header = header.replace('if(!dis64inferslot(work, i, at, kind))', 'if(0 && !dis64inferslot(work, i, at, kind))', 1)
    elif control == 'publication':
        header = header.replace('return dis64callimagecheck(data, bytes, objectlimit, work->locals, work->count,',
                                '*result = image;\n\treturn dis64callimagecheck(data, bytes, objectlimit, work->locals, work->count,', 1)
    elif control == 'kind':
        header = header.replace('kind = Dis64Int;', 'kind = Dis64Bool;', 1)
    source = source.replace('#include "dis64-pilot.h"', '#include "dis64-pilot.h"\n'+header, 1)
    source = source.replace('static uchar imagebytes[Dis64CallImageBytes];',
                            'static uchar imagebytes[Dis64CallImageBytes];\n\tstatic Dis64CallInference inference;', 1)
    marker = '\t\tvalid = resultnumber(file);'
    source = source.replace(marker, '\t\tmemset(locals, 0xa5, sizeof(locals));\n\t\tmemset(slots, 0xa5, sizeof(slots));\n'+marker, 1)
    old = 'dis64callimagecheck(imagebytes, bytes, objectlimit,\n\t\tlocals, localcount, resultdecoded, resultwork, resultqueue, workcap, limit,'
    new = 'dis64callinfercheck(imagebytes, bytes, objectlimit,\n\t\t\t&inference, resultdecoded, resultwork, resultqueue, workcap, limit,'
    if source.count(old) != 1: raise RuntimeError('Whole-image entry drift')
    if control != 'external': source = source.replace(old, new, 1)
    if control == 'external':
        # Bad caller declarations must be rejected safely rather than dereferenced by the control.
        source = source.replace('memset(locals, 0xa5, sizeof(locals));', 'memset(locals, 0, sizeof(locals));')
    return source.replace('PASS whole-image calls:', 'PASS inferred-image calls:', 1)


# Replay immutable peer corpus and fresh typed/error probes with no local declarations in the protocol.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write-goldens', action='store_true')
    args = parser.parse_args()
    extra = goldens(args.write_goldens)
    if args.write_goldens:
        print('WROTE', len(extra), 'raw-image inference goldens')
        return
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    manifest = BASE['fixtures']()
    rows = []
    for item in manifest['cases']:
        data = bytes.fromhex((BASE['GOLDENS']/(item['name']+'.hex')).read_text())
        rows.append((item['name'], data, item['private_accept'], item['result_kind'], item['result'], item['runtime_error'], item['chain'], len(item['locals'])))
    for kind, change, data, accepted, value in extra:
        rows.append(('derived-'+str(kind)+'-'+change, data, accepted, kind, value, 0, 0, 2))
    env = dict(os.environ)
    arch = env.get('NATIVE64_ARCH', 'amd64')
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    with tempfile.TemporaryDirectory(prefix='dis64-infer-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        env['TMPDIR'] = str(work)
        stream = bytearray(words(len(rows)))
        for name, data, accepted, kind, value, failure, chain, functions in rows:
            path = work/(name+'.dis64')
            path.write_bytes(data)
            namebytes = str(path).encode()
            stream += words(len(namebytes))+namebytes+words(4096, functions, 256, CALLS.LIMIT)
            # Protocol placeholders have no authority and are poisoned before invoking inference.
            for _ in range(functions): stream += words(0, 0, 0, 0)
            stream += words(int(accepted), kind, value>>32, value&0xffffffff, failure, chain, 0, 42)
        (work/'cases.bin').write_bytes(stream)
        command = shlex.split(env.get('CC', 'cc'))+flags+['-DDIS_WORD32=1']
        if arch == 'arm64': command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch == 'riscv64': command += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
        elif arch != 'amd64': raise RuntimeError('Unsupported target '+arch)
        command += ['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        source = work/'infer.c'
        for control in (None, 'external', 'seed', 'publication', 'kind'):
            source.write_text(unit(control))
            binary = work/(control or 'normal')
            subprocess.run(command+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(shlex.split(env.get('NATIVE64_RUNNER', ''))+[str(binary), str(work/'cases.bin')], env=env,
                                    capture_output=True, text=True, timeout=90)
            if control is None:
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)
            elif result.returncode != 10 or not any(t in result.stderr for t in ('whole-image decision mismatch', 'whole-image refusal published output')):
                raise RuntimeError('Inference control missed predicate: '+str(control)+' '+result.stdout+result.stderr)
            else: print('PASS inference semantic control:', control, arch, flush=True)
    print('PASS inference corpus: committed25 identities plus', len(rows)-25, 'typed probes; no caller local declarations', flush=True)


if __name__ == '__main__':
    main()
