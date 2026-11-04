#!/usr/bin/env python3
"""Cross-check independent Python module-wide storage against raw verdicts and actual C."""
import argparse
import json
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
RAW = runpy.run_path(str(ROOT/'utils/tests/dis64-storage-image.py'))


# Extend explicit raw verdicts with namespace/content, whole-map and scalar boundary cases.
def cases():
    rows = list(RAW['cases']())
    image, sections, words = RAW['image'], RAW['sections'], RAW['words']
    descriptor, binding, signature = RAW['descriptor'], RAW['binding'], RAW['signature']
    extra = [
        ('identical-import-repeated', 1, sections(imports=[binding(), binding(module=4, symbol=2)])),
        ('Bool-parameter-structural', 1, sections(imports=[binding(sig=signature(4, (4, 1, 2, 3)))])),
        ('void-parameter-refused', 0, sections(imports=[binding(sig=signature(0, (0,)))])),
        ('unknown-result-refused', 0, sections(exports=[binding(export=True, sig=signature(5))])),
        ('unused-ref-padding', 0, sections(types=[descriptor(80, 8), descriptor(16, 8, b'\x80'), descriptor(8, 8, b'\x01')])),
        ('unused-empty-type', 1, sections(types=[descriptor(80, 8), descriptor(16, 8, b'\x80'), descriptor(0, 1)])),
        ('unused-empty-align', 0, sections(types=[descriptor(80, 8), descriptor(16, 8, b'\x80'), descriptor(0, 8)])),
        ('zero-DATA-count-trailer', 0, sections(data=words(0, 123))),
        ('zero-import-count-trailer', 0, sections(imports=[])+[(50001, b'')]),
    ]
    # Replace the empty import payload with a trailing word, preserving the enclosing extent.
    name, expected, parts = extra[-1]
    extra[-1] = name, expected, [(kind, words(0, 123) if kind == 5 else body) for kind, body in parts]
    for name, expected, parts in extra:
        rows.append((name, expected, image(parts), RAW['POLICY']))
    # All 32 Rune bits are structural values, including negative/surrogate/out-of-Unicode values.
    for rune in (0, 0xd800, 0x10ffff, 0x110000, 0x80000000, 0xffffffff):
        parts = sections(name=0, imports=[binding(module=0, symbol=0)])
        parts = [(kind, words(1, 1, rune) if kind == 7 else body) for kind, body in parts]
        rows.append(('Rune32-'+str(rune), 1, image(parts), RAW['POLICY']))
    # Structural framing deliberately includes INOP/IECLR; execution policy is a separate gate.
    for opcode in range(256):
        rows.append(('opcode-byte-'+str(opcode), int(opcode < 175),
                     image(sections(code=words(1)+bytes((opcode, 27)))), RAW['POLICY']))
    for mode in range(256):
        source, destination, middle = (mode >> 3) & 7, mode & 7, mode >> 6
        for width, encoded in ((1, b'\x00'), (2, b'\x80\x01'), (4, b'\xc0\x00\x00\x01')):
            count = int(middle != 0)+int(source != 3)+int(destination != 3)
            code = words(1)+bytes((15, mode))+encoded*count
            rows.append(('mode-byte-'+str(mode)+'-width-'+str(width), int(source <= 3 and destination <= 3),
                         image(sections(code=code)), RAW['POLICY']))
    return rows


# Remove one entire proof stage only inside this test's loaded Python namespace.
def checker(control):
    source = (ROOT/'utils/dis64_storage.py').read_text()
    controls = {
        'types': ('if size > object_limit or align not in (1, 2, 4, 8) or size % align or (size == 0 and align != 1):', 'if False:', 'bad-unused-type'),
        'data': ('if any(offset < end and begin < offset+width for begin, end in spans):', 'if False:', 'data-overlap'),
        'imports': ('imports = bindings(sections[5], False, strings, types, instructions, binding_limit, parameter_limit) if 5 in sections else None', 'imports = None', 'import-conflict'),
        'exports': ('exports = bindings(sections[6], True, strings, types, instructions, binding_limit, parameter_limit) if 6 in sections else None', 'exports = None', 'export-duplicate'),
        'code': ('reader.finish()\n    return count\n\n\n# Check all scalar DATA', 'return count\n\n\n# Check all scalar DATA', 'code-trailer'),
        'module': ('if descriptor[0] != size:', 'if False:', 'data-size-mismatch'),
    }
    witness = None
    if control:
        old, new, witness = controls[control]
        if source.count(old) != 1:
            raise RuntimeError('independent checker control drift: '+control)
        source = source.replace(old, new, 1)
    namespace = {'__name__': 'independent_storage_test'}
    exec(compile(source, str(ROOT/'utils/dis64_storage.py'), 'exec'), namespace)
    return namespace['inspect_storage'], witness


# Compare explicit verdicts without converting programming exceptions into malformed-image results.
def verify(rows, inspect, witness):
    checks = 0
    for name, expected, image, limits in rows:
        try:
            result = inspect(image, limits)
            actual = 1
        except ValueError:
            actual = 0
        if actual != expected:
            if name != witness or expected != 0 or actual != 1:
                raise RuntimeError('Python storage mismatch: '+name)
            return checks, True
        if actual:
            if result['owning_execution_supported'] is not False:
                raise RuntimeError('storage checker granted execution')
            checks += 1
        checks += 1
    return checks, False


# Run exactly the same raw corpus through the actual allocation-free C joined reader.
def c_verdicts(rows, work):
    word = RAW['words']
    stream = word(len(rows))
    for name, expected, image, limits in rows:
        encoded = name.encode('ascii')
        stream += word(len(encoded))+encoded+word(expected, *limits, len(image))+image
    (work/'fixtures').write_bytes(stream)
    flags = shlex.split(os.environ.get('NATIVE64_CFLAGS', '-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all'))
    binary = work/'test'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc'))+flags+['-I'+str(ROOT/'include'), str(ROOT/'utils/tests/dis64-storage-image.c'), '-o', str(binary)], check=True)
    result = subprocess.run(shlex.split(os.environ.get('NATIVE64_RUNNER', ''))+[str(binary), str(work/'fixtures')], capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise RuntimeError('actual C storage verification failed: '+result.stdout+result.stderr)
    print(result.stdout, end='')


# Verify diagnostic JSON and the important absent-table versus present-empty-table distinction.
def cli_checks(work):
    count = 0
    for present in (False, True):
        image = RAW['image'](RAW['sections'](data=RAW['words'](0) if present else None,
                                             imports=[] if present else None,
                                             exports=[] if present else None))
        path = work/'input.dis64'
        path.write_bytes(image)
        result = subprocess.run([os.sys.executable, str(ROOT/'utils/dis64_storage.py'), str(path)], capture_output=True, text=True, timeout=10)
        if result.returncode:
            raise RuntimeError('independent CLI refused valid storage: '+result.stderr)
        report = json.loads(result.stdout)
        for key in ('initializers', 'imports', 'exports'):
            if report[key] != (0 if present else None):
                raise RuntimeError('CLI erased optional-table presence: '+key)
            count += 1
        if report['owning_execution_supported'] is not False or report['storage_checked'] is not True:
            raise RuntimeError('CLI execution boundary')
        count += 1
    path.write_bytes(b'Dis64')
    result = subprocess.run([os.sys.executable, str(ROOT/'utils/dis64_storage.py'), str(path)], capture_output=True, text=True, timeout=10)
    report = json.loads(result.stdout)
    if result.returncode != 1 or report['storage_checked'] is not False or report['owning_execution_supported'] is not False:
        raise RuntimeError('CLI malformed storage boundary')
    return count+1


# Controls require a named semantic disagreement; C failures and Python exceptions never count as success.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control', choices=('types', 'data', 'imports', 'exports', 'code', 'module'))
    args = parser.parse_args()
    rows = cases()
    inspect, witness = checker(args.control)
    checks, detected = verify(rows, inspect, witness)
    if args.control:
        if not detected:
            raise RuntimeError('Python storage omission escaped: '+args.control)
        print('PASS independent Python storage control:', args.control, witness)
        return
    if detected:
        raise RuntimeError('unexpected semantic disagreement')
    with tempfile.TemporaryDirectory(prefix='python-storage-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        c_verdicts(rows, work)
        checks += cli_checks(work)
    print('PASS independent Python module-wide storage:', len(rows), 'raw rows,', checks, 'Python predicates; actual C agrees, no execution admission')


if __name__ == '__main__':
    main()
