#!/usr/bin/env python3
"""Bind real compiler diagnostics to exact images and verify target fault PCs in whole VMs."""
from pathlib import Path
import argparse
import copy
import json
import os
import re
import runpy
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT/'utils/dis64-locations.py'
HELPERS = runpy.run_path(str(ROOT/'utils/tests/native64-scalar-boot.py'))


# Keep failure diagnostics while requiring a real exit status, never an early printed marker alone.
def command(arguments, accepted=True, env=None):
    result = subprocess.run(list(map(str, arguments)), env=env, capture_output=True, timeout=30)
    if (result.returncode == 0) != accepted:
        raise RuntimeError(repr(arguments)+'\n'+result.stderr.decode(errors='replace'))
    return result


# Exercise exact identities, escaped filenames, failed publication and the actual C/JIT error paths.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--emu', type=Path)
    parser.add_argument('--runner', default='')
    args = parser.parse_args()
    root = args.root.resolve()
    compiler = args.limbo.resolve()
    processes = 0
    with tempfile.TemporaryDirectory(prefix='Dis64 source map ', dir=root) as directory:
        work = Path(directory)
        source = work/'quoted"\\ tab\tλ.b'
        image, mapped, ordinary = work/'program.dis64', work/'locations.json', work/'ordinary.dis64'
        body = ('implement Located;\nLocated: module { init: fn(); };\nvalue := 17;\n'
                'init() {\n    for(i := 0; i < 4097; i++) value++;\n    value /= 0;\n}\n')
        source.write_text(body)
        compile_ = [sys.executable, TOOL, 'compile', '--limbo', compiler,
                    '--source', source, '--image', image, '--map', mapped]
        command(compile_)
        original = mapped.read_bytes()
        document = json.loads(original)
        rows = document['instructions']
        if not rows or not any(row['start'] and row['start']['file'] == str(source) for row in rows):
            raise RuntimeError('Compiler did not preserve the escaped source name')
        command(compile_)
        if mapped.read_bytes() != original:
            raise RuntimeError('Nondeterministic source map')
        command([compiler, '-6', '-o', ordinary, source], env=dict(os.environ, LIMBO_DIS64_TRACE='0'))
        if ordinary.read_bytes() != image.read_bytes():
            raise RuntimeError('Tracing changed the executable image')
        lookup = [sys.executable, TOOL, 'lookup', '--image', image, '--map', mapped, '--pc']
        for pc in range(len(rows)):
            if json.loads(command(lookup+[pc]).stdout) != rows[pc]:
                raise RuntimeError('Lookup changed a location')
        for pc in (-1, len(rows)):
            command(lookup+[pc], False)
        for mutation in ('hash', 'opcode', 'pc', 'count', 'negative-line', 'missing-location'):
            bad = copy.deepcopy(document)
            if mutation == 'hash': bad['image_sha256'] = '0'*64
            if mutation == 'opcode': bad['instructions'][0]['op'] ^= 1
            if mutation == 'pc': bad['instructions'][0]['pc'] = 1
            if mutation == 'count': bad['instructions'].pop()
            if mutation == 'negative-line': bad['instructions'][0]['start']['line'] = -1
            if mutation == 'missing-location': del bad['instructions'][0]['stop']
            mapped.write_text(json.dumps(bad))
            command(lookup+[0], False)
        mapped.write_bytes(original)
        if args.emu:
            cases = (('value /= 0;', 'zero divide'), ('value <<= 32;', 'invalid argument'),
                     ('wide /= big 0;', 'zero divide'), ('wide <<= 64;', 'invalid argument'),
                     ('if(value != 4114) value /= 0;', ''))
            for statement, error in cases:
                text = body.replace('value := 17;', 'value := 17; wide := big 17;').replace('value /= 0;', statement)
                source.write_text(text)
                command(compile_)
                for jit in (0, 1):
                    output = HELPERS['execute'](shlex.split(args.runner)+[str(args.emu.resolve()), '-v',
                        '-r', str(root), '-c', str(jit), '-d', '/'+str(image.relative_to(root))])
                    marker = 'native64 scalar module Located: jit='+str(jit)+', exit="'+error+'"'
                    matches = re.findall(r'native64 scalar fault: dis-pc=(\d+)', output)
                    if (marker not in output or len(matches) != bool(error) or ('Broken:' in output) != bool(error) or
                            'native64 scalar scheduler empty after program retirement' not in output or
                            'panic:' in output or 'faults:' in output):
                        raise RuntimeError('Missing target fault/retirement markers:\n'+output)
                    if error:
                        row = json.loads(command(lookup+[int(matches[0])]).stdout)
                        if row['start']['line'] != 6 or row['start']['file'] != str(source):
                            raise RuntimeError('Target fault PC resolves to the wrong source statement: '+repr(row))
                    processes += 1
            mapped.write_bytes(original)
        # An unsupported source must preserve the previously reviewed map.
        source.write_text('implement Bad;\nBad: module {init: fn();};\ns := "managed";\ninit() {}\n')
        command(compile_, False)
        if mapped.read_bytes() != original:
            raise RuntimeError('Failed compiler overwrote the old sidecar')
        # Reject both lexical aliases and hard links before invoking the compiler.
        source.write_text(body)
        image.unlink(missing_ok=True);image.hardlink_to(source)
        command(compile_, False)
        if source.read_text() != body:
            raise RuntimeError('Hard-link destination damaged source input')
    print('PASS Dis64 locations:', len(rows), 'exact PC rows; deterministic/escaped diagnostics, active mismatches,', processes, 'whole C/JIT processes')


if __name__ == '__main__':
    main()
