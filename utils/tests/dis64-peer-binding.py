#!/usr/bin/env python3
"""Replay independently decoded Code/Exports and structural bindings without enabling execution."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import runpy
import shutil
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PEER = '0517104b4192ee0297f58a56a419e6f24e66cde0'  # Independently published reader/report checkpoint.
PINS = {
    'disVM/envelope/tests/peer_call_binding.py': '6ed863f72b59ee9470921f4a83168c8eae2ad458bb6691f24b950b434f39791d',
    'disVM/envelope/reports/inferno0112-binding.json': 'eec42df02d54d8c5cbdb1fd2fbdfbaf6b377a82459672898970188194bd7fce0',
}  # Dependencies retain the separate partial-schema gate's unchanged pins.


# Keep explicit stages distinct from parser crashes, file errors or semantic execution authority.
class GateMismatch(RuntimeError):
    """A reviewed integration predicate failed."""


# Refuse only at named integration stages; arbitrary exceptions never satisfy a control.
def require(condition, stage):
    if not condition:
        raise GateMismatch(stage)


# Import only exact reviewed source files, without adding the external checkout to sys.path.
def reader_at(root):
    base = runpy.run_path(str(ROOT/'utils/tests/dis64-peer-schema.py'))
    partial = base['reader_at'](root)
    for name, digest in PINS.items():
        require(hashlib.sha256((root/name).read_bytes()).hexdigest() == digest, 'peer identity')
    sys.modules['peer_call_schema'] = partial
    spec = importlib.util.spec_from_file_location('inferno_reviewed_peer_binding', root/'disVM/envelope/tests/peer_call_binding.py')
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    reader.self_test()
    return reader


# Rehashing a deliberately changed image permits testing schema binding independently of identity.
def rewrite(root, manifest, image):
    item = manifest['cases'][0]
    (root/(item['name']+'.hex')).write_text(image.hex()+'\n')
    item['sha256'] = hashlib.sha256(image).hexdigest()
    (root/'manifest.json').write_text(json.dumps(manifest))


# Inspect exact reports first, then isolate identity, ID/range, unsupported-form and authority mutations.
def verify(root, work, control):
    reader = reader_at(root)
    corpus = ROOT/'utils/tests/goldens/dis64-calls'
    reference = json.loads((root/'disVM/envelope/reports/inferno0112-binding.json').read_text())
    generator = ROOT/'utils/tests/dis64-call-goldens.py'
    require(hashlib.sha256(generator.read_bytes()).hexdigest() == json.loads((root/'disVM/envelope/reports/inferno0111-schema.json').read_text())['peer_checker_sha256'], 'generator identity')
    result = reader.inspect(corpus)
    require(result == {key: reference[key] for key in ('manifest_sha256', 'scope', 'cases')}, 'published report')
    require(len(result['cases']) == 25, 'population')
    canonical = [row for row in result['cases'] if row['status'] == 'structural IDs/ranges valid']
    require(len(canonical) == 23 and sum(row['stage'] == 'outer' and row['status'] == 'malformed' for row in result['cases']) == 2, 'population')
    require(all(row['execution'] == 'unvalidated; no admission granted' and 'owning execution' in row['unvalidated'] for row in result['cases']), 'scope')
    original_inspect = reader.inspect
    if control == 'identity':
        reader.inspect = lambda corpus: result
    elif control == 'binding':
        reader.bind_payloads = lambda data: canonical[0]['decoded']
    elif control == 'unsupported':
        original_code = reader.code_schema
        def missing_unsupported(data):
            """An omitted unsupported classification must not silently become malformed or valid."""
            try:
                return original_code(data)
            except reader.UnsupportedSchema:
                return []
        reader.code_schema = missing_unsupported
    elif control == 'authority':
        def admitted(corpus):
            """Diagnostic-only admission promotion is forbidden by the manifest authority oracle."""
            changed = original_inspect(corpus)
            for row in changed['cases']:
                row['execution'] = 'admitted'
            return changed
        reader.inspect = admitted
    copy = work/'corpus'
    shutil.copytree(corpus, copy)
    original_manifest = json.loads((copy/'manifest.json').read_text())
    name = original_manifest['cases'][0]['name']
    original = bytes.fromhex((copy/(name+'.hex')).read_text())
    (copy/(name+'.hex')).write_text(original.hex()+'00\n')
    try:
        reader.inspect(copy)
    except ValueError as failure:
        require(str(failure).startswith('byte identity:'), 'identity stage')
    else:
        require(False, 'identity')
    for kind, stage in ((6, 'binding'), (2, 'unsupported')):
        image = bytearray(original)
        offset = next(row[2] for row in reader.sections(image) if row[0] == kind)
        if kind == 6:
            struct.pack_into('>I', image, offset+8, 0x7fffffff)
        else:
            image[offset+5] = 6
        rewrite(copy, json.loads(json.dumps(original_manifest)), image)
        row = reader.inspect(copy)['cases'][0]
        expected = 'malformed' if stage == 'binding' else 'unsupported'
        require(row['stage'] == 'schema/binding' and row['status'] == expected, stage)
    manifest = json.loads(json.dumps(original_manifest))
    manifest['private_policy'] = {'instructions': 0, 'types': 0}
    for row in manifest['cases']:
        row.update(locals={'poison': [-1, 999]}, private_accept=True, production_accept=True)
    rewrite(copy, manifest, original)
    changed = reader.inspect(copy)
    require({key: value for key, value in changed.items() if key != 'manifest_sha256'} ==
            {key: value for key, value in result.items() if key != 'manifest_sha256'}, 'authority')
    return {'peer_commit': PEER, 'identities': 25, 'structural_bindings': 23, 'malformed_outer': 2,
            'mutations': ['identity', 'ID/range', 'unsupported form', 'manifest authority'],
            'scope': result['scope'], 'unvalidated': canonical[0]['unvalidated'], 'execution_admitted': False}


# Isolate all mutation writes and preserve the exact corpus plus external checkout bytes.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--peer-root', type=Path, required=True)
    parser.add_argument('--control', choices=('identity', 'binding', 'unsupported', 'authority'))
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='dis64-peer-binding-', dir=ROOT) as directory:
        try:
            report = verify(args.peer_root.resolve(), Path(directory), args.control)
        except GateMismatch as failure:
            if args.control and str(failure) == args.control:
                print('PASS independent binding omission control:', args.control)
                return
            raise
        if args.control:
            raise RuntimeError('integration control missed its semantic predicate')
        if args.report:
            args.report.write_text(json.dumps(report, indent=2)+'\n')
        print('PASS independent Dis64 bindings: 25 identities; 23 structural ID/range passes; '
              '2 malformed outer; four mutations; typed roles/owning execution remain unvalidated')


if __name__ == '__main__':
    main()
