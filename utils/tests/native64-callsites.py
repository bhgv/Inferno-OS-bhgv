#!/usr/bin/env python3
"""Consume opt-in Limbo call-site plans; never treat diagnostic JSON as Dis64 metadata."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def generate(limbo, work, env):
    source = work/'callsites.b'
    calls = []
    for name in ('sprint', 'aprint', 'print', 'fprint'):
        for count in (2, 150):
            kinds = ['string' if i in (1, 70, 149) else 'int' for i in range(count)]
            fmt = '|'.join('%s' if k == 'string' else '%d' for k in kinds)
            values = ', '.join('text' if k == 'string' else 'number' for k in kinds)
            calls.append('sys->'+name+'('+('fd, ' if name == 'fprint' else '')+json.dumps(fmt)+', '+values+');')
    calls += ['sys->sprint("empty");', 'sys->sprint("%d|%bd|%g|%s", number, big number, real number, text);',
              'sys->sprint("%x", array[1] of byte);']
    source.write_text('implement CallSites;\ninclude "sys.m";\nCallSites: module { init: fn(); };\n'
                      'init() { sys := load Sys Sys->PATH; number := -1; text := "ok"; fd := sys->fildes(1);\n'+
                      '\n'.join(calls)+'\n}\n')
    command = [str(limbo), '-I', str(ROOT/'module')]
    ordinary = work/'ordinary.dis'; traced = work/'traced.dis'
    subprocess.run(command+['-o', str(ordinary), str(source)], env=env, check=True, capture_output=True)
    result = subprocess.run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if ordinary.read_bytes() != traced.read_bytes(): raise RuntimeError('Call-site diagnostics changed legacy output')
    records = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in result.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
    repeat = subprocess.run(command+['-Dq', '-o', str(traced), str(source)], env=env, check=True, capture_output=True, text=True)
    if repeat.stderr != result.stderr: raise RuntimeError('Call-site plans are nondeterministic')
    if len(records) != 11 or sum(r['accepted'] for r in records) != 10: raise RuntimeError('Missing accepted/refused source call-site plans: '+str(records))
    if records[-1]['reason'] != 'tail-kind': raise RuntimeError('Opaque reference was admitted without an exact Type witness')
    header = ['/* Generated only from the current compiler\'s typechecked source call plans. */']
    for operation, name in enumerate(('sprint', 'aprint', 'print', 'fprint')):
        for index, count in enumerate((2, 150)):
            record = records[operation*2+index]
            if record['callee'] != 'Sys_'+name or len(record['tail']) != count: raise RuntimeError('Call-site identity mismatch')
            if record['result'] != ('string' if name == 'sprint' else 'array-byte' if name == 'aprint' else 'int') or record['fixed_arguments'] != (2 if name == 'fprint' else 1):
                raise RuntimeError('Original function result/fixed argument identity was lost')
            fixed = 80 if name == 'fprint' else 72
            expected = [0]*((fixed+count*8+63)//64)
            for offset in ([64, 72] if name == 'fprint' else [64]): expected[offset//64] |= 0x80>>((offset//8)%8)
            for i, slot in enumerate(record['tail']):
                kind = 'string' if i in (1, 70, 149) else 'int'
                if slot['kind'] != kind or slot['offset'] != i*8 or slot['bytes'] != 8 or slot['legacy_offset'] != (40 if name == 'fprint' else 36)+i*4:
                    raise RuntimeError('Incorrect actual argument type/slot')
                if kind == 'string': expected[(fixed+i*8)//64] |= 0x80>>(((fixed+i*8)//8)%8)
            if record['fixed_bytes'] != fixed or record['frame_bytes'] != fixed+count*8 or record['map'] != expected:
                raise RuntimeError('Incorrect compiler-produced complete frame geometry')
            label = str(operation)+'_'+str(count)
            header += ['static const uchar callmap_'+label+'[] = {'+','.join(map(str, record['map']))+'};',
                       'static const UWORD callsignature_'+label+' = '+str(record['signature'])+'U;',
                       'static Native64TailSlot callslots_'+label+'[] = {']
            header += ['{'+('Native64TailRef' if slot['kind'] == 'string' else 'Native64TailInt')+','+str(slot['offset'])+',8,'+('&Tstring' if slot['kind'] == 'string' else 'nil')+'},' for slot in record['tail']]
            header += ['};']
    if records[8]['tail'] or records[8]['frame_bytes'] != 72: raise RuntimeError('Empty tail layout failed')
    if [s['kind'] for s in records[9]['tail']] != ['int', 'big', 'real', 'string']: raise RuntimeError('Numeric source kinds lost')
    for flag in ('-64', '-64m'):
        refused = work/('refused'+flag+'.dis64')
        attempt = subprocess.run(command+[flag, '-Dq', '-o', str(refused), str(source)], env=env, capture_output=True, text=True)
        if attempt.returncode == 0 or refused.exists(): raise RuntimeError('Diagnostic plan bypassed Dis64 admission')
    # Verify the runtime's private descriptor-count boundary from actual source,
    # while keeping the legacy producer usable for the refused call as well.
    for count in (4096, 4097):
        boundary = work/('boundary'+str(count)+'.b')
        boundary.write_text('implement Boundary; include "sys.m"; Boundary: module { init: fn(); }; '
                            'init() { sys := load Sys Sys->PATH; number := 1; sys->sprint('+json.dumps('%d'*count)+', '+
                            ', '.join(['number']*count)+'); }')
        ordinary_boundary = work/'boundary.dis'
        traced_boundary = work/'boundary-trace.dis'
        subprocess.run(command+['-o', str(ordinary_boundary), str(boundary)], env=env, check=True, capture_output=True)
        output = subprocess.run(command+['-Dq', '-o', str(traced_boundary), str(boundary)], env=env, check=True, capture_output=True, text=True)
        if ordinary_boundary.read_bytes() != traced_boundary.read_bytes(): raise RuntimeError('Boundary diagnostics changed legacy output')
        plans = [json.loads(line.removeprefix('NATIVE64_CALL ')) for line in output.stderr.splitlines() if line.startswith('NATIVE64_CALL ')]
        if len(plans) != 1 or plans[0]['accepted'] != (count == 4096): raise RuntimeError('Tail-count boundary was not enforced')
        plan = plans[0]
        if count == 4097:
            if plan['reason'] != 'tail-count': raise RuntimeError('Incorrect boundary refusal')
        elif len(plan['tail']) != count or plan['frame_bytes'] != 72+count*8 or plan['map'] != [0, 0x80]+[0]*512:
            raise RuntimeError('Maximum admitted call-site map was truncated')
    (work/'callsites-native64.h').write_text('\n'.join(header)+'\n')
    print('PASS source call-site plans: four Sys operations, 2/150/4096 slots, 4097 refusal, empty/mixed tails, complete maps, opaque-reference refusal, deterministic diagnostics, unchanged legacy bytecode and preserved Dis64 refusal', flush=True)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limbo', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='native64-callsites-', dir=os.environ.get('NATIVE64_TMPDIR', ROOT)) as directory:
        work = Path(directory)
        generate(args.limbo.resolve(), work, dict(os.environ, TMPDIR=str(work)))


if __name__ == '__main__': main()
