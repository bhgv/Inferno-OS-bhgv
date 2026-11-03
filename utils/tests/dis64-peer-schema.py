#!/usr/bin/env python3
"""Replay the reviewed FlashVM partial schema reader on Inferno's immutable call corpus."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PEER = '92f2dafdb21e7554ea91b3de679f56505762bf5b'  # Reviewed independent reader/report commit.
PINS = {
    'disVM/envelope/tests/peer_call_schema.py': '75a5160af3daf0b137fe584479533ec38ce432b3863c0fc36a85acf3b2853187',
    'disVM/envelope/tests/peer_call_goldens.py': 'e35b59b62bad30d9daa3120f02fc29830d5540903caf1e641001d7d0e52835f8',
    'disVM/envelope/reports/inferno0111-schema.json': 'fab146da93ac47dd790e792a83a45abd880cb1fe0d436545fcae0e3d3fa7a1e9',
}  # Byte identities, not executable or input provenance authority.


class GateMismatch(RuntimeError):
    """A semantic integration predicate failed before any execution admission."""


# Make refusal stages explicit so unrelated exceptions cannot pass diagnostic controls.
def require(condition, stage):
    if not condition:
        raise GateMismatch(stage)


# Execute only the exact reviewed parser and its one sibling import from the selected checkout.
def reader_at(root):
    for name, digest in PINS.items():
        require(hashlib.sha256((root/name).read_bytes()).hexdigest() == digest, 'peer source identity')
    tests = root/'disVM/envelope/tests'
    dependency = importlib.util.spec_from_file_location('peer_call_goldens', tests/'peer_call_goldens.py')
    module = importlib.util.module_from_spec(dependency)
    sys.modules['peer_call_goldens'] = module
    dependency.loader.exec_module(module)
    spec = importlib.util.spec_from_file_location('inferno_reviewed_peer_schema', tests/'peer_call_schema.py')
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    reader.self_test()
    return reader


# Ignore only the manifest identity when comparing schema output after an authority-only mutation.
def payload(report):
    return {key: value for key, value in report.items() if key != 'manifest_sha256'}


# Preserve the parser/report's stated partial scope and refuse promotion to code/call acceptance.
def verify(root, control, work):
    corpus = ROOT/'utils/tests/goldens/dis64-calls'
    reference = json.loads((root/'disVM/envelope/reports/inferno0111-schema.json').read_text())
    generator = ROOT/'utils/tests/dis64-call-goldens.py'
    require(hashlib.sha256(generator.read_bytes()).hexdigest() == reference['peer_checker_sha256'], 'generator identity')
    reader = reader_at(root)
    result = reader.inspect(corpus)
    require(result == {key: reference[key] for key in ('manifest_sha256', 'scope', 'cases')}, 'published partial schema')
    require(len(result['cases']) == 25, 'corpus population')
    require(sum(row['outer'] == 'canonical' for row in result['cases']) == 23, 'outer population')
    require(sum(row['outer'] == 'malformed' for row in result['cases']) == 2, 'outer population')
    canonical = [row for row in result['cases'] if row['outer'] == 'canonical']
    require(all(row['partial_schema'] == 'metadata/types/strings valid' for row in canonical), 'partial section population')
    require(all(row['execution'] == 'not evaluated; no admission granted' and
                'code/exports schema' in row['unvalidated'] for row in canonical), 'scope boundary')
    original_inspect = reader.inspect
    if control == 'identity':
        reader.inspect = lambda corpus: result
    if control == 'schema':
        reader.metadata = lambda data: canonical[0]['metadata']
    if control == 'authority':
        def admitted(corpus):
            """Diagnostic-only promotion must be detected by the authority mutation oracle."""
            changed = original_inspect(corpus)
            for row in changed['cases']:
                row['execution'] = 'admitted'
            return changed
        reader.inspect = admitted
    copy = work/'corpus'
    shutil.copytree(corpus, copy)
    manifest = json.loads((copy/'manifest.json').read_text())
    first = manifest['cases'][0]
    image_path = copy/(first['name']+'.hex')
    original = image_path.read_text()
    image_path.write_text(original+'00\n')
    try:
        reader.inspect(copy)
    except ValueError as failure:
        require(str(failure).startswith('byte identity:'), 'identity refusal stage')
    else:
        require(False, 'identity')
    image_path.write_text(original)
    image = bytearray(bytes.fromhex(original))
    metadata = next(row for row in reader.sections(image) if row[0] == 1)
    struct.pack_into('>I', image, metadata[2]+4, 2)
    image_path.write_text(image.hex()+'\n')
    first['sha256'] = hashlib.sha256(image).hexdigest()
    (copy/'manifest.json').write_text(json.dumps(manifest))
    malformed = reader.inspect(copy)['cases'][0]
    require(malformed['outer'] == 'canonical' and malformed['partial_schema'] == 'malformed' and
            malformed['reason'] == 'module profile', 'schema')
    image_path.write_text(original)
    manifest = json.loads((corpus/'manifest.json').read_text())
    manifest['private_policy'] = {'instructions': 0, 'types': 0, 'object_bytes': 0}
    for item in manifest['cases']:
        item['private_accept'] = True
        item['production_accept'] = True
        item['locals'] = {'untrusted': 'not type authority'}
    (copy/'manifest.json').write_text(json.dumps(manifest))
    require(payload(reader.inspect(copy)) == payload(result), 'authority')
    return {'peer_commit': PEER, 'scope': result['scope'], 'identities': 25,
            'canonical_outer': 23, 'malformed_outer': 2, 'partial_schemas': 23,
            'mutation_checks': ['byte identity', 'malformed metadata', 'manifest authority'],
            'unvalidated': canonical[0]['unvalidated'], 'execution_admitted': False}


# All writes are isolated mutations/report output; the committed corpus and peer checkout stay unchanged.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--peer-root', type=Path, required=True)
    parser.add_argument('--control', choices=('identity', 'schema', 'authority'))
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='dis64-peer-schema-', dir=ROOT) as directory:
        try:
            report = verify(args.peer_root.resolve(), args.control, Path(directory))
        except GateMismatch as failure:
            if not args.control or str(failure) != args.control:
                raise
            print('PASS independent peer integration control:', args.control)
            return
        if args.control:
            raise RuntimeError('integration control failed to reach its semantic predicate')
        if args.report:
            args.report.write_text(json.dumps(report, indent=2)+'\n')
        print('PASS independent Dis64 peer: 25 identities; 23 canonical/2 malformed outer; '
              '23 metadata/types/Rune32 schemas; 3 mutation checks; code/exports/calls unvalidated')


if __name__ == '__main__':
    main()
