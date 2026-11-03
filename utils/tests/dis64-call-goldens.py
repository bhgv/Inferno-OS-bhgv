#!/usr/bin/env python3
"""Reproducible complete Dis64 call images and private native execution, without loader activation."""
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
GOLDENS = ROOT/'utils/tests/goldens/dis64-calls'
SUPPORT = runpy.run_path(str(ROOT/'utils/tests/native64-result-calls.py'))
CALLS = SUPPORT['CALLS']
I, words, EXIT, RET = CALLS.I, CALLS.words, CALLS.EXIT, CALLS.RET


# Reuse agreed section framing, independently serialized with standard Python integers.
def container(case, metadata=None, order=(1, 2, 3, 6, 7), extra=None):
    code, exports, strings, types = case[:4]
    sections = {1: metadata if metadata is not None else words(0, 1, 0, 0, 0),
                2: code, 3: types, 6: exports, 7: strings}
    if extra:
        sections.update(extra)
    offset = 32+12*len(order)
    directory = bytearray()
    payload = bytearray()
    for kind in order:
        value = sections[kind]
        directory += struct.pack('>HHII', kind, 1, offset, len(value))
        payload += value
        offset += len(value)
    return b'Dis64\r\n\x1a'+struct.pack('>HHIIIII', 1, 0, 32+12*len(order), 1, 0, 0, len(order))+directory+payload


# Construct the already accepted bounded root-to-leaf grammar with no new opcode/section ID.
def scalar(chain=False, failure=False, change=None):
    root = [CALLS.INIT.move(42, 64), CALLS.frame(), CALLS.indirect('IMOVW', 64, 40, 64),
            CALLS.indirect('ILEA', 104, 40, 32), CALLS.call(13 if chain else 7),
            CALLS.INIT.move(104, 80, 1, 'W')]
    if chain:
        root += [CALLS.INIT.move(80, 64, 1, 'W'), CALLS.frame(), CALLS.indirect('IMOVW', 64, 40, 64),
                 CALLS.indirect('ILEA', 104, 40, 32), CALLS.call(15), CALLS.INIT.move(104, 88, 1, 'W')]
    root += [EXIT]
    if change == 'interior': root[4] = CALLS.call(8)
    if change == 'frame': root[1] = CALLS.frame(typeid=0)
    if change == 'argument': root[2] = CALLS.INIT.move(42, 64)
    if change == 'reserved': root[0] = CALLS.INIT.move(42, 40)
    if change == 'result': root[3] = CALLS.indirect('ILEA', 96, 40, 32)
    if change == 'destination': root[3] = CALLS.indirect('ILEA', 104, 40, 40)
    if change == 'mode': root[3] = I('ILEA', 9, source=104, destination=32)
    if change == 'unready': root[0] = CALLS.INIT.move(104, 80, 1, 'W')
    leaf = [CALLS.RETURNS.store(1, 64, 1), RET]
    if change == 'width': leaf[0] = CALLS.RETURNS.store(2, 64, 1)
    if change == 'return': leaf[0] = CALLS.INIT.move(1, 72)
    if failure:
        leaf = [CALLS.INIT.move(1, 72), I('IDIVW', 17, source=0, destination=72)]+leaf
    count = 3 if chain else 2
    return CALLS.fixture(root, leaves=[leaf]* (count-1),
        slots=[((64, 1), (80, 1), (88, 1))]+[((64, 1), (72, 1))]*(count-1),
        signatures=[()]+[((4,) if change == 'signature' else (1,))]*(count-1), results=[0]+[(4 if change == 'kind' else 1)]*(count-1), sizes=[112, 88])


# Bind reproducible byte identities to trusted fixture declarations and explicit private/public outcomes.
def corpus():
    rows = []
    base = scalar()
    def add(name, case, accepted, description, image=None, failure=0, chain=0):
        image = container(case) if image is None else image
        rows.append((name, image, {'name': name, 'sha256': hashlib.sha256(image).hexdigest(),
            'bytes': len(image), 'private_accept': bool(accepted), 'production_accept': False,
            'result_kind': 1, 'result': 42, 'second_result': 42, 'runtime_error': failure,
            'chain': chain, 'description': description, 'locals': case[7]}))
    add('answer42', base, True, 'Entry calls one int32 helper and copies returned42.')
    add('chain42', scalar(chain=True), True, 'Two sequential root-to-leaf calls; second consumes the copied first result. No helper-to-helper call.', chain=1)
    add('divide-zero', scalar(failure=True), True, 'Static proof succeeds; actual callee raises zero divide before result publication.', failure=1)
    add('reordered42', base, True, 'Same meaning with a different legal section-directory order.', container(base, order=(7, 3, 1, 6, 2)))
    for change, name in (('interior', 'reject-interior'), ('frame', 'reject-frame'), ('argument', 'reject-argument'),
                         ('reserved', 'reject-pointer-cell'), ('result', 'reject-result-cell'),
                         ('destination', 'reject-lea-destination'), ('mode', 'reject-lea-mode'),
                         ('unready', 'reject-unready-result'), ('signature', 'reject-signature'),
                         ('kind', 'reject-result-kind'), ('width', 'reject-result-width'), ('return', 'reject-return')):
        case = scalar(change=change)
        if CALLS.oracle(case) is not None:
            raise RuntimeError('Independent raw-call oracle accepted '+name)
        add(name, case, False, 'Raw call-role refusal: '+change)
    managed = list(base)
    managed[3] = words(3)+base[3][4:]+words(8, 8)+b'\x80'
    add('reject-managed-type', managed, False, 'Unused managed descriptors are outside the private np0 constructor contract.')
    many = list(base)
    many[3] = words(65)+base[3][4:]+(words(8, 8)+b'\0')*63
    add('reject-type-budget', many, False, 'Private descriptor population exceeds the declared64-type budget.')
    add('reject-entry', base, False, 'Module entry differs from the canonical exported root.', container(base, words(0, 1, 0, 1, 0)))
    add('reject-module-frame', base, False, 'Module frame differs from its exported entry.', container(base, words(0, 1, 0, 0, 1)))
    add('reject-library', base, False, 'Library metadata cannot authorize an executable root.', container(base, words(0, 0, 0)))
    add('reject-data', base, False, 'This private composition profile refuses module data.', container(base, words(0, 1, 4, 0, 0, 0)))
    add('reject-imports', base, False, 'Even an empty imports section is outside this private profile.',
        container(base, order=(1, 2, 3, 5, 6, 7), extra={5: words(0)}))
    add('reject-truncated', base, False, 'The whole image loses its final byte.', container(base)[:-1])
    add('reject-trailer', base, False, 'Unlisted bytes follow the complete container.', container(base)+b'\0')
    for _, _, item in rows:
        item['locals'] = json.loads(json.dumps(item['locals']))
    return rows


# Test deterministic committed goldens without silently rewriting changed artifacts.
def fixtures(write=False):
    rows = corpus()
    manifest = {'purpose': 'private whole-image executable goldens; no production file-call activation',
                'declarations': 'trusted fixture/compiler locals; hashes are identity, not authentication',
                'format': 'existing Dis64 v1.0 ABI1 with no extensions or new IDs',
                'private_policy': {'object_bytes': 4096, 'instructions': 256, 'types': 64, 'functions': 64},
                'cases': [item for _, _, item in rows]}
    encoded = json.dumps(manifest, ensure_ascii=False, indent=2)+'\n'
    if write:
        GOLDENS.mkdir(parents=True, exist_ok=True)
        (GOLDENS/'manifest.json').write_text(encoded)
    elif (GOLDENS/'manifest.json').read_text() != encoded:
        raise RuntimeError('Golden manifest drift; regeneration requires review')
    for name, image, item in rows:
        text = '\n'.join(image.hex()[i:i+64] for i in range(0, len(image)*2, 64))+'\n'
        target = GOLDENS/(name+'.hex')
        if write: target.write_text(text)
        elif target.read_text() != text or hashlib.sha256(bytes.fromhex(target.read_text())).hexdigest() != item['sha256']:
            raise RuntimeError('Golden byte drift: '+name)
    return manifest


# Preserve actual VM/decoder/heap/JIT bodies and replace only the private harness entry point.
def unit():
    body = SUPPORT['unit'](None)
    marker = '/* Keep protected runtime failures and refused raw inputs distinct from trusted width probes. */'
    if body.count(marker) != 1:
        raise RuntimeError('Private result runtime boundary drift')
    return body.split(marker)[0]+'\n\n'+(ROOT/'utils/tests/dis64-call-image.c').read_text()


# Execute exact files in C and RX only after whole-image proof; public loader admission stays closed.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write-goldens', action='store_true')
    args = parser.parse_args()
    manifest = fixtures(args.write_goldens)
    if args.write_goldens:
        print('WROTE', len(manifest['cases']), 'deterministic private call images')
        return
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    env = dict(os.environ)
    compiler = shlex.split(env.get('CC', 'cc'))
    flags = shlex.split(env.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'))
    runner = shlex.split(env.get('NATIVE64_RUNNER', ''))
    arch = env.get('NATIVE64_ARCH', 'amd64')
    with tempfile.TemporaryDirectory(prefix='dis64-call-goldens-', dir=env.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory); env['TMPDIR'] = str(work)
        stream = bytearray(words(len(manifest['cases'])))
        for item in manifest['cases']:
            image = bytes.fromhex((GOLDENS/(item['name']+'.hex')).read_text())
            path = work/(item['name']+'.dis64'); path.write_bytes(image)
            name = str(path).encode()
            stream += words(len(name))+name+words(4096, len(item['locals']), 256, CALLS.LIMIT)
            for symbol, pc, frame, slots in item['locals']:
                stream += words(symbol, pc, frame, len(slots))
                for offset, kind in slots: stream += words(offset, kind)
            stream += words(int(item['private_accept']), item['result_kind'], 0, item['result'],
                            item['runtime_error'], item['chain'], 0, item['second_result'])
        (work/'cases.bin').write_bytes(stream)
        command = compiler+flags+['-DDIS_WORD32=1']
        if arch == 'arm64': command += ['-DDIS64_TEST_ARM64=1', '-DDIS64_TEST_COMPILER="comp-arm64.c"']
        elif arch == 'riscv64': command += ['-DDIS64_TEST_RISCV64=1', '-DDIS64_TEST_COMPILER="comp-riscv64.c"']
        elif arch != 'amd64': raise RuntimeError('Unsupported target '+arch)
        command += ['-I'+str(work)]+['-I'+str(ROOT/p) for p in ('Linux/'+arch+'/include', 'Linux/386/include', 'emu/port', 'include', 'libinterp')]
        source = work/'calls.c'; source.write_text(unit())
        header = (ROOT/'include/dis64-call-image.h').read_text()
        for mode in ('normal', 'entry', 'profile', 'publication', 'map', 'budget'):
            changed = header
            if mode == 'entry':
                changed = changed.replace('image.code = data+code.offset;', 'image.module.frametype = 0;\n\timage.code = data+code.offset;', 1)
            if mode == 'profile': changed = changed.replace('image.module.profile != 1 ||', '0 ||', 1)
            if mode == 'map': changed = changed.replace('if(type.map[j] != 0)', 'if(0 && type.map[j] != 0)', 1)
            if mode == 'budget': changed = changed.replace('types.bytes, Dis64LocalFunctions, objectlimit', 'types.bytes, 65536, objectlimit', 1)
            if mode == 'publication': changed = changed.replace('if(!dis64boundcallcheck(', '*result = image;\n\tif(!dis64boundcallcheck(', 1)
            if mode != 'normal' and changed == header: raise RuntimeError('Image control drift')
            (work/'dis64-call-image.h').write_text(changed)
            binary = work/mode
            subprocess.run(command+[str(source), str(ROOT/'libinterp/dec.c'), '-Wl,--wrap=free', '-lm', '-o', str(binary)], env=env, check=True)
            result = subprocess.run(runner+[str(binary), str(work/'cases.bin')], env=env, capture_output=True, text=True, timeout=90)
            if mode == 'normal':
                if result.returncode: raise RuntimeError(result.stdout+result.stderr)
                print(result.stdout, end='', flush=True)
            elif result.returncode != 10 or not any(text in result.stderr for text in ('whole-image decision mismatch', 'whole-image refusal published output')):
                raise RuntimeError('Whole-image control missed semantic predicate: '+mode+' '+result.stdout+result.stderr)
            else: print('PASS whole-image semantic control:', mode, arch, flush=True)


if __name__ == '__main__':
    main()
