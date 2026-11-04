#!/usr/bin/env python3
"""Replay reviewed independent wire-derived roles/init/result/LEA without granting execution."""
import argparse
from collections import Counter
import contextlib
import io
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PEER = '7119413a641d69bee54adf315ec6e65725a542ba'  # Immutable independent typed-checkpoint source.
MANIFEST = 'fd22e66dbdaf55fbbb8c1751a5e8d34d7b7c18dbababc44260a505ff9b6265b0'  # Exact existing 25-image corpus.
PINS = {
    'peer_call_goldens.py': 'e35b59b62bad30d9daa3120f02fc29830d5540903caf1e641001d7d0e52835f8',
    'peer_call_schema.py': '75a5160af3daf0b137fe584479533ec38ce432b3863c0fc36a85acf3b2853187',
    'peer_call_binding.py': '6ed863f72b59ee9470921f4a83168c8eae2ad458bb6691f24b950b434f39791d',
    'peer_call_roles.py': '6825f5a46f5051e378ee037cebe1ac12e565d5a0558c28a27eff3b44f796c1f8',
    'peer_call_flow.py': '4fa1a7dbfe4b6d15bc0d73c2b16fe4bb43031428561d7a9d5274cc98252e6b00',
    'peer_call_semantics.py': '9b5c78c6e1522720d6800e59acb86c35cd21b46e3fcad414363970ff112ce71e',
    'test_peer_call_semantics.py': '81eaf1e488c191820450c9f024fbdd034fe5089ba2dd1588d6542a9a97f07e85',
}  # Reviewed closure only; no private Inferno locals or acceptance labels become authority.
REPORT_PIN = '436f727728c71a5464a25be360477b07797135b2db9897d16f7de9fb893ca8a8'  # Immutable published per-image report.


class GateMismatch(RuntimeError):
    """Only this explicit integration predicate can satisfy its corresponding control."""


# Keep semantic disagreement distinct from parse, file, resource or child-process errors.
def require(condition, stage):
    if not condition:
        raise GateMismatch(stage)


# Load only reviewed exact modules in dependency order, never an external checkout's sys.path.
def reviewed_modules(root):
    tests = root/'disVM/envelope/tests'
    for name, digest in PINS.items():
        require(hashlib.sha256((tests/name).read_bytes()).hexdigest() == digest, 'peer source identity')
    modules = {}
    for name in PINS:
        module_name = Path(name).stem
        spec = importlib.util.spec_from_file_location(module_name, tests/name)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)
        modules[module_name] = module
    return modules


# Normalize tuples and integer dictionary keys exactly as the published JSON report does.
def canonical(value):
    return json.loads(json.dumps(value))


# A child omission must reach the peer's exact independently expected semantic failure.
def omitted_obligation(root, stage, label):
    code = """import importlib.util, pathlib, sys
spec=importlib.util.spec_from_file_location('reviewed_integration',sys.argv[1])
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
modules=module.reviewed_modules(pathlib.Path(sys.argv[2]))
flow=modules['peer_call_flow']; original=flow.require
flow.require=lambda valid,stage,reason: None if stage==sys.argv[3] else original(valid,stage,reason)
modules['test_peer_call_semantics'].run_suite()
"""
    child = subprocess.run([sys.executable, '-c', code, str(Path(__file__).resolve()), str(root), stage],
                           capture_output=True, text=True, timeout=60)
    require(child.returncode == 10 and child.stderr.strip() == 'FAIL checkpoint oracle: '+label,
            'peer semantic omission '+stage)


# Preserve report distinctions, then independently exercise stale results, identity and sidecar authority.
def verify(root, work, control):
    report_path = root/'disVM/envelope/reports/inferno0120-semantics.json'
    require(hashlib.sha256(report_path.read_bytes()).hexdigest() == REPORT_PIN, 'published report identity')
    reference = json.loads(report_path.read_text())
    require(reference['peer_repository'] == 'bhgv/Inferno-OS-bhgv' and
            reference['peer_revision'] == '82d5275be9404068d716232b9f8102ea1511fc17', 'published corpus revision')
    modules = reviewed_modules(root)
    reader = modules['peer_call_semantics']
    oracle = modules['test_peer_call_semantics']
    corpus = ROOT/'utils/tests/goldens/dis64-calls'
    require(hashlib.sha256((corpus/'manifest.json').read_bytes()).hexdigest() == MANIFEST, 'corpus identity')
    result = canonical(reader.inspect(corpus))
    require(result == {key:reference[key] for key in result}, 'published typed report')
    if control == 'admission':
        result['cases'][0]['owning_execution_supported'] = True
    require(all(not row['owning_execution_supported'] and row['execution'].startswith('closed;') and
                row['required_execution_mode'] == 'explicit immutable dis64-calls; NOT activated by this checker'
                for row in result['cases']), 'admission')
    counts = Counter(row['status'] for row in result['cases'])
    require(counts == {'bounded-checks-passed': 7, 'bounded-refusal': 12, 'unsupported': 4, 'malformed': 2}, 'population')
    by_name = {row['name']: row for row in result['cases']}
    for name in ('reject-managed-type', 'reject-type-budget', 'reject-imports'):
        require(by_name[name]['status'] == 'bounded-checks-passed' and
                'unused descriptor semantic roles' in by_name[name]['unproved'], 'policy distinction')
    require(by_name['reject-signature']['status'] == 'unsupported' and
            by_name['reject-result-kind']['status'] == 'unsupported' and
            'runtime arithmetic safety/termination' in by_name['divide-zero']['unproved'], 'policy distinction')
    require(all('functions' not in row for row in result['cases'] if row['status'] != 'bounded-checks-passed'), 'partial publication')
    original_inspect = reader.inspect
    if control == 'identity':
        reader.inspect = lambda path: result
    copied = work/'corpus'
    shutil.copytree(corpus, copied)
    name = json.loads((copied/'manifest.json').read_text())['cases'][0]['name']
    image_path = copied/(name+'.hex')
    original = bytes.fromhex(image_path.read_text())
    image_path.write_text(original.hex()+'00\n')
    try:
        reader.inspect(copied)
    except ValueError as failure:
        require(str(failure).startswith('byte identity:'), 'identity stage')
    else:
        require(False, 'identity')
    # Rehash a semantic mutation so identity cannot substitute for raw instruction checking.
    if control == 'facts':
        reader.inspect_image = lambda image: by_name[name]
    synthetic, code = oracle.fixture()
    code[0] = oracle.instruction(45, 9, 104, 64)
    synthetic[2] = oracle.pack(len(code))+b''.join(code)
    mutated = reader.inspect_image(oracle.envelope(synthetic))
    require(mutated['status'] == 'bounded-refusal' and mutated['stage'] == 'initialization' and
            'functions' not in mutated, 'facts')
    require(oracle.run_suite() == 39, 'synthetic population')
    if control == 'authority':
        def tainted(path):
            """Deliberately make untrusted sidecar policy influence output, without changing executable bytes."""
            answer = original_inspect(path)
            answer['cases'][0]['sidecar_authority'] = json.loads((path/'manifest.json').read_text()).get('private_policy')
            return answer
        reader.inspect = tainted
        # The oracle imported the callable by value; its explicit dependency must see the same omission.
        oracle.inspect = tainted
    old_temp = tempfile.tempdir
    tempfile.tempdir = str(work)
    try:
        diagnostic = io.StringIO()
        try:
            with contextlib.redirect_stderr(diagnostic):
                identities = oracle.corpus_invariance(corpus)
        except SystemExit as failure:
            if (control == 'authority' and failure.code == 10 and
                    diagnostic.getvalue().strip() == 'FAIL checkpoint oracle: sidecar-authority'):
                raise GateMismatch('authority') from failure
            sys.stderr.write(diagnostic.getvalue())
            raise
    finally:
        tempfile.tempdir = old_temp
    require(identities == 25, 'metamorphic population')
    for stage, label in [('initialization','unready-result'), ('frame-signature','wrong-prepared-type'),
                         ('lea-role','wrong-result-tail'), ('return-initialization','missing-return')]:
        omitted_obligation(root, stage, label)
    return {'peer_commit': PEER, 'manifest_sha256': MANIFEST, 'images': 25,
            'statuses': dict(counts), 'synthetic_cases': 39, 'peer_semantic_controls': 4,
            'integration_mutations': ['identity','cached facts','sidecar authority','admission'],
            'owning_execution_supported': False, 'scope': result['scope'],
            'unproved': result['cases'][0]['unproved']}


# Keep all mutated files in private scratch; the immutable shared corpus and reviewed checkout stay unchanged.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--peer-root', type=Path, required=True)
    parser.add_argument('--control', choices=('identity','facts','authority','admission'))
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='dis64-peer-semantics-', dir=ROOT) as directory:
        try:
            report = verify(args.peer_root.resolve(), Path(directory), args.control)
        except GateMismatch as failure:
            if args.control and str(failure) == args.control:
                print('PASS independent typed-call integration control:', args.control)
                return
            raise
        if args.control:
            raise RuntimeError('integration control missed its exact semantic predicate')
        if args.report:
            args.report.write_text(json.dumps(report,indent=2)+'\n')
        print('PASS independent typed-call checkpoint: 25 identities; 7 bounded passes, 12 refusals, '
              '4 unsupported, 2 malformed; 39 synthetic cases/four peer omissions; owning execution closed')


if __name__ == '__main__':
    main()
